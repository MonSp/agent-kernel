import * as fs from 'fs';
import * as path from 'path';
import { SkillMapping } from './types';

/** Default path to skill-mapping.json relative to the agent-kernel root. */
const DEFAULT_CONFIG_PATH = path.resolve(
  __dirname,
  '..',
  '..',
  'config',
  'skill-mapping.json',
);

/**
 * Load all skill mappings from a JSON config file.
 *
 * @param configPath - Absolute or relative path to skill-mapping.json.
 *                     Defaults to `<agent-kernel>/config/skill-mapping.json`.
 * @returns A record keyed by Company skill ID.
 */
export function loadMappings(
  configPath: string = DEFAULT_CONFIG_PATH,
): Record<string, SkillMapping> {
  const raw = fs.readFileSync(configPath, 'utf-8');
  const parsed: Record<string, unknown> = JSON.parse(raw);

  const mappings: Record<string, SkillMapping> = {};
  for (const [key, value] of Object.entries(parsed)) {
    // Skip meta-keys like "_comment"
    if (key.startsWith('_')) continue;

    const entry = value as Record<string, string>;
    if (entry.gameAbility && entry.category && entry.description) {
      mappings[key] = {
        gameAbility: entry.gameAbility,
        category: entry.category,
        description: entry.description,
      };
    }
  }

  return mappings;
}

/** In-memory cache of the loaded mappings. */
let cachedMappings: Record<string, SkillMapping> | null = null;

/**
 * Get the game ability name for a given Company skill ID.
 *
 * @param skillId - Company skill identifier (e.g. "backend_dev").
 * @param configPath - Optional path override.
 * @returns The game ability name, or undefined if not found.
 */
export function getGameAbility(
  skillId: string,
  configPath?: string,
): string | undefined {
  if (!cachedMappings || configPath) {
    cachedMappings = loadMappings(configPath);
  }
  return cachedMappings[skillId]?.gameAbility;
}

/**
 * Get the list of all Company skill IDs defined in the mapping.
 *
 * @param configPath - Optional path override.
 * @returns Array of skill ID strings.
 */
export function getCompanySkills(configPath?: string): string[] {
  if (!cachedMappings || configPath) {
    cachedMappings = loadMappings(configPath);
  }
  return Object.keys(cachedMappings);
}

/** Reset the internal cache (useful in tests). */
export function resetCache(): void {
  cachedMappings = null;
}
