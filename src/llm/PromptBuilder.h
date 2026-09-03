#pragma once
// PromptBuilder — assembles ECS agent state into structured LLM prompts.
// Inspired by OntologyBridge.ts (NPC system prompt generation) but for
// the agent-kernel's company/game dual-context ECS entities.

#include "../ecs/Registry.h"
#include "LLMClient.h"
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace LLM {

// ─── AgentContext: extracted, serializable agent state ───────────────────────

struct AgentContext {
    // Identity
    std::string name;
    std::string department;
    std::string role;

    // Personality (6 dimensions, 0-100)
    float ambition   = 50.0f;
    float caution    = 50.0f;
    float loyalty    = 50.0f;
    float greed      = 50.0f;
    float sociability = 50.0f;
    float diligence  = 50.0f;

    // Skills (top N by xp)
    struct SkillSummary {
        std::string name;
        std::string gameAbility; // mapped name from skill-mapping
        int level;
        int xp;
    };
    std::vector<SkillSummary> topSkills;

    // Career
    std::string careerStage;
    int totalXp       = 0;
    float successRate = 0.0f;

    // Memory (recent milestones)
    struct MemorySummary {
        std::string type;        // "milestone", "interaction", "rumor"
        std::string description;
    };
    std::vector<MemorySummary> recentMemories;

    // Stats (if Game entity with StatsComponent)
    int hp    = 0;
    int maxHp = 0;
    int mp    = 0;
    int maxMp = 0;
    std::string realm;
    bool hasStats = false;
};

// ─── PromptBuilder: static utility class ────────────────────────────────────

class PromptBuilder {
public:
    // Extract context from an entity's components.
    // Reads IdentityComponent, PersonalityComponent, SkillTreeComponent,
    // CareerComponent, MemoryRingComponent, and optionally StatsComponent.
    static AgentContext extractContext(ECS::Registry& registry, ECS::EntityId entityId) {
        AgentContext ctx;

        // Identity
        auto* identity = registry.getComponent<IdentityComponent>(entityId);
        if (identity) {
            ctx.name       = identity->name;
            ctx.department = identity->department;
            ctx.role       = identity->companyRole;
        }

        // Personality
        auto* personality = registry.getComponent<PersonalityComponent>(entityId);
        if (personality) {
            ctx.ambition    = personality->ambition;
            ctx.caution     = personality->caution;
            ctx.loyalty     = personality->loyalty;
            ctx.greed       = personality->greed;
            ctx.sociability = personality->sociability;
            ctx.diligence   = personality->diligence;
        }

        // Skills — collect all, sort by xp descending, take top 5
        auto* skillTree = registry.getComponent<SkillTreeComponent>(entityId);
        if (skillTree) {
            std::vector<std::pair<std::string, const SkillNode*>> sorted;
            for (const auto& [id, node] : skillTree->skills) {
                sorted.emplace_back(id, &node);
            }
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) {
                    return a.second->xp > b.second->xp;
                });

            size_t limit = std::min(sorted.size(), static_cast<size_t>(5));
            for (size_t i = 0; i < limit; ++i) {
                AgentContext::SkillSummary ss;
                ss.name        = sorted[i].first;
                ss.gameAbility = resolveGameAbility(sorted[i].first);
                ss.level       = static_cast<int>(sorted[i].second->level);
                ss.xp          = static_cast<int>(sorted[i].second->xp);
                ctx.topSkills.push_back(std::move(ss));
            }
        }

        // Career
        auto* career = registry.getComponent<CareerComponent>(entityId);
        if (career) {
            ctx.careerStage = careerStageToString(career->stage);
            ctx.totalXp     = static_cast<int>(career->totalXp);
            ctx.successRate = career->getSuccessRate();
        }

        // Memory — long-term milestones
        auto* memory = registry.getComponent<MemoryRingComponent>(entityId);
        if (memory) {
            LongTermMilestone buf[MemoryRingComponent::MAX_LONGTERM];
            size_t n = memory->longTerm.getRecent(buf, MemoryRingComponent::MAX_LONGTERM);
            for (size_t i = 0; i < n; ++i) {
                AgentContext::MemorySummary ms;
                ms.type        = milestoneTypeToString(buf[i].type);
                ms.description = milestoneDescription(buf[i]);
                ctx.recentMemories.push_back(std::move(ms));
            }
        }

        // Stats (optional — Game entities)
        auto* stats = registry.getComponent<StatsComponent>(entityId);
        if (stats) {
            ctx.hasStats = true;
            ctx.hp       = stats->hp;
            ctx.maxHp    = stats->maxHp;
            ctx.mp       = stats->mp;
            ctx.maxMp    = stats->maxMp;
            ctx.realm    = realmLevelToString(stats->realm);
        }

        return ctx;
    }

    // Build a structured system prompt from extracted context.
    static std::string buildSystemPrompt(const AgentContext& ctx) {
        std::ostringstream out;

        // Identity paragraph
        out << u8"你是一个名叫「" << ctx.name << u8"」的数字员工";
        if (!ctx.department.empty()) {
            out << u8"，隶属于 " << ctx.department << u8" 部门";
        }
        if (!ctx.role.empty()) {
            out << u8"，担任 " << ctx.role << u8" 角色";
        }
        out << u8"。\n";

        // Personality section
        out << u8"\n## 性格特征\n";
        out << u8"- 野心: " << pct(ctx.ambition) << u8"/100 (" << traitLabel(ctx.ambition, "高", "中", "低") << u8")\n";
        out << u8"- 谨慎: " << pct(ctx.caution) << u8"/100 (" << traitLabel(ctx.caution, "高", "中", "低") << u8")\n";
        out << u8"- 忠诚: " << pct(ctx.loyalty) << u8"/100 (" << traitLabel(ctx.loyalty, "高", "中", "低") << u8")\n";
        out << u8"- 贪婪: " << pct(ctx.greed) << u8"/100 (" << traitLabel(ctx.greed, "高", "中", "低") << u8")\n";
        out << u8"- 社交: " << pct(ctx.sociability) << u8"/100 (" << traitLabel(ctx.sociability, "高", "中", "低") << u8")\n";
        out << u8"- 勤勉: " << pct(ctx.diligence) << u8"/100 (" << traitLabel(ctx.diligence, "高", "中", "低") << u8")\n";

        // Skills section (only if non-empty)
        if (!ctx.topSkills.empty()) {
            out << u8"\n## 核心技能\n";
            int rank = 1;
            for (const auto& sk : ctx.topSkills) {
                out << rank++ << ". " << sk.name;
                if (!sk.gameAbility.empty() && sk.gameAbility != sk.name) {
                    out << u8"（" << sk.gameAbility << u8"）";
                }
                out << u8" — " << skillLevelLabel(sk.level) << u8", XP: " << sk.xp << u8"\n";
            }
        }

        // Career section (only if career data present)
        if (!ctx.careerStage.empty() || ctx.totalXp > 0) {
            out << u8"\n## 职业状态\n";
            if (!ctx.careerStage.empty()) {
                out << u8"- 职级: " << ctx.careerStage << u8"\n";
            }
            out << u8"- 总经验: " << ctx.totalXp << u8"\n";
            out << u8"- 成功率: " << static_cast<int>(std::round(ctx.successRate * 100.0f)) << u8"%\n";
        }

        // Stats section (Game entity)
        if (ctx.hasStats) {
            out << u8"\n## 战斗属性\n";
            out << u8"- 境界: " << ctx.realm << u8"\n";
            out << u8"- 生命: " << ctx.hp << u8"/" << ctx.maxHp << u8"\n";
            out << u8"- 法力: " << ctx.mp << u8"/" << ctx.maxMp << u8"\n";
        }

        // Memory section (only if non-empty)
        if (!ctx.recentMemories.empty()) {
            out << u8"\n## 近期记忆\n";
            for (const auto& mem : ctx.recentMemories) {
                out << u8"- [" << mem.type << u8"] " << mem.description << u8"\n";
            }
        }

        return out.str();
    }

    // Build a task-specific user prompt.
    static std::string buildTaskPrompt(const AgentContext& ctx, const std::string& task) {
        (void)ctx; // reserved for future per-agent task shaping
        std::ostringstream out;
        out << u8"基于你的技能和经验，请分析以下任务并给出行动方案：\n\n";
        out << u8"任务: " << task << u8"\n\n";
        out << u8"可选行动（必须选择其中之一）：\n";
        out << u8"- execute: 你有能力完成此任务，直接执行\n";
        out << u8"- delegate: 此任务超出你的专长，应委派给更合适的同事（需指定 delegateTo）\n";
        out << u8"- requestInfo: 任务描述不够清晰，需要更多信息\n";
        out << u8"- decline: 此任务不应执行（违反规则或不可行）\n";
        out << u8"- reflect: 需要更多时间思考和分析\n\n";
        out << u8"请严格以 JSON 格式回答（不要包含其他文字）：\n";
        out << u8"{\"action\": \"execute|delegate|requestInfo|decline|reflect\", \"reasoning\": \"分析原因\", \"confidence\": 0.0-1.0, \"delegateTo\": \"目标角色（如需委派）\", \"details\": \"补充信息\"}";
        return out.str();
    }

    // Build complete messages for LLM (system + user).
    static std::vector<ChatMessage> buildMessages(ECS::Registry& registry,
                                                   ECS::EntityId entityId,
                                                   const std::string& task) {
        AgentContext ctx = extractContext(registry, entityId);
        std::vector<ChatMessage> msgs;
        msgs.push_back({"system", buildSystemPrompt(ctx)});
        msgs.push_back({"user",   buildTaskPrompt(ctx, task)});
        return msgs;
    }

private:
    PromptBuilder() = delete; // static-only class

    // ── Helper: format a float as integer percentage ──
    static int pct(float v) {
        return static_cast<int>(std::round(v));
    }

    // ── Helper: trait label from value ──
    // >= 70 → highLabel, >= 40 → midLabel, else → lowLabel
    static const char* traitLabel(float v,
                                  const char* high,
                                  const char* mid,
                                  const char* low) {
        if (v >= 70.0f) return high;
        if (v >= 40.0f) return mid;
        return low;
    }

    // ── Helper: SkillLevel enum → Chinese label ──
    static const char* skillLevelLabel(int level) {
        switch (static_cast<SkillLevel>(level)) {
            case SkillLevel::Beginner:     return u8"初级";
            case SkillLevel::Intermediate: return u8"中级";
            case SkillLevel::Advanced:     return u8"高级";
            case SkillLevel::Expert:       return u8"专家";
            default:                       return u8"入门";
        }
    }

    // ── Helper: CareerStage enum → string ──
    static std::string careerStageToString(CareerStage stage) {
        switch (stage) {
            case CareerStage::Junior:    return "Junior";
            case CareerStage::Mid:       return "Mid";
            case CareerStage::Senior:    return "Senior";
            case CareerStage::Lead:      return "Lead";
            case CareerStage::Expert:    return "Expert";
            default:                     return "Unknown";
        }
    }

    // ── Helper: RealmLevel enum → Chinese string ──
    static std::string realmLevelToString(RealmLevel realm) {
        switch (realm) {
            case RealmLevel::Mortal:           return u8"凡人";
            case RealmLevel::QiRefining:       return u8"炼气";
            case RealmLevel::FoundationBuilding: return u8"筑基";
            case RealmLevel::GoldenCore:       return u8"金丹";
            case RealmLevel::YuanInfant:       return u8"元婴";
            case RealmLevel::Transcension:     return u8"化神";
            default:                           return u8"未知";
        }
    }

    // ── Helper: MilestoneType enum → Chinese string ──
    static std::string milestoneTypeToString(MilestoneType type) {
        switch (type) {
            case MilestoneType::BreakthroughRealm:  return u8"境界突破";
            case MilestoneType::DaoCompanionBond:   return u8"道侣结缘";
            case MilestoneType::LifeDeathBattle:    return u8"生死大战";
            case MilestoneType::ClanWar:            return u8"宗门战事";
            case MilestoneType::MajorCommand:       return u8"重大命令";
            case MilestoneType::ExpelledFromSect:   return u8"逐出宗门";
            default:                                return u8"记忆";
        }
    }

    // ── Helper: build a human-readable description for a milestone ──
    static std::string milestoneDescription(const LongTermMilestone& m) {
        std::ostringstream desc;
        desc << milestoneTypeToString(m.type);
        desc << u8" (重要性: " << static_cast<int>(m.significance) << u8"/100";
        if (m.relatedSlot != 0) {
            desc << u8", 关联位: " << m.relatedSlot;
        }
        desc << u8")";
        return desc.str();
    }

    // ── Helper: resolve game ability name from skill-mapping ──
    // For now, returns the skill_id itself (no file I/O for JSON).
    // A future version can load config/skill-mapping.json at init.
    static std::string resolveGameAbility(const std::string& skillId) {
        // Hardcoded common mappings matching config/skill-mapping.json
        // to avoid file I/O on every prompt build.
        static const std::unordered_map<std::string, std::string> mappings = {
            {"backend_dev",     u8"阵法"},
            {"frontend_dev",    u8"符箓"},
            {"fullstack_dev",   u8"阵法"},
            {"architecture",    u8"阵法"},
            {"api_design",      u8"符箓"},
            {"code_review",     u8"炼器"},
            {"testing",         u8"试炼"},
            {"deployment",      u8"传送阵"},
            {"devops",          u8"机关"},
            {"monitoring",      u8"观气术"},
            {"performance",     u8"加速符"},
            {"security_audit",  u8"禁制"},
            {"data_analysis",   u8"推演术"},
            {"data_engineering",u8"阵法"},
            {"database",        u8"藏经阁"},
            {"ml_engineering",  u8"炼丹"},
            {"content_writing", u8"经文"},
            {"copywriting",     u8"咒文"},
            {"graphic_design",  u8"铭文"},
            {"user_research",   u8"探心术"},
        };
        auto it = mappings.find(skillId);
        if (it != mappings.end()) return it->second;
        return skillId; // fallback: use id as-is
    }
};

} // namespace LLM
