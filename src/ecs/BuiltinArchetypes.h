#pragma once

// BuiltinArchetypes.h — Pre-registered Company and Game archetypes
//
// Call registerBuiltinArchetypes() after registerAllSchemas() to populate
// the ArchetypeRegistry with 6 standard archetypes (3 Company + 3 Game).

#include "EntityArchetype.h"
#include "ComponentSchemas.h"

inline void registerBuiltinArchetypes() {
    auto& archReg = ECS::ArchetypeRegistry::instance();

    // Guard against double registration
    if (archReg.getArchetype("Engineer") != nullptr) return;

    // Ensure schemas are available
    registerAllSchemas();

    // ═══════════════════════════════════════════════════════════════════════
    // Company Archetypes
    // ═══════════════════════════════════════════════════════════════════════

    // ── Engineer ── Identity + SkillTree + Career + Evolution
    {
        ECS::EntityArchetype arch;
        arch.name = "Engineer";
        arch.description = "Company engineer: identity, skill tree, career progression, and self-evolution";

        arch.components.push_back({"IdentityComponent", {
            {"role", "Specialist"},
            {"department", "Engineering"}
        }});
        arch.components.push_back({"SkillTreeComponent", {}});
        arch.components.push_back({"CareerComponent", {
            {"stage", "Junior"},
            {"totalXp", "0"}
        }});
        arch.components.push_back({"EvolutionComponent", {}});

        archReg.registerArchetype(std::move(arch));
    }

    // ── Designer ── Identity + SkillTree + Career
    {
        ECS::EntityArchetype arch;
        arch.name = "Designer";
        arch.description = "Company designer: identity, skill tree, and career progression";

        arch.components.push_back({"IdentityComponent", {
            {"role", "Specialist"},
            {"department", "Design"}
        }});
        arch.components.push_back({"SkillTreeComponent", {}});
        arch.components.push_back({"CareerComponent", {
            {"stage", "Junior"},
            {"totalXp", "0"}
        }});

        archReg.registerArchetype(std::move(arch));
    }

    // ── Manager ── Identity + SkillTree + Career + Evolution + Personality
    {
        ECS::EntityArchetype arch;
        arch.name = "Manager";
        arch.description = "Company manager: identity, skill tree, career, evolution, and personality";

        arch.components.push_back({"IdentityComponent", {
            {"role", "Manager"},
            {"department", "Management"}
        }});
        arch.components.push_back({"SkillTreeComponent", {}});
        arch.components.push_back({"CareerComponent", {
            {"stage", "Mid"},
            {"totalXp", "0"}
        }});
        arch.components.push_back({"EvolutionComponent", {}});
        arch.components.push_back({"PersonalityComponent", {
            {"ambition", "50"},
            {"caution", "50"},
            {"loyalty", "50"},
            {"greed", "50"},
            {"sociability", "50"},
            {"diligence", "50"}
        }});

        archReg.registerArchetype(std::move(arch));
    }

    // ═══════════════════════════════════════════════════════════════════════
    // Game Archetypes
    // ═══════════════════════════════════════════════════════════════════════

    // ── Warrior ── Identity + Stats + Personality + SkillTree + Career + Lifecycle
    {
        ECS::EntityArchetype arch;
        arch.name = "Warrior";
        arch.description = "Game warrior: combat-ready entity with stats, personality, skills, and lifecycle";

        arch.components.push_back({"IdentityComponent", {
            {"role", "Specialist"},
            {"companyRole", "Warrior"}
        }});
        arch.components.push_back({"StatsComponent", {
            {"hp", "100"},
            {"maxHp", "100"},
            {"mp", "50"},
            {"maxMp", "50"}
        }});
        arch.components.push_back({"PersonalityComponent", {
            {"ambition", "50"},
            {"caution", "50"},
            {"loyalty", "50"},
            {"greed", "50"},
            {"sociability", "50"},
            {"diligence", "50"}
        }});
        arch.components.push_back({"SkillTreeComponent", {}});
        arch.components.push_back({"CareerComponent", {
            {"stage", "Junior"},
            {"totalXp", "0"}
        }});
        arch.components.push_back({"LifecycleComponent", {
            {"lifeState", "Active"},
            {"age", "0"}
        }});

        archReg.registerArchetype(std::move(arch));
    }

    // ── Alchemist ── Identity + Stats + Personality + SkillTree + Career + Lifecycle
    {
        ECS::EntityArchetype arch;
        arch.name = "Alchemist";
        arch.description = "Game alchemist: crafter entity with stats, personality, skills, and lifecycle";

        arch.components.push_back({"IdentityComponent", {
            {"role", "Specialist"},
            {"companyRole", "Alchemist"}
        }});
        arch.components.push_back({"StatsComponent", {
            {"hp", "100"},
            {"maxHp", "100"},
            {"mp", "50"},
            {"maxMp", "50"}
        }});
        arch.components.push_back({"PersonalityComponent", {
            {"ambition", "50"},
            {"caution", "50"},
            {"loyalty", "50"},
            {"greed", "50"},
            {"sociability", "50"},
            {"diligence", "50"}
        }});
        arch.components.push_back({"SkillTreeComponent", {}});
        arch.components.push_back({"CareerComponent", {
            {"stage", "Junior"},
            {"totalXp", "0"}
        }});
        arch.components.push_back({"LifecycleComponent", {
            {"lifeState", "Active"},
            {"age", "0"}
        }});

        archReg.registerArchetype(std::move(arch));
    }

    // ── Elder ── Identity + Stats + Personality + SkillTree + Career + Lifecycle + Social + Memory
    {
        ECS::EntityArchetype arch;
        arch.name = "Elder";
        arch.description = "Game elder: powerful entity with all systems including social and memory";

        arch.components.push_back({"IdentityComponent", {
            {"role", "Lead"},
            {"companyRole", "Elder"}
        }});
        arch.components.push_back({"StatsComponent", {
            {"hp", "100"},
            {"maxHp", "100"},
            {"mp", "50"},
            {"maxMp", "50"}
        }});
        arch.components.push_back({"PersonalityComponent", {
            {"ambition", "50"},
            {"caution", "50"},
            {"loyalty", "50"},
            {"greed", "50"},
            {"sociability", "50"},
            {"diligence", "50"}
        }});
        arch.components.push_back({"SkillTreeComponent", {}});
        arch.components.push_back({"CareerComponent", {
            {"stage", "Junior"},
            {"totalXp", "0"}
        }});
        arch.components.push_back({"LifecycleComponent", {
            {"lifeState", "Active"},
            {"age", "0"}
        }});
        arch.components.push_back({"SocialComponent", {}});
        arch.components.push_back({"MemoryRingComponent", {}});

        archReg.registerArchetype(std::move(arch));
    }
}
