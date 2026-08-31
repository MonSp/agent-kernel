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
