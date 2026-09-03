/** Trait representing a personality dimension (0–100 scale). */
export interface PersonalityTraits {
  ambition: number;
  caution: number;
  loyalty: number;
  greed: number;
  sociability: number;
  diligence: number;
}

/** A single skill node in an agent's skill tree. */
export interface SkillNode {
  skillId: string;
  category: string;
  level: string;
  xp: number;
  usageCount: number;
  successCount: number;
  effectiveness: number;
  dependencies?: string[];
}

/** Optional in-world identity for the agent. */
export interface GameIdentity {
  realm?: string;
  faction?: string;
  gameClass?: string;
  appearance?: string;
}

/** Career stage of an agent. */
export type CareerStage = 'Junior' | 'Mid' | 'Senior' | 'Lead' | 'Expert';

/** Role of an agent within the company hierarchy. */
export type AgentRole = 'Worker' | 'Specialist' | 'Lead' | 'Manager' | 'Director';

/** Full agent profile as returned by the kernel. */
export interface AgentProfile {
  entityId: number;
  identity: {
    id: string;
    name: string;
    department: string;
    companyRole: string;
    role: AgentRole;
    teamId: string;
  };
  skillTree: Record<string, SkillNode>;
  career: {
    totalXp: number;
    stage: CareerStage;
    tasksCompleted: number;
    tasksSucceeded: number;
    avgReviewScore: number;
  };
}

/** Parameters for creating a new agent. */
export interface CreateAgentParams {
  id: string;
  name: string;
  department: string;
  companyRole: string;
  teamId?: string;
  role?: AgentRole;
}

/** A skill-mapping entry from skill-mapping.json. */
export interface SkillMapping {
  gameAbility: string;
  category: string;
  description: string;
}

/** Request envelope sent over the IPC socket. */
export interface KernelRequest {
  method: string;
  params?: Record<string, unknown>;
  id?: string | number;
}

/** Response envelope received from the IPC socket. */
export interface KernelResponse<T = unknown> {
  ok: boolean;
  data?: T;
  error?: string;
}

/** Internal: a pending request waiting for its response. */
export interface PendingRequest {
  resolve: (value: unknown) => void;
  reject: (reason: Error) => void;
}

/** L4 Decision returned by agentDecide. */
export interface Decision {
  action: 'execute' | 'delegate' | 'requestInfo' | 'decline' | 'reflect';
  reasoning: string;
  confidence: number;
  delegateTo?: string;
  details?: string;
}

/** A single effect applied during a tick. */
export interface ActionEffect {
  target: number;   // TargetComponent enum
  fieldName: string;
  delta: number;
  description: string;
}

/** L5 TickResult returned by agentTick. */
export interface TickResult {
  action: string;       // ActionType string
  tickNumber: number;
  timestamp: number;
  decision: Decision;
  effects: ActionEffect[];
}

/** L5 SimulationSummary returned by runSimulation. */
export interface SimulationSummary {
  totalTicks: number;
  averageConfidence: number;
  actionCounts: Record<string, number>;
}

/** L5 runSimulation response. */
export interface SimulationResult {
  results: TickResult[];
  summary: SimulationSummary;
}

/** L6: A journal event from EventJournal. */
export interface JournalEvent {
  id: number;
  timestamp: number;
  entityId: number;
  eventType: string;
  payload: string;
}

/** L6: A mailbox message from AgentMailbox. */
export interface MailboxMessage {
  id: number;
  from: number;
  to: number;
  payload: string;
  timestamp: number;
  delivered: boolean;
  acked: boolean;
}

/** L6: An event pushed from the EventStreamServer. */
export type StreamEvent =
  | { type: 'journal_event'; id: number; timestamp: number; entityId: number; eventType: string; payload: string }
  | { type: 'message_received'; id: number; from: number; to: number; payload: string; timestamp: number };
