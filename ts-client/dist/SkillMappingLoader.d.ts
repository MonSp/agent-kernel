import { SkillMapping } from './types';
/**
 * Load all skill mappings from a JSON config file.
 *
 * @param configPath - Absolute or relative path to skill-mapping.json.
 *                     Defaults to `<agent-kernel>/config/skill-mapping.json`.
 * @returns A record keyed by Company skill ID.
 */
export declare function loadMappings(configPath?: string): Record<string, SkillMapping>;
/**
 * Get the game ability name for a given Company skill ID.
 *
 * @param skillId - Company skill identifier (e.g. "backend_dev").
 * @param configPath - Optional path override.
 * @returns The game ability name, or undefined if not found.
 */
export declare function getGameAbility(skillId: string, configPath?: string): string | undefined;
/**
 * Get the list of all Company skill IDs defined in the mapping.
 *
 * @param configPath - Optional path override.
 * @returns Array of skill ID strings.
 */
export declare function getCompanySkills(configPath?: string): string[];
/** Reset the internal cache (useful in tests). */
export declare function resetCache(): void;
//# sourceMappingURL=SkillMappingLoader.d.ts.map