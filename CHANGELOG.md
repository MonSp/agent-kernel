# Changelog

## [0.6.0] - 2026-09-02

### Added

**Layer 5: Agent Tick & Action Effects System**
- `src/ecs/systems/ActionTypes.h`: 7 种行动类型 + 确定性 Decision → ActionType 映射
- `src/ecs/systems/ActionEffect.h`: TargetComponent 枚举 + generateEffects() 效果生成
- `src/ecs/systems/ActionExecutor.h/.cpp`: 将 ActionEffect 应用到 ECS 组件
- `src/ecs/systems/TickEngine.h/.cpp`: 单 Agent tick 循环（感知→LLM决策→执行→效果）
- `src/ecs/systems/SimulationRunner.h/.cpp`: 多 Agent N-tick 批量模拟
- 2 个新 IPC 方法：`agentTick`、`runSimulation`

**Layer 6: EventJournal + AgentMailbox**
- `src/ecs/systems/EventJournal.h`: 线程安全 append-only 事件日志（ring buffer，10000 条容量）
- `src/ecs/systems/AgentMailbox.h`: 线程安全 per-entity FIFO 消息队列（1000 条/entity）
- `src/ecs/systems/EventStreamServer.h`: 专用 Unix socket 事件推送服务器
- 4 个新 IPC 方法：`appendEvent`、`getEvents`、`sendMessage`、`getMessages`
- `sendMessage` 自动记录事件到 EventJournal
- Daemon `--events-socket` 参数启用事件流

**LLM 配置**
- `AgentKernelBridge.h`: `makeLLMConfig()` 从环境变量读取 LLM 配置
- 支持 `LLM_API_KEY`、`LLM_BASE_URL`、`LLM_MODEL` 环境变量
- 无 API key 时回退到 stub 模式

**LLM Prompt 改进**
- `PromptBuilder.h`: prompt 显式列出 5 种有效行动类型（execute/delegate/requestInfo/decline/reflect）
- LLM 正确返回 `delegate` 行动及 `delegateTo` 目标

### Fixed

**线程安全（5 个 Critical）**
- `AgentKernelBridge.h`: 添加 `handlerMutex_` 序列化所有 IPC 处理器调用
- `EventJournal.h`: 所有公共方法添加 `std::mutex` 保护
- `AgentMailbox.h`: 所有公共方法添加 `std::mutex` 保护
- `EventStreamServer.h`: 追踪客户端线程（不再 detach），`stop()` 时 join 所有线程
- `EventStreamServer.h`: `escapeStr` 添加 `\t` 转义

**IPC 安全**
- `AgentKernelBridge.h`: 每个请求创建新的 TickEngine（无共享状态，线程安全）
- `TickEngine.cpp`: `toJson()` 对字符串字段进行 JSON 转义
- `EventStreamServer.h`: `embedPayload()` 正确处理 JSON 对象嵌入（反斜杠解码）

### Test Results

- 内核 C++ 测试: 181/181 通过
- Python 客户端 E2E: 21/21 通过
- TS 客户端: 21/21 通过

## [0.5.0] - 2026-08-31

### Added

**Layer 4: LLM Reasoning Engine**
- `src/llm/HttpClient.h/.cpp`: HTTP POST 客户端（curl + stub 回退）
- `src/llm/LLMClient.h/.cpp`: 多 provider LLM 客户端（OpenAI/DeepSeek/Gemini/Custom）
- `src/llm/PromptBuilder.h`: Agent 状态 → 结构化中文 LLM prompt
- `src/llm/DecisionEngine.h/.cpp`: LLM 调用 → 结构化 Decision（Execute/Delegate/RequestInfo/Decline/Reflect）
- 1 个新 IPC 方法：`agentDecide`
- 17 DecisionEngine 测试 + 11 PromptBuilder 测试 + 15 LLM client 测试

**Layer 3: Ontology Reasoning**
- `src/ecs/EntityArchetype.h`: 实体原型模板系统
- `src/ecs/BuiltinArchetypes.h`: 6 个内置原型（Engineer/Designer/Manager/Warrior/Alchemist/Elder）
- `src/ecs/SchemaValidator.h`: 组件 schema 验证
- 3 个新 IPC 方法：`listArchetypes`、`createFromArchetype`、`validateEntity`

**Layer 2: Dynamic Component Storage**
- `src/ecs/GenericComponentStore.h`: 类型擦除的字节缓冲区存储
- `DynamicComponentRegistry`: 运行时组件注册
- 混合 Registry：9 个硬编码组件 + 动态路径

**Layer 1: Schema Introspection**
- `src/ecs/Schema.h`: FieldDescriptor + ComponentSchema + SchemaRegistry
- `src/ecs/ComponentSchemas.h`: 9 个组件的 schema 注册
- 3 个新 IPC 方法：`getSchemas`、`getSchema`、`describeEntity`

### Test Results

- L1: 46 测试通过
- L2: 63 测试通过
- L3: 82 测试通过
- L4: ~127 测试通过
