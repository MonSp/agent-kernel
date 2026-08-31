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
exports.loadMappings = loadMappings;
exports.getGameAbility = getGameAbility;
exports.getCompanySkills = getCompanySkills;
exports.resetCache = resetCache;
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
/** Default path to skill-mapping.json relative to the agent-kernel root. */
const DEFAULT_CONFIG_PATH = path.resolve(__dirname, '..', '..', 'config', 'skill-mapping.json');
/**
 * Load all skill mappings from a JSON config file.
 *
 * @param configPath - Absolute or relative path to skill-mapping.json.
 *                     Defaults to `<agent-kernel>/config/skill-mapping.json`.
 * @returns A record keyed by Company skill ID.
 */
function loadMappings(configPath = DEFAULT_CONFIG_PATH) {
    const raw = fs.readFileSync(configPath, 'utf-8');
    const parsed = JSON.parse(raw);
    const mappings = {};
    for (const [key, value] of Object.entries(parsed)) {
        // Skip meta-keys like "_comment"
        if (key.startsWith('_'))
            continue;
        const entry = value;
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
let cachedMappings = null;
/**
 * Get the game ability name for a given Company skill ID.
 *
 * @param skillId - Company skill identifier (e.g. "backend_dev").
 * @param configPath - Optional path override.
 * @returns The game ability name, or undefined if not found.
 */
function getGameAbility(skillId, configPath) {
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
function getCompanySkills(configPath) {
    if (!cachedMappings || configPath) {
        cachedMappings = loadMappings(configPath);
    }
    return Object.keys(cachedMappings);
}
/** Reset the internal cache (useful in tests). */
function resetCache() {
    cachedMappings = null;
}
//# sourceMappingURL=SkillMappingLoader.js.map