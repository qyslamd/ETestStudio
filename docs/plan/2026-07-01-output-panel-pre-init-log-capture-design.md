# OutputPanel 启动前日志回放 设计规格

## 背景

`OutputPanel`（`src/app/widgets/OutputPanel.h`）通过 `QtConsoleSink::logMessage` 信号把 spdlog 输出实时显示到 UI。但 `Logger::init()`（`main.cpp:45`）创建 `QtConsoleSink` 之后，到 `OutputPanel` 构造并 connect 信号（`main_window.cpp:812-815`）之前这段时间内 emit 的日志全部丢失。`Logger::init()` 之前的 LOG 调用本身因 `s_initialized=false` 已经 noop（保持现状）。

丢失的日志包括：
- `LOG_INFO("MAIN", "...")` 应用启动路径
- `GlobalExceptionHandler::init()` 期间
- 翻译加载
- `CrashHandler::init()` 期间
- `MainWindow` 构造期间

让 OutputPanel 能完整看到从 `Logger::init()` 起的所有日志，是改善排错体验的低成本增强。

## 架构

```
[spdlog async logger]
        │ sink_it_  （spdlog 工作线程）
        ▼
[QtConsoleSink] ──push──▶ [LogHistoryBuffer]（环形 5000，QMutex 保护）
        │ emit logMessage
        ▼
[OutputPanel] ◀── drain / emit drained (QueuedConnection) ──┘
```

### 数据流

1. `Logger::init()` 创建一个 5000 容量的 `LogHistoryBuffer`
2. spdlog 工作线程 sink 日志：`QtConsoleSink` 先 `history->push(level, text)`，再 `emit logMessage`
3. `MainWindow` → `OutputPanel` 构造：在 `setupUi()` 末尾 `connect(history, &LogHistoryBuffer::drained, this, &OutputPanel::onHistoricalLogs)`（AutoConnection，同线程 → Direct），再调 `Logger::qtHistoryBuffer()->drain(this)`，emit `drained` 同步触发 `onHistoricalLogs` 槽把历史追加到 QTextEdit
4. `main_window.cpp` 接着 `connect(qtSink, &QtConsoleSink::logMessage, panel, &OutputPanel::appendLog)`：spdlog worker 线程 → UI 线程，跨线程 → Qt 默认 QueuedConnection，新日志入队到 UI 事件循环
5. 因 drain 在 connect 之前同步完成，历史先于实时进入 QTextEdit；后续实时日志按 spdlog 时间顺序入队追加

## 组件

### 新增 `LogEntry`（POD 结构，定义在 `LogHistoryBuffer.h`）

```cpp
struct LogEntry {
    int level;       // spdlog::level::level_enum 强转 int
    QString text;    // QtConsoleSink 格式化后文本
};
```

### 新增 `LogHistoryBuffer`（`src/core/logger/LogHistoryBuffer.h/cpp`）

```cpp
class LogHistoryBuffer : public QObject {
    Q_OBJECT
 public:
    explicit LogHistoryBuffer(int capacity = 5000, QObject* parent = nullptr);
    ~LogHistoryBuffer() override;

    // spdlog 工作线程调用：写入一条历史。容量满则丢最老。
    void push(int level, const QString& text);

    // UI 线程调用：把当前所有历史快照通过 drained 信号一次性发给 panel
    // 调一次后置 drained_=true，再次调用为 noop。
    void drain(QObject* receiver);

 signals:
    void drained(const QList<LogEntry>& entries);

 private:
    QMutex mutex_;
    std::deque<LogEntry> buffer_;
    int capacity_;
    bool drained_ = false;
};
```

- `push`：持锁 → 若 `buffer_.size() >= capacity_` 则 `pop_front` → `push_back`
- `drain`：持锁 → 拷贝当前 `buffer_` 快照为 `QList<LogEntry>` → 锁内置 `drained_=true` → 锁外 `QPointer<QObject> guard(receiver); if (guard) emit drained(snapshot);`（receiver 失效则不发信号；UI 线程同线程 connect 默认 Direct，槽同步执行）
- 用信号而非直调，便于测试和单一职责

### 改动 `QtConsoleSink`（`src/core/logger/QtConsoleSink.h/cpp`）

构造签名加可选参数：
```cpp
explicit QtConsoleSink(LogHistoryBuffer* history = nullptr,
                       QObject* parent = nullptr);
```

`sink_it_` 在 emit 之前先 `push`：
```cpp
void QtConsoleSink::sink_it_(const spdlog::details::log_msg& msg) {
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    QString text = QString::fromUtf8(formatted.data(),
                                     static_cast<int>(formatted.size()));
    if (history_) {
        history_->push(static_cast<int>(msg.level), text);
    }
    emit logMessage(static_cast<int>(msg.level), text);
}
```

历史与实时走的是同一条 emit 信号，`OutputPanel::appendLog` 不感知差异。

### 改动 `Logger`（`src/core/logger/Logger.h/cpp`）

`Logger.h`：
- 新增静态字段 `static LogHistoryBuffer* s_historyBuffer`
- 新增静态方法 `static LogHistoryBuffer* qtHistoryBuffer();`

`Logger::init()`：
- 在创建 `QtConsoleSink` 之前先 `auto* hist = new LogHistoryBuffer(5000);`（`Logger` 拥有所有权）
- 创建 `QtConsoleSink` 时把指针传入：`auto qtSink = std::make_shared<QtConsoleSink>(hist);`
- 保存 `s_historyBuffer = hist;`

`Logger::shutdown()`：
- `delete s_historyBuffer; s_historyBuffer = nullptr;` 放在 `spdlog::shutdown()` 之前

`Logger::qtHistoryBuffer()`：直接返回 `s_historyBuffer`

### 改动 `OutputPanel`（`src/app/widgets/OutputPanel.h/cpp`）

`OutputPanel.h` 新增私有槽：
```cpp
private slots:
  void onHistoricalLogs(const QList<etest::core::logger::LogEntry>& entries);
```

`OutputPanel.cpp` 构造：在 `setupUi()` 末尾追加：
```cpp
if (auto* hist = etest::core::logger::Logger::qtHistoryBuffer()) {
    connect(hist, &etest::core::logger::LogHistoryBuffer::drained,
            this, &OutputPanel::onHistoricalLogs);
    hist->drain(this);
}
```

`onHistoricalLogs` 实现：循环 entries 调 `appendLog(e.level, e.text)`。

注意：connect + drain 都在 `setupUi()` 内部、`main_window.cpp` connect 之前调用，确保历史先入队、实时后 connect。

## 关键决策

| 决策 | 理由 |
|------|------|
| 容量固定 5000 条 | 与 `OutputPanel::kMaxLines = 5000` 对齐 |
| ring buffer 丢最老 | 启动期日志量小，不会真的丢；最坏情况下保证不丢最新 |
| drain 一次性 | `drained_` 标志避免重复回放（已构造的 OutputPanel 不应再收到历史） |
| 历史与实时顺序保证 | drain 在 main_window.cpp connect 之前同步完成；后续实时 logMessage 跨线程 QueuedConnection 按 FIFO 入队 |
| `Logger::init()` 之前仍 noop | LOG_* 宏未改动，影响面最小 |
| `LogHistoryBuffer` 由 `Logger` 拥有 | 单一所有者；与 `QtConsoleSink` 弱引用关系（sink 不持所有权） |
| `drained` 信号而非直调 | 单一职责 + 易测试（mock receiver 直接观察 signal payload） |
| `drained` 投递用 `QPointer` 守 receiver | 防御 receiver 提前析构 |

## 错误处理

- **receiver 已销毁**：`drain` 内部用 `QPointer<QObject>` 守 receiver，失效则不发信号；已 connect 的 receiver 在 `QObject` 析构时由 Qt 自动 disconnect
- **Logger 未 init**：`Logger::qtHistoryBuffer()` 返回 nullptr，`OutputPanel` 构造内 nullptr 检查跳过
- **drain 在非 UI 线程调用**：要求 `drained` 信号在 UI 线程触发（Direct 同线程），所以 `drain` 必须在 UI 线程调用；调用方需保证在 UI 线程发起 drain
- **多线程 push/drain 竞争**：`QMutex` 保护 `buffer_` 和 `drained_` 标志

## 线程安全

| 操作 | 线程 | 同步 |
|------|------|------|
| `LogHistoryBuffer::push` | spdlog 工作线程 | `mutex_` |
| `LogHistoryBuffer::drain` | UI 线程 | `mutex_` |
| `drained_` 读写 | 跨线程 | 锁内 |
| `history_` 指针（QtConsoleSink 持有） | Logger init 时设置，运行时只读 | 无需同步 |

## 文件改动清单

### 新增

- `src/core/logger/LogHistoryBuffer.h`
- `src/core/logger/LogHistoryBuffer.cpp`

### 修改

- `src/core/logger/QtConsoleSink.h` — 加 history 参数、成员
- `src/core/logger/QtConsoleSink.cpp` — sink_it_ 中 push
- `src/core/logger/Logger.h` — 加 s_historyBuffer 字段、qtHistoryBuffer() 方法
- `src/core/logger/Logger.cpp` — init/shutdown 中管理 history
- `src/app/widgets/OutputPanel.h` — 新增 `onHistoricalLogs` 私有槽
- `src/app/widgets/OutputPanel.cpp` — 构造内 connect + drain，实现 `onHistoricalLogs`
- `src/core/CMakeLists.txt` — SOURCES 追加 `logger/LogHistoryBuffer.cpp`、HEADERS 追加 `logger/LogHistoryBuffer.h`
- `tests/core/logger_test.cpp` — 扩展：新增 `LogHistoryBuffer` 相关 TEST_F

`tests/core/CMakeLists.txt` 不需改（沿用现有 `test_core_logger` target）。

## 测试

### 单元测试（扩展 `tests/core/logger_test.cpp`）

新增 `LogHistoryBufferTest` 测试夹具，沿用现有 `QCoreApplication` 单例（事件循环可用）。

- **容量上限**：连续 push 6000 条，buffer 容量保持 5000，最早的 1000 条被丢弃（用 `QSignalSpy` 监听 `drained` 信号，验证收到的 entries 数量与内容）
- **drain 一次**：push 100 条 → 创建 mock receiver，connect `drained` → 调 `drain(mock)`（DirectConnection 同步触发槽）→ 验证 mock 收到 100 条且顺序正确
- **drain 多次幂等**：drain 一次后再调一次，第二次不再 emit
- **drain 防御 receiver 销毁**：传入 `nullptr` 或已析构的 `QObject*`，不崩溃
- **drain 不修改 buffer**：drain 一次后再次 push 200 条，buffer 持有新 200 条（drain 只复制快照，不清空）
- **线程安全**：4 线程并发 push 各 1000 条，最终 buffer 容量 5000 且总数 4000 中的最后 5000 条
- **push + drain 并发**：drain 持锁拷贝快照，drain 期间 push 不影响快照

测试使用 googletest 1.17.0 + Qt5::Test。

### 手动验证

1. 构建 Debug 版（`scripts/build_ninja.bat`）
2. 启动应用，OutputPanel 出现时立即能看到：
   - "日志系统初始化完成，当前日志级别: ..."
   - "全局配置管理模块初始化完成"
   - "崩溃捕获模块初始化完成"
   - 任何 `MainWindow` 构造期间的日志
3. 后续实时日志持续追加，时间顺序连贯
4. 启动后立即快速产生 5000+ 条日志（开发可用 `LOG_INFO` 循环），验证 ring buffer 行为（不丢最新、不超容量）
5. 关闭并重启应用，验证无崩溃（history 在 Logger::shutdown 中正确释放）

## 范围限定

不在本次范围：
- 改动 LOG_* 宏使其在 `Logger::init()` 之前也能记录
- 改 `OutputPanel::kMaxLines = 5000` 的 UI 行数限制
- 把历史日志落盘（spdlog 文件 sink 已独立承担持久化）
- 任何 QtConsoleSink 信号签名变更（保持 `void logMessage(int level, const QString& formattedText)`）
- 任何 OutputPanel::appendLog 行为变更
