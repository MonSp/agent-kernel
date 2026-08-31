import { describe, it, expect, beforeAll, afterAll, beforeEach } from 'vitest';
import { ChildProcess, spawn } from 'child_process';
import * as path from 'path';
import * as fs from 'fs';
import * as net from 'net';
import { AgentKernelClient } from '../src/AgentKernelClient';
import { loadMappings, getGameAbility, getCompanySkills, resetCache } from '../src/SkillMappingLoader';

const SOCKET_PATH = '/tmp/agent-kernel-ts-test.sock';
const DAEMON_PATH = path.resolve(__dirname, '..', '..', 'build', 'agent-kernel-daemon');
const MAPPING_PATH = path.resolve(__dirname, '..', '..', 'config', 'skill-mapping.json');

/** Wait for the socket file to appear (server is ready). */
function waitForSocket(socketPath: string, timeoutMs = 5000): Promise<void> {
  return new Promise((resolve, reject) => {
    const deadline = Date.now() + timeoutMs;
    const check = () => {
      if (fs.existsSync(socketPath)) {
        // Try connecting to verify it's actually accepting
        const sock = net.createConnection(socketPath);
        sock.on('connect', () => {
          sock.destroy();
          resolve();
        });
        sock.on('error', () => {
          if (Date.now() > deadline) {
            reject(new Error('Timed out waiting for socket'));
          } else {
            setTimeout(check, 100);
          }
        });
      } else if (Date.now() > deadline) {
        reject(new Error('Timed out waiting for socket file'));
      } else {
        setTimeout(check, 100);
      }
    };
    check();
  });
}

describe('Skill Mapping', () => {
  beforeEach(() => {
    resetCache();
  });

  it('should load all 42 skill mappings', () => {
    const mappings = loadMappings(MAPPING_PATH);
    const keys = Object.keys(mappings);
    expect(keys.length).toBe(42);
  });

  it('should have correct category breakdown', () => {
    const mappings = loadMappings(MAPPING_PATH);
    const categories: Record<string, number> = {};
    for (const m of Object.values(mappings)) {
      categories[m.category] = (categories[m.category] || 0) + 1;
    }
    expect(categories['Engineering']).toBe(14);
    expect(categories['Design']).toBe(10);
    expect(categories['Content']).toBe(7);
    expect(categories['Data']).toBe(6);
    expect(categories['Management']).toBe(5);
  });

  it('should map backend_dev to 阵法造诣', () => {
    const ability = getGameAbility('backend_dev', MAPPING_PATH);
    expect(ability).toBe('阵法造诣');
  });

  it('should return undefined for unknown skill', () => {
    const ability = getGameAbility('nonexistent_skill', MAPPING_PATH);
    expect(ability).toBeUndefined();
  });

  it('should list all company skills', () => {
    const skills = getCompanySkills(MAPPING_PATH);
    expect(skills).toContain('backend_dev');
    expect(skills).toContain('data_analysis');
    expect(skills).toContain('competitive_analysis');
    expect(skills.length).toBe(42);
  });

  it('should have all required fields on each mapping', () => {
    const mappings = loadMappings(MAPPING_PATH);
    for (const [skillId, mapping] of Object.entries(mappings)) {
      expect(mapping.gameAbility, `${skillId} gameAbility`).toBeTruthy();
      expect(mapping.category, `${skillId} category`).toBeTruthy();
      expect(mapping.description, `${skillId} description`).toBeTruthy();
    }
  });
});

describe('AgentKernelClient IPC', () => {
  let daemon: ChildProcess | null = null;
  let client: AgentKernelClient;

  beforeAll(async () => {
    // Clean up stale socket
    try { fs.unlinkSync(SOCKET_PATH); } catch { /* ignore */ }

    // Verify daemon binary exists
    if (!fs.existsSync(DAEMON_PATH)) {
      throw new Error(`Daemon binary not found at ${DAEMON_PATH}. Build it first.`);
    }

    // Spawn the kernel daemon
    daemon = spawn(DAEMON_PATH, ['--socket', SOCKET_PATH], {
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    daemon.on('error', (err) => {
      console.error('Failed to start daemon:', err);
    });

    // Wait for socket to become available
    await waitForSocket(SOCKET_PATH);

    // Create and connect client
    client = new AgentKernelClient(SOCKET_PATH);
    await client.connect();
  }, 15000);

  afterAll(() => {
    if (client) {
      client.disconnect();
    }
    if (daemon) {
      daemon.kill('SIGTERM');
      daemon = null;
    }
    // Clean up socket
    try { fs.unlinkSync(SOCKET_PATH); } catch { /* ignore */ }
  });

  it('should connect to the daemon', () => {
    expect(client.isConnected()).toBe(true);
  });

  it('should create an agent', async () => {
    const agent = await client.createAgent({
      id: 'ts-agent-001',
      name: 'TestAgent',
      department: 'Engineering',
      companyRole: 'Developer',
      teamId: 'team-ts',
      role: 'Worker',
    });

    expect(agent).toBeDefined();
    expect(agent.identity.id).toBe('ts-agent-001');
    expect(agent.identity.name).toBe('TestAgent');
    expect(agent.identity.department).toBe('Engineering');
    expect(agent.entityId).toBeDefined();
  });

  it('should get an agent by entityId', async () => {
    const agent = await client.getAgent(0);
    expect(agent.identity.id).toBe('ts-agent-001');
    expect(agent.identity.name).toBe('TestAgent');
  });

  it('should list agents', async () => {
    const agents = await client.listAgents();
    expect(Array.isArray(agents)).toBe(true);
    expect(agents.length).toBeGreaterThanOrEqual(1);

    const found = agents.find((a) => a.identity.id === 'ts-agent-001');
    expect(found).toBeDefined();
  });

  it('should update an agent', async () => {
    const updated = await client.updateAgent(0, {
      name: 'UpdatedAgent',
      role: 'Lead',
    });
    expect(updated.identity.name).toBe('UpdatedAgent');
    expect(updated.identity.role).toBe('Lead');
  });

  it('should get skills for an agent', async () => {
    const skills = await client.getSkills(0);
    expect(typeof skills).toBe('object');
    // Fresh agent has empty skill tree
  });

  it('should create a second agent and delete it', async () => {
    const agent2 = await client.createAgent({
      id: 'ts-agent-002',
      name: 'DeleteMe',
      department: 'Design',
      companyRole: 'Designer',
    });
    const entityId = agent2.entityId;

    await client.deleteAgent(entityId);

    // Verify it's gone
    try {
      await client.getAgent(entityId);
      expect.fail('Should have thrown');
    } catch (err) {
      expect((err as Error).message).toContain('entity not found');
    }
  });

  it('should sync full state', async () => {
    const state = await client.syncState();
    expect(state).toBeDefined();
    expect(state.agents).toBeDefined();
    expect(Array.isArray(state.agents)).toBe(true);
    expect(state.count).toBe(state.agents.length);
  });

  it('should handle errors gracefully', async () => {
    try {
      // Use a non-existent entity ID
      await client.getAgent(99999);
      expect.fail('Should have thrown');
    } catch (err) {
      expect((err as Error).message).toContain('entity not found');
    }
  });
});
