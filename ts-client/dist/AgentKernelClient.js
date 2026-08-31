"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.AgentKernelClient = void 0;
const net = __importStar(require("net"));
const DEFAULT_SOCKET_PATH = '/tmp/agent-kernel.sock';
/**
 * TypeScript client for the C++ agent-kernel IPC daemon.
 *
 * Communicates over a Unix domain socket using newline-delimited JSON
 * (matching the C++ UnixSocketServer protocol).
 */
class AgentKernelClient {
    constructor(socketPath = DEFAULT_SOCKET_PATH) {
        this.socket = null;
        this.buffer = '';
        this.nextId = 1;
        this.pending = new Map();
        this.connected = false;
        this.socketPath = socketPath;
    }
    /**
     * Connect to the kernel daemon's Unix socket.
     * Resolves once the connection is established.
     */
    async connect() {
        return new Promise((resolve, reject) => {
            this.socket = net.createConnection(this.socketPath);
            this.socket.on('connect', () => {
                this.connected = true;
                resolve();
            });
            this.socket.on('data', (data) => {
                this.onData(data);
            });
            this.socket.on('error', (err) => {
                if (!this.connected) {
                    reject(err);
                }
                else {
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
    disconnect() {
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
            this.connected = false;
        }
    }
    /** Whether the client is currently connected. */
    isConnected() {
        return this.connected;
    }
    // ─── Agent CRUD ────────────────────────────────────────────────
    /**
     * Create a new agent in the kernel.
     */
    async createAgent(params) {
        const resp = await this.request('createAgent', params);
        return resp;
    }
    /**
     * Get an agent by its entity ID.
     */
    async getAgent(entityId) {
        const resp = await this.request('getAgent', { entityId });
        return resp;
    }
    /**
     * Update fields on an existing agent.
     */
    async updateAgent(entityId, updates) {
        const resp = await this.request('updateAgent', { entityId, ...updates });
        return resp;
    }
    /**
     * Delete an agent by entity ID.
     */
    async deleteAgent(entityId) {
        await this.request('deleteAgent', { entityId });
    }
    /**
     * List all agents currently in the kernel.
     */
    async listAgents() {
        const resp = await this.request('listAgents');
        return resp;
    }
    // ─── Skills ────────────────────────────────────────────────────
    /**
     * Add XP to a specific skill on an agent.
     */
    async addSkillXp(entityId, skillId, xp) {
        const resp = await this.request('addSkillXp', { entityId, skillId, xp });
        return resp;
    }
    /**
     * Get all skills for an agent.
     */
    async getSkills(entityId) {
        const resp = await this.request('getSkills', { entityId });
        return resp;
    }
    // ─── Sync ──────────────────────────────────────────────────────
    /**
     * Sync full state from the kernel (all agents).
     */
    async syncState() {
        const resp = await this.request('syncState');
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
    async request(method, params) {
        if (!this.socket || !this.connected) {
            throw new Error('Not connected. Call connect() first.');
        }
        const id = this.nextId++;
        const req = { method, params, id };
        const line = JSON.stringify(req) + '\n';
        return new Promise((resolve, reject) => {
            // Register pending request (keyed by id for future async matching)
            this.pending.set(id, {
                resolve: resolve,
                reject,
            });
            this.socket.write(line, (err) => {
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
    onData(data) {
        this.buffer += data.toString('utf-8');
        let newlineIdx;
        while ((newlineIdx = this.buffer.indexOf('\n')) !== -1) {
            const line = this.buffer.slice(0, newlineIdx).replace(/\r$/, '');
            this.buffer = this.buffer.slice(newlineIdx + 1);
            if (!line)
                continue;
            // Resolve the oldest pending request (FIFO)
            const firstKey = this.pending.keys().next().value;
            if (firstKey !== undefined) {
                const pending = this.pending.get(firstKey);
                this.pending.delete(firstKey);
                try {
                    const resp = JSON.parse(line);
                    if (resp.ok) {
                        pending.resolve(resp.data);
                    }
                    else {
                        pending.reject(new Error(resp.error || 'Unknown kernel error'));
                    }
                }
                catch (parseErr) {
                    pending.reject(new Error(`Failed to parse response: ${parseErr.message}`));
                }
            }
        }
    }
}
exports.AgentKernelClient = AgentKernelClient;
//# sourceMappingURL=AgentKernelClient.js.map