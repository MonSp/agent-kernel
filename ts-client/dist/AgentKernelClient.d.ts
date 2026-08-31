import { AgentProfile, CreateAgentParams, SkillNode } from './types';
/**
 * TypeScript client for the C++ agent-kernel IPC daemon.
 *
 * Communicates over a Unix domain socket using newline-delimited JSON
 * (matching the C++ UnixSocketServer protocol).
 */
export declare class AgentKernelClient {
    private socketPath;
    private socket;
    private buffer;
    private nextId;
    private pending;
    private connected;
    constructor(socketPath?: string);
    /**
     * Connect to the kernel daemon's Unix socket.
     * Resolves once the connection is established.
     */
    connect(): Promise<void>;
    /** Disconnect from the kernel daemon. */
    disconnect(): void;
    /** Whether the client is currently connected. */
    isConnected(): boolean;
    /**
     * Create a new agent in the kernel.
     */
    createAgent(params: CreateAgentParams): Promise<AgentProfile>;
    /**
     * Get an agent by its entity ID.
     */
    getAgent(entityId: number): Promise<AgentProfile>;
    /**
     * Update fields on an existing agent.
     */
    updateAgent(entityId: number, updates: Partial<{
        name: string;
        department: string;
        companyRole: string;
        teamId: string;
        role: string;
    }>): Promise<AgentProfile>;
    /**
     * Delete an agent by entity ID.
     */
    deleteAgent(entityId: number): Promise<void>;
    /**
     * List all agents currently in the kernel.
     */
    listAgents(): Promise<AgentProfile[]>;
    /**
     * Add XP to a specific skill on an agent.
     */
    addSkillXp(entityId: number, skillId: string, xp: number): Promise<SkillNode>;
    /**
     * Get all skills for an agent.
     */
    getSkills(entityId: number): Promise<Record<string, SkillNode>>;
    /**
     * Sync full state from the kernel (all agents).
     */
    syncState(): Promise<{
        agents: AgentProfile[];
        count: number;
    }>;
    /**
     * Send a request and wait for the response.
     *
     * The current C++ server processes requests sequentially per client
     * and doesn't echo request IDs, so we use a simple FIFO queue.
     * The `id` field is included for future protocol extensions.
     */
    private request;
    /**
     * Handle incoming data from the socket.
     * Splits on newlines and resolves pending requests in FIFO order.
     */
    private onData;
}
//# sourceMappingURL=AgentKernelClient.d.ts.map