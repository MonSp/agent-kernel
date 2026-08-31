#pragma once

// ComponentSchemas.h — Registers schemas for all 9 ECS components
//
// Call registerAllSchemas() at startup (daemon or test runner) to populate
// the global SchemaRegistry with introspectable metadata for every component.

#include "ecs/Schema.h"
#include "ecs/components/IdentityComponent.h"
#include "ecs/components/StatsComponent.h"
#include "ecs/components/PersonalityComponent.h"
#include "ecs/components/LifecycleComponent.h"
#include "ecs/components/SocialComponent.h"
#include "ecs/components/MemoryRingComponent.h"
#include "ecs/components/SkillTreeComponent.h"
#include "ecs/components/CareerComponent.h"
#include "ecs/components/EvolutionComponent.h"

inline void registerAllSchemas() {
    auto& reg = ECS::SchemaRegistry::instance();

    // ── 1. IdentityComponent (6 fields) ───────────────────────────────────────
    {
        ECS::ComponentSchema schema;
        schema.name = "IdentityComponent";
        schema.description = "Agent identity: ID, name, department, role";

        schema.addField("id", ECS::FieldType::String,
                        offsetof(IdentityComponent, id), sizeof(std::string),
                        "std::string", "Agent unique ID");
        schema.addField("name", ECS::FieldType::String,
                        offsetof(IdentityComponent, name), sizeof(std::string),
                        "std::string", "Display name");
        schema.addField("department", ECS::FieldType::String,
                        offsetof(IdentityComponent, department), sizeof(std::string),
                        "std::string", "Department name");
        schema.addField("companyRole", ECS::FieldType::String,
                        offsetof(IdentityComponent, companyRole), sizeof(std::string),
                        "std::string", "Company role title");
        schema.addField("teamId", ECS::FieldType::String,
                        offsetof(IdentityComponent, teamId), sizeof(std::string),
                        "std::string", "Team identifier");
        schema.addEnumField("role", offsetof(IdentityComponent, role), sizeof(AgentRole),
                            {{0, "Worker"}, {1, "Specialist"}, {2, "Lead"},
                             {3, "Manager"}, {4, "Director"}},
                            "Agent role in organization");

        reg.registerSchema("IdentityComponent", std::move(schema));
    }

    // ── 2. StatsComponent (8 fields) ──────────────────────────────────────────
    {
        ECS::ComponentSchema schema;
        schema.name = "StatsComponent";
        schema.description = "Combat and career stats for an agent";

        schema.addField("power", ECS::FieldType::Int32,
                        offsetof(StatsComponent, power), sizeof(int32_t),
                        "int32_t", "Combat power");
        schema.addFieldWithConstraint("hp", ECS::FieldType::Int32,
                                      offsetof(StatsComponent, hp), sizeof(int32_t),
                                      0.0f, 2147483647.0f, "Current HP");
        schema.addFieldWithConstraint("maxHp", ECS::FieldType::Int32,
                                      offsetof(StatsComponent, maxHp), sizeof(int32_t),
                                      0.0f, 2147483647.0f, "Maximum HP");
        schema.addFieldWithConstraint("mp", ECS::FieldType::Int32,
                                      offsetof(StatsComponent, mp), sizeof(int32_t),
                                      0.0f, 2147483647.0f, "Current MP");
        schema.addFieldWithConstraint("maxMp", ECS::FieldType::Int32,
                                      offsetof(StatsComponent, maxMp), sizeof(int32_t),
                                      0.0f, 2147483647.0f, "Maximum MP");
        schema.addEnumField("realm", offsetof(StatsComponent, realm), sizeof(RealmLevel),
                            {{0, "Mortal"}, {1, "QiRefining"}, {2, "FoundationBuilding"},
                             {3, "GoldenCore"}, {4, "YuanInfant"}, {5, "Transcension"}},
                            "Cultivation realm");
        schema.addFieldWithConstraint("xp", ECS::FieldType::Int32,
                                      offsetof(StatsComponent, xp), sizeof(int32_t),
                                      0.0f, 2147483647.0f, "Experience points");
        schema.addField("careerLevel", ECS::FieldType::Uint8,
                        offsetof(StatsComponent, careerLevel), sizeof(uint8_t),
                        "uint8_t", "Career level");

        reg.registerSchema("StatsComponent", std::move(schema));
    }

    // ── 3. PersonalityComponent (6 fields, all 0-100) ─────────────────────────
    {
        ECS::ComponentSchema schema;
        schema.name = "PersonalityComponent";
        schema.description = "6-dimension personality model, each trait 0-100";

        schema.addFieldWithConstraint("ambition", ECS::FieldType::Float32,
                                      offsetof(PersonalityComponent, ambition), sizeof(float),
                                      0.0f, 100.0f, "Ambition level");
        schema.addFieldWithConstraint("caution", ECS::FieldType::Float32,
                                      offsetof(PersonalityComponent, caution), sizeof(float),
                                      0.0f, 100.0f, "Caution level");
        schema.addFieldWithConstraint("loyalty", ECS::FieldType::Float32,
                                      offsetof(PersonalityComponent, loyalty), sizeof(float),
                                      0.0f, 100.0f, "Loyalty level");
        schema.addFieldWithConstraint("greed", ECS::FieldType::Float32,
                                      offsetof(PersonalityComponent, greed), sizeof(float),
                                      0.0f, 100.0f, "Greed level");
        schema.addFieldWithConstraint("sociability", ECS::FieldType::Float32,
                                      offsetof(PersonalityComponent, sociability), sizeof(float),
                                      0.0f, 100.0f, "Sociability level");
        schema.addFieldWithConstraint("diligence", ECS::FieldType::Float32,
                                      offsetof(PersonalityComponent, diligence), sizeof(float),
                                      0.0f, 100.0f, "Diligence level");

        reg.registerSchema("PersonalityComponent", std::move(schema));
    }

    // ── 4. LifecycleComponent (5 fields — skip optional<DeathCause>) ──────────
    {
        ECS::ComponentSchema schema;
        schema.name = "LifecycleComponent";
        schema.description = "Agent lifecycle state machine";

        schema.addField("birthTime", ECS::FieldType::Uint64,
                        offsetof(LifecycleComponent, birthTime), sizeof(uint64_t),
                        "uint64_t", "Birth timestamp");
        schema.addFieldWithConstraint("age", ECS::FieldType::Float32,
                                      offsetof(LifecycleComponent, age), sizeof(float),
                                      0.0f, 1e30f, "Agent age in years");
        schema.addEnumField("lifeState", offsetof(LifecycleComponent, lifeState), sizeof(AgentLifeState),
                            {{0, "Idle"}, {1, "Active"}, {2, "Paused"}, {3, "Terminated"}},
                            "Current life state");
        schema.addEnumField("birthType", offsetof(LifecycleComponent, birthType), sizeof(BirthType),
                            {{0, "Natural"}, {1, "WarOrphan"}, {2, "Wanderer"}, {3, "DemonBeast"}},
                            "Birth type");
        schema.addField("lastUpdateTime", ECS::FieldType::Uint64,
                        offsetof(LifecycleComponent, lastUpdateTime), sizeof(uint64_t),
                        "uint64_t", "Last update timestamp");

        reg.registerSchema("LifecycleComponent", std::move(schema));
    }

    // ── 5. SocialComponent (8 float fields — skip cooldown array) ─────────────
    {
        ECS::ComponentSchema schema;
        schema.name = "SocialComponent";
        schema.description = "Social needs and emotion system";

        schema.addFieldWithConstraint("hunger", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, hunger), sizeof(float),
                                      0.0f, 100.0f, "Hunger level");
        schema.addFieldWithConstraint("fatigue", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, fatigue), sizeof(float),
                                      0.0f, 100.0f, "Fatigue level");
        schema.addFieldWithConstraint("energy", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, energy), sizeof(float),
                                      0.0f, 100.0f, "Energy level");
        schema.addFieldWithConstraint("socialDesire", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, socialDesire), sizeof(float),
                                      0.0f, 100.0f, "Social desire level");
        schema.addFieldWithConstraint("mood", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, mood), sizeof(float),
                                      0.0f, 100.0f, "Mood level");
        schema.addFieldWithConstraint("anger", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, anger), sizeof(float),
                                      0.0f, 100.0f, "Anger level");
        schema.addFieldWithConstraint("fear", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, fear), sizeof(float),
                                      0.0f, 100.0f, "Fear level");
        schema.addFieldWithConstraint("joy", ECS::FieldType::Float32,
                                      offsetof(SocialComponent, joy), sizeof(float),
                                      0.0f, 100.0f, "Joy level");

        reg.registerSchema("SocialComponent", std::move(schema));
    }

    // ── 6. MemoryRingComponent — name only, too complex for field registration ─
    {
        ECS::ComponentSchema schema;
        schema.name = "MemoryRingComponent";
        schema.description = "3-tier memory system (short/mid/long-term + rumors). "
                             "Ring buffers make per-field introspection impractical.";

        reg.registerSchema("MemoryRingComponent", std::move(schema));
    }

    // ── 7. SkillTreeComponent — name only, unordered_map not introspectable ───
    {
        ECS::ComponentSchema schema;
        schema.name = "SkillTreeComponent";
        schema.description = "Skill tree with XP, level-up, and dependency tracking. "
                             "Contains unordered_map; per-field introspection not available.";

        reg.registerSchema("SkillTreeComponent", std::move(schema));
    }

    // ── 8. CareerComponent (5 fields) ─────────────────────────────────────────
    {
        ECS::ComponentSchema schema;
        schema.name = "CareerComponent";
        schema.description = "Career progression: XP, stage, tasks, reviews";

        schema.addFieldWithConstraint("totalXp", ECS::FieldType::Uint32,
                                      offsetof(CareerComponent, totalXp), sizeof(uint32_t),
                                      0.0f, 4294967295.0f, "Total career XP");
        schema.addEnumField("stage", offsetof(CareerComponent, stage), sizeof(CareerStage),
                            {{0, "Junior"}, {1, "Mid"}, {2, "Senior"}, {3, "Lead"}, {4, "Expert"}},
                            "Career stage");
        schema.addFieldWithConstraint("tasksCompleted", ECS::FieldType::Uint32,
                                      offsetof(CareerComponent, tasksCompleted), sizeof(uint32_t),
                                      0.0f, 4294967295.0f, "Total tasks completed");
        schema.addFieldWithConstraint("tasksSucceeded", ECS::FieldType::Uint32,
                                      offsetof(CareerComponent, tasksSucceeded), sizeof(uint32_t),
                                      0.0f, 4294967295.0f, "Tasks completed successfully");
        schema.addFieldWithConstraint("avgReviewScore", ECS::FieldType::Float32,
                                      offsetof(CareerComponent, avgReviewScore), sizeof(float),
                                      0.0f, 10.0f, "Average review score (0-10)");

        reg.registerSchema("CareerComponent", std::move(schema));
    }

    // ── 9. EvolutionComponent (3 scalar fields — skip vector<EvolutionRecord>) ─
    {
        ECS::ComponentSchema schema;
        schema.name = "EvolutionComponent";
        schema.description = "Evolution history and self-improvement tracking";

        schema.addFieldWithConstraint("totalEvolutions", ECS::FieldType::Uint32,
                                      offsetof(EvolutionComponent, totalEvolutions), sizeof(uint32_t),
                                      0.0f, 4294967295.0f, "Total evolution attempts");
        schema.addFieldWithConstraint("successfulEvolutions", ECS::FieldType::Uint32,
                                      offsetof(EvolutionComponent, successfulEvolutions), sizeof(uint32_t),
                                      0.0f, 4294967295.0f, "Successful evolution count");
        schema.addFieldWithConstraint("diversityScore", ECS::FieldType::Float32,
                                      offsetof(EvolutionComponent, diversityScore), sizeof(float),
                                      0.0f, 1.0f, "Diversity score (0.0-1.0)");

        reg.registerSchema("EvolutionComponent", std::move(schema));
    }
}
