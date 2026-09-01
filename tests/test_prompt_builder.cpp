// Tests for PromptBuilder — agent state → LLM context assembly.
#include "llm/PromptBuilder.h"
#include "llm/LLMClient.h"
#include "agent_kernel.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace ECS;
using namespace LLM;

// ─── Test: extractContext with full components ──────────────────────────────

static void testExtractContextFull() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    // Identity
    auto& ident = reg.addComponent<IdentityComponent>(id,
        "agent-42", "小明", AgentRole::Specialist);
    ident.department  = "engineering";
    ident.companyRole = "backend_dev";
    ident.teamId      = "team-alpha";

    // Personality
    reg.addComponent<PersonalityComponent>(id,
        80.0f, 30.0f, 60.0f, 40.0f, 55.0f, 70.0f);

    // Skills
    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    skills.getSkill("backend_dev")->xp = 800;
    skills.addSkill("testing", SkillCategory::Engineering, SkillLevel::Intermediate);
    skills.getSkill("testing")->xp = 300;
    skills.addSkill("code_review", SkillCategory::Engineering, SkillLevel::Beginner);
    skills.getSkill("code_review")->xp = 100;
    skills.addSkill("deployment", SkillCategory::Engineering, SkillLevel::Beginner);
    skills.getSkill("deployment")->xp = 50;
    skills.addSkill("devops", SkillCategory::Engineering, SkillLevel::Beginner);
    skills.getSkill("devops")->xp = 25;
    skills.addSkill("monitoring", SkillCategory::Engineering, SkillLevel::Beginner);
    skills.getSkill("monitoring")->xp = 10;

    // Career
    auto& career = reg.addComponent<CareerComponent>(id);
    career.stage = CareerStage::Mid;
    career.totalXp = 600;
    career.tasksCompleted = 6;
    career.tasksSucceeded = 5;

    // Extract
    AgentContext ctx = PromptBuilder::extractContext(reg, id);

    // Verify identity
    assert(ctx.name == "小明");
    assert(ctx.department == "engineering");
    assert(ctx.role == "backend_dev");

    // Verify personality
    assert(std::abs(ctx.ambition - 80.0f) < 0.01f);
    assert(std::abs(ctx.caution - 30.0f) < 0.01f);
    assert(std::abs(ctx.loyalty - 60.0f) < 0.01f);
    assert(std::abs(ctx.greed - 40.0f) < 0.01f);
    assert(std::abs(ctx.sociability - 55.0f) < 0.01f);
    assert(std::abs(ctx.diligence - 70.0f) < 0.01f);

    // Verify skills — should be top 5, sorted by xp descending
    assert(ctx.topSkills.size() == 5);
    assert(ctx.topSkills[0].name == "backend_dev");
    assert(ctx.topSkills[0].xp == 800);
    assert(ctx.topSkills[0].level == static_cast<int>(SkillLevel::Advanced));
    assert(ctx.topSkills[1].name == "testing");
    assert(ctx.topSkills[1].xp == 300);
    assert(ctx.topSkills[2].name == "code_review");
    assert(ctx.topSkills[2].xp == 100);
    assert(ctx.topSkills[3].name == "deployment");
    assert(ctx.topSkills[3].xp == 50);
    assert(ctx.topSkills[4].name == "devops");
    assert(ctx.topSkills[4].xp == 25);
    // "monitoring" (xp=10) should be excluded (only top 5)

    // Verify career
    assert(ctx.careerStage == "Mid");
    assert(ctx.totalXp == 600);
    assert(std::abs(ctx.successRate - (5.0f / 6.0f)) < 0.01f);

    // Verify no stats
    assert(!ctx.hasStats);

    printf("  PASS: testExtractContextFull\n");
}

// ─── Test: buildSystemPrompt contains expected content ──────────────────────

static void testBuildSystemPromptContent() {
    AgentContext ctx;
    ctx.name       = "小明";
    ctx.department = "engineering";
    ctx.role       = "backend_dev";
    ctx.ambition   = 80.0f;
    ctx.caution    = 30.0f;
    ctx.loyalty    = 60.0f;
    ctx.greed      = 40.0f;
    ctx.sociability = 55.0f;
    ctx.diligence  = 70.0f;

    AgentContext::SkillSummary sk1;
    sk1.name = "backend_dev"; sk1.gameAbility = "阵法"; sk1.level = 3; sk1.xp = 800;
    AgentContext::SkillSummary sk2;
    sk2.name = "testing"; sk2.gameAbility = "试炼"; sk2.level = 2; sk2.xp = 300;
    ctx.topSkills.push_back(sk1);
    ctx.topSkills.push_back(sk2);

    ctx.careerStage = "Mid";
    ctx.totalXp     = 600;
    ctx.successRate = 0.83f;

    std::string prompt = PromptBuilder::buildSystemPrompt(ctx);

    // Verify name appears
    assert(prompt.find(u8"小明") != std::string::npos);
    // Verify department appears
    assert(prompt.find(u8"engineering") != std::string::npos);
    // Verify role appears
    assert(prompt.find(u8"backend_dev") != std::string::npos);
    // Verify skills section
    assert(prompt.find(u8"## 核心技能") != std::string::npos);
    assert(prompt.find(u8"backend_dev") != std::string::npos);
    assert(u8"800" && prompt.find("800") != std::string::npos);
    // Verify career section
    assert(prompt.find(u8"## 职业状态") != std::string::npos);
    assert(prompt.find(u8"Mid") != std::string::npos);
    assert(prompt.find("600") != std::string::npos);
    assert(prompt.find(u8"83%") != std::string::npos);
    // Verify personality section
    assert(prompt.find(u8"## 性格特征") != std::string::npos);
    assert(prompt.find(u8"80/100") != std::string::npos);
    // High ambition (80 >= 70)
    assert(prompt.find(u8"野心") != std::string::npos);

    printf("  PASS: testBuildSystemPromptContent\n");
}

// ─── Test: buildMessages returns system + user message ──────────────────────

static void testBuildMessagesStructure() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    // Minimal setup: identity + personality
    auto& ident = reg.addComponent<IdentityComponent>(id,
        "agent-01", "测试员", AgentRole::Worker);
    ident.department  = "qa";
    ident.companyRole = "testing";

    reg.addComponent<PersonalityComponent>(id,
        50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f);

    auto msgs = PromptBuilder::buildMessages(reg, id, "审查新API的安全性");

    // Should be exactly 2 messages: system + user
    assert(msgs.size() == 2);
    assert(msgs[0].role == "system");
    assert(msgs[1].role == "user");

    // System message should contain agent name
    assert(msgs[0].content.find(u8"测试员") != std::string::npos);
    // User message should contain the task
    assert(msgs[1].content.find(u8"审查新API的安全性") != std::string::npos);
    // User message should contain JSON format instruction
    assert(msgs[1].content.find("\"action\"") != std::string::npos);
    assert(msgs[1].content.find("\"confidence\"") != std::string::npos);

    printf("  PASS: testBuildMessagesStructure\n");
}

// ─── Test: Game entity with StatsComponent includes realm/HP ────────────────

static void testExtractContextWithStats() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    auto& ident = reg.addComponent<IdentityComponent>(id,
        "warrior-01", "剑客", AgentRole::Specialist);
    ident.department  = "战斗部";
    ident.companyRole = "战士";

    reg.addComponent<PersonalityComponent>(id,
        90.0f, 20.0f, 70.0f, 30.0f, 40.0f, 80.0f);

    // Stats — Game entity
    auto& stats = reg.addComponent<StatsComponent>(id, 100, 500, 200, RealmLevel::GoldenCore);
    stats.hp = 350;
    stats.mp = 120;

    AgentContext ctx = PromptBuilder::extractContext(reg, id);

    assert(ctx.hasStats);
    assert(ctx.hp == 350);
    assert(ctx.maxHp == 500);
    assert(ctx.mp == 120);
    assert(ctx.maxMp == 200);
    assert(ctx.realm == u8"金丹");

    // Verify prompt includes stats
    std::string prompt = PromptBuilder::buildSystemPrompt(ctx);
    assert(prompt.find(u8"## 战斗属性") != std::string::npos);
    assert(prompt.find(u8"金丹") != std::string::npos);
    assert(prompt.find(u8"350") != std::string::npos);
    assert(prompt.find(u8"500") != std::string::npos);

    printf("  PASS: testExtractContextWithStats\n");
}

// ─── Test: MemoryRingComponent milestones appear in prompt ──────────────────

static void testExtractContextWithMemory() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    reg.addComponent<IdentityComponent>(id,
        "elder-01", "长老", AgentRole::Lead);

    reg.addComponent<PersonalityComponent>(id,
        60.0f, 80.0f, 90.0f, 10.0f, 30.0f, 70.0f);

    // Memory with milestones
    auto& mem = reg.addComponent<MemoryRingComponent>(id);
    mem.recordMilestone(MilestoneType::BreakthroughRealm, 0, 90);
    mem.recordMilestone(MilestoneType::ClanWar, 42, 70);
    mem.recordMilestone(MilestoneType::DaoCompanionBond, 17, 85);

    AgentContext ctx = PromptBuilder::extractContext(reg, id);

    assert(ctx.recentMemories.size() == 3);

    // Verify milestone descriptions
    assert(ctx.recentMemories[0].type == u8"道侣结缘");
    assert(ctx.recentMemories[1].type == u8"宗门战事");
    assert(ctx.recentMemories[2].type == u8"境界突破");

    // Verify prompt includes memories section
    std::string prompt = PromptBuilder::buildSystemPrompt(ctx);
    assert(prompt.find(u8"## 近期记忆") != std::string::npos);
    assert(prompt.find(u8"道侣结缘") != std::string::npos);
    assert(prompt.find(u8"宗门战事") != std::string::npos);
    assert(prompt.find(u8"境界突破") != std::string::npos);

    printf("  PASS: testExtractContextWithMemory\n");
}

// ─── Test: minimal components — graceful degradation ────────────────────────

static void testExtractContextMinimal() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    // Only Identity — no personality, no skills, no career, no memory, no stats
    reg.addComponent<IdentityComponent>(id,
        "ghost-00", "幽灵", AgentRole::Worker);

    AgentContext ctx = PromptBuilder::extractContext(reg, id);

    assert(ctx.name == "幽灵");
    // Personality defaults
    assert(std::abs(ctx.ambition - 50.0f) < 0.01f);
    // No skills
    assert(ctx.topSkills.empty());
    // No career
    assert(ctx.careerStage.empty());
    assert(ctx.totalXp == 0);
    // No memories
    assert(ctx.recentMemories.empty());
    // No stats
    assert(!ctx.hasStats);

    // Build prompt — should not crash, should still be valid
    std::string prompt = PromptBuilder::buildSystemPrompt(ctx);
    assert(prompt.find(u8"幽灵") != std::string::npos);
    assert(prompt.find(u8"## 性格特征") != std::string::npos);
    // Skills, career, memory sections should be omitted
    assert(prompt.find(u8"## 核心技能") == std::string::npos);
    assert(prompt.find(u8"## 近期记忆") == std::string::npos);
    assert(prompt.find(u8"## 战斗属性") == std::string::npos);
    // Career section still appears (totalXp=0, stage empty but the condition
    // is !ctx.careerStage.empty() || ctx.totalXp > 0 — both false)
    assert(prompt.find(u8"## 职业状态") == std::string::npos);

    printf("  PASS: testExtractContextMinimal\n");
}

// ─── Test: skill gameAbility mapping ────────────────────────────────────────

static void testSkillGameAbilityMapping() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    reg.addComponent<IdentityComponent>(id, "dev-01", "码农", AgentRole::Worker);

    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("backend_dev", SkillCategory::Engineering, SkillLevel::Advanced);
    skills.getSkill("backend_dev")->xp = 500;
    skills.addSkill("unknown_skill", SkillCategory::Engineering, SkillLevel::Beginner);
    skills.getSkill("unknown_skill")->xp = 100;

    AgentContext ctx = PromptBuilder::extractContext(reg, id);

    // backend_dev should map to "阵法"
    assert(ctx.topSkills.size() == 2);
    assert(ctx.topSkills[0].gameAbility == u8"阵法");
    // unknown_skill should fall back to the skill id itself
    assert(ctx.topSkills[1].gameAbility == "unknown_skill");

    printf("  PASS: testSkillGameAbilityMapping\n");
}

// ─── Test: career stage conversion ──────────────────────────────────────────

static void testCareerStageConversion() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "x", "x", AgentRole::Worker);

    // Test each career stage
    CareerStage stages[] = {
        CareerStage::Junior, CareerStage::Mid, CareerStage::Senior,
        CareerStage::Lead, CareerStage::Expert
    };
    const char* expected[] = {"Junior", "Mid", "Senior", "Lead", "Expert"};

    for (int i = 0; i < 5; ++i) {
        auto& career = reg.addComponent<CareerComponent>(id);
        career.stage = stages[i];

        AgentContext ctx = PromptBuilder::extractContext(reg, id);
        assert(ctx.careerStage == expected[i]);
    }

    printf("  PASS: testCareerStageConversion\n");
}

// ─── Test: realm level conversion ───────────────────────────────────────────

static void testRealmLevelConversion() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();
    reg.addComponent<IdentityComponent>(id, "x", "x", AgentRole::Worker);

    struct { RealmLevel level; const char* expected; } realms[] = {
        {RealmLevel::Mortal,            u8"凡人"},
        {RealmLevel::QiRefining,        u8"炼气"},
        {RealmLevel::FoundationBuilding, u8"筑基"},
        {RealmLevel::GoldenCore,        u8"金丹"},
        {RealmLevel::YuanInfant,        u8"元婴"},
        {RealmLevel::Transcension,      u8"化神"},
    };

    for (const auto& r : realms) {
        reg.addComponent<StatsComponent>(id, 10, 100, 50, r.level);
        AgentContext ctx = PromptBuilder::extractContext(reg, id);
        assert(ctx.realm == r.expected);
    }

    printf("  PASS: testRealmLevelConversion\n");
}

// ─── Test: trait label thresholds ───────────────────────────────────────────

static void testTraitLabelThresholds() {
    // High ambition (80)
    AgentContext ctx;
    ctx.name = "test";
    ctx.ambition = 80.0f;
    ctx.caution  = 50.0f;
    ctx.loyalty  = 30.0f;

    std::string prompt = PromptBuilder::buildSystemPrompt(ctx);
    // ambition 80 → 高
    assert(prompt.find(u8"野心: 80/100 (高)") != std::string::npos);
    // caution 50 → 中
    assert(prompt.find(u8"谨慎: 50/100 (中)") != std::string::npos);
    // loyalty 30 → 低
    assert(prompt.find(u8"忠诚: 30/100 (低)") != std::string::npos);

    printf("  PASS: testTraitLabelThresholds\n");
}

// ─── Test: full buildMessages with all components ───────────────────────────

static void testBuildMessagesFullEntity() {
    Registry::getInstance().clear();
    auto& reg = Registry::getInstance();

    Entity e = reg.createEntity();
    EntityId id = e.getId();

    // Full setup
    auto& ident = reg.addComponent<IdentityComponent>(id,
        "sage-001", "智者", AgentRole::Lead);
    ident.department  = "research";
    ident.companyRole = "architect";

    reg.addComponent<PersonalityComponent>(id,
        85.0f, 70.0f, 90.0f, 20.0f, 65.0f, 88.0f);

    auto& skills = reg.addComponent<SkillTreeComponent>(id);
    skills.addSkill("architecture", SkillCategory::Engineering, SkillLevel::Expert);
    skills.getSkill("architecture")->xp = 2500;

    auto& career = reg.addComponent<CareerComponent>(id);
    career.stage = CareerStage::Senior;
    career.totalXp = 5500;
    career.tasksCompleted = 20;
    career.tasksSucceeded = 18;

    auto& mem = reg.addComponent<MemoryRingComponent>(id);
    mem.recordMilestone(MilestoneType::MajorCommand, 0, 95);

    auto& stats = reg.addComponent<StatsComponent>(id, 200, 1000, 500, RealmLevel::YuanInfant);
    stats.hp = 950;
    stats.mp = 420;

    auto msgs = PromptBuilder::buildMessages(reg, id, "设计新的微服务架构");

    assert(msgs.size() == 2);
    assert(msgs[0].role == "system");
    assert(msgs[1].role == "user");

    const std::string& sys = msgs[0].content;
    const std::string& usr = msgs[1].content;

    // System prompt should have all sections
    assert(sys.find(u8"智者") != std::string::npos);
    assert(sys.find(u8"research") != std::string::npos);
    assert(sys.find(u8"## 性格特征") != std::string::npos);
    assert(sys.find(u8"## 核心技能") != std::string::npos);
    assert(sys.find(u8"architecture") != std::string::npos);
    assert(sys.find(u8"## 职业状态") != std::string::npos);
    assert(sys.find(u8"Senior") != std::string::npos);
    assert(sys.find(u8"## 战斗属性") != std::string::npos);
    assert(sys.find(u8"元婴") != std::string::npos);
    assert(sys.find(u8"950") != std::string::npos);
    assert(sys.find(u8"## 近期记忆") != std::string::npos);
    assert(sys.find(u8"重大命令") != std::string::npos);

    // User prompt should contain the task
    assert(usr.find(u8"设计新的微服务架构") != std::string::npos);

    printf("  PASS: testBuildMessagesFullEntity\n");
}

// ─── Test runner ────────────────────────────────────────────────────────────

extern void runPromptBuilderTests();

void runPromptBuilderTests() {
    printf("Running PromptBuilder tests...\n");

    testExtractContextFull();
    testBuildSystemPromptContent();
    testBuildMessagesStructure();
    testExtractContextWithStats();
    testExtractContextWithMemory();
    testExtractContextMinimal();
    testSkillGameAbilityMapping();
    testCareerStageConversion();
    testRealmLevelConversion();
    testTraitLabelThresholds();
    testBuildMessagesFullEntity();

    printf("All 11 PromptBuilder tests PASSED.\n");
}
