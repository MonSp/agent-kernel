import * as net from 'net';
import {
  AgentProfile,
  CreateAgentParams,
  Decision,
  JournalEvent,
  KernelRequest,
  KernelResponse,
  MailboxMessage,
  PendingRequest,
  SimulationResult,
  SkillNode,
  StreamEvent,
  TickResult,
} from './types';

const DEFAULT_SOCKET_PATH = '/tmp/agent-kernel.sock';

/**
 * TypeScript client for the C++ agent-kernel IPC daemon.
 *
 * Communicates over a Unix domain socket using newline-delimited JSON
 * (matching the C++ UnixSocketServer protocol).
 */
export class AgentKernelClient {
  private socketPath: string;
  private socket: net.Socket | null = null;
  private buffer: string = '';
  private nextId: number = 1;
  private pending: Map<string | number, PendingRequest> = new Map();
  private connected: boolean = false;

  constructor(socketPath: string = DEFAULT_SOCKET_PATH) {
    this.socketPath = socketPath;
  }

  /**
   * Connect to the kernel daemon's Unix socket.
   * Resolves once the connection is established.
   */
  async connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      this.socket = net.createConnection(this.socketPath);

      this.socket.on('connect', () => {
        this.connected = true;
        resolve();
      });

      this.socket.on('data', (data: Buffer) => {
        this.onData(data);
      });

      this.socket.on('error', (err: Error) => {
        if (!this.connected) {
          reject(err);
        } else {
          // Forward errors to all pending requests
          for (const pending of this.pending.values()) {
            pending.reject(err);
          }
          this.pending.clear();
          this.connected = false;
        }
      });

      this.socket.on('close', () => {
        this.connected = false;
        // Reject any remaining pending requests
        const closeErr = new Error('Socket closed');
        for (const pending of this.pending.values()) {
          pending.reject(closeErr);
        }
        this.pending.clear();
      });
    });
  }

  /** Disconnect from the kernel daemon. */
  disconnect(): void {
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
      this.connected = false;
    }
  }

  /** Whether the client is currently connected. */
  isConnected(): boolean {
    return this.connected;
  }

  // ─── Agent CRUD ────────────────────────────────────────────────

  /**
   * Create a new agent in the kernel.
   */
  async createAgent(params: CreateAgentParams): Promise<AgentProfile> {
    const resp = await this.request<AgentProfile>('createAgent', params as unknown as Record<string, unknown>);
    return resp;
  }

  /**
   * Get an agent by its entity ID.
   */
  async getAgent(entityId: number): Promise<AgentProfile> {
    const resp = await this.request<AgentProfile>('getAgent', { entityId });
    return resp;
  }

  /**
   * Update fields on an existing agent.
   */
  async updateAgent(
    entityId: number,
    updates: Partial<{ name: string; department: string; companyRole: string; teamId: string; role: string }>,
  ): Promise<AgentProfile> {
    const resp = await this.request<AgentProfile>('updateAgent', { entityId, ...updates });
    return resp;
  }

  /**
   * Delete an agent by entity ID.
   */
  async deleteAgent(entityId: number): Promise<void> {
    await this.request<{ deleted: boolean }>('deleteAgent', { entityId });
  }

  /**
   * List all agents currently in the kernel.
   */
  async listAgents(): Promise<AgentProfile[]> {
    const resp = await this.request<AgentProfile[]>('listAgents');
    return resp;
  }

  // ─── Skills ────────────────────────────────────────────────────

  /**
   * Add XP to a specific skill on an agent.
   */
  async addSkillXp(entityId: number, skillId: string, xp: number): Promise<SkillNode> {
    const resp = await this.request<SkillNode>('addSkillXp', { entityId, skillId, xp });
    return resp;
  }

  /**
   * Get all skills for an agent.
   */
  async getSkills(entityId: number): Promise<Record<string, SkillNode>> {
    const resp = await this.request<Record<string, SkillNode>>('getSkills', { entityId });
    return resp;
  }

  // ─── Sync ──────────────────────────────────────────────────────

  /**
   * Sync full state from the kernel (all agents).
   */
  async syncState(): Promise<{ agents: AgentProfile[]; count: number }> {
    const resp = await this.request<{ agents: AgentProfile[]; count: number }>('syncState');
    return resp;
  }

  // ─── L4: LLM Decision ─────────────────────────────────────────

  /**
   * Ask the kernel's LLM to make a decision for an agent on a task.
   */
  async agentDecide(entityId: number, task: string): Promise<Decision> {
    const resp = await this.request<Decision>('agentDecide', { entityId, task });
    return resp;
  }

  // ─── L5: Agent Tick & Simulation ─────────────────────────────

  /**
   * Run a single agent tick: perceive → LLM decide → execute → apply effects.
   */
  async agentTick(entityId: number, task: string): Promise<TickResult> {
    const resp = await this.request<TickResult>('agentTick', { entityId, task });
    return resp;
  }

  /**
   * Run a multi-agent batch simulation for N ticks.
   */
  async runSimulation(
    entityIds: number[],
    ticks: number = 1,
    tasks?: string[],
  ): Promise<SimulationResult> {
    const params: Record<string, unknown> = { entityIds, ticks };
    if (tasks) params.tasks = tasks;
    const resp = await this.request<SimulationResult>('runSimulation', params);
    return resp;
  }

  // ─── L6: EventJournal ────────────────────────────────────────

  /**
   * Append an event to the kernel's EventJournal.
   */
  async appendEvent(entityId: number, eventType: string, payload: string): Promise<number> {
    const resp = await this.request<{ eventId: number }>('appendEvent', { entityId, eventType, payload });
    return resp.eventId;
  }

  /**
   * Query events from the EventJournal.
   */
  async getEvents(options?: { entityId?: number; sinceId?: number }): Promise<JournalEvent[]> {
    const params: Record<string, unknown> = {};
    if (options?.entityId !== undefined) params.entityId = options.entityId;
    if (options?.sinceId !== undefined) params.sinceId = options.sinceId;
    const resp = await this.request<{ events: JournalEvent[] }>('getEvents', params);
    return resp.events;
  }

  // ─── L6: AgentMailbox ───────────────────────────────────────

  /**
   * Send a message from one entity to another via the kernel mailbox.
   */
  async sendMessage(from: number, to: number, payload: string): Promise<number> {
    const resp = await this.request<{ messageId: number }>('sendMessage', { from, to, payload });
    return resp.messageId;
  }

  /**
   * Receive undelivered messages for an entity.
   */
  async getMessages(entityId: number, limit?: number): Promise<{ messages: MailboxMessage[]; pending: number }> {
    const params: Record<string, unknown> = { entityId };
    if (limit !== undefined) params.limit = limit;
    const resp = await this.request<{ messages: MailboxMessage[]; pending: number }>('getMessages', params);
    return resp;
  }

  // ─── Internal ──────────────────────────────────────────────────

  /**
   * Send a request and wait for the response.
   *
   * The current C++ server processes requests sequentially per client
   * and doesn't echo request IDs, so we use a simple FIFO queue.
   * The `id` field is included for future protocol extensions.
   */
  private async request<T>(method: string, params?: Record<string, unknown>): Promise<T> {
    if (!this.socket || !this.connected) {
      throw new Error('Not connected. Call connect() first.');
    }

    const id = this.nextId++;
    const req: KernelRequest = { method, params, id };
    const line = JSON.stringify(req) + '\n';

    return new Promise<T>((resolve, reject) => {
      // Register pending request (keyed by id for future async matching)
      this.pending.set(id, {
        resolve: resolve as (v: unknown) => void,
        reject,
      });

      this.socket!.write(line, (err) => {
        if (err) {
          this.pending.delete(id);
          reject(err);
        }
      });
    });
  }

  /**
   * Handle incoming data from the socket.
   * Splits on newlines and resolves pending requests in FIFO order.
   */
  private onData(data: Buffer): void {
    this.buffer += data.toString('utf-8');

    let newlineIdx: number;
    while ((newlineIdx = this.buffer.indexOf('\n')) !== -1) {
      const line = this.buffer.slice(0, newlineIdx).replace(/\r$/, '');
      this.buffer = this.buffer.slice(newlineIdx + 1);

      if (!line) continue;

      // Resolve the oldest pending request (FIFO)
      const firstKey = this.pending.keys().next().value;
      if (firstKey !== undefined) {
        const pending = this.pending.get(firstKey)!;
        this.pending.delete(firstKey);

        try {
          const resp: KernelResponse = JSON.parse(line);
          if (resp.ok) {
            pending.resolve(resp.data);
          } else {
            pending.reject(new Error(resp.error || 'Unknown kernel error'));
          }
        } catch (parseErr) {
          pending.reject(
            new Error(`Failed to parse response: ${(parseErr as Error).message}`),
          );
        }
      }
    }
  }
}
