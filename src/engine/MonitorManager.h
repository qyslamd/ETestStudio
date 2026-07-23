#ifndef ETEST_ENGINE_MONITOR_MANAGER_H_
#define ETEST_ENGINE_MONITOR_MANAGER_H_

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QPair>
#include <functional>

namespace etest::engine {

// ── 查表条目：记录哪个监听器的哪个通道挂载在哪个设备端口上 ──
struct MonitorTapInfo {
  int monitorIndex = -1;
  int channelIndex = -1;   // = taps[] 数组下标，非物理通道号
  QString deviceId;         // 来自 tap.deviceId（UUID）
  QString devicePort;       // 来自 tap.devicePort
  QString displayMode;      // 来自 tap.displayMode（waveform/led/meter/frame）
};

// ── 单次采样记录 ──
struct MonitorSample {
  int monitorIndex = -1;
  int channelIndex = -1;
  double engValue = 0.0;       // 工程值

  // 原始值（二选一，视信号类型使用哪个字段）：
  double rawValue = 0.0;       // AD/DA 单整数
  QByteArray rawFrame;         // CAN/Serial/A429 帧字节

  QDateTime timestamp;          // slot 中取 currentDateTime()
};

// ═══════════════════════════════════════════════════════════════════
// MonitorManager — 监听器数据管理
// ═══════════════════════════════════════════════════════════════════
//
// 职责：
// 1. 从拓扑 JSON 加载监听器配置，建立 (deviceId, port) → MonitorTapInfo 查表
// 2. 接收 StepRunner::hardwareOperationFinished 信号，查表匹配后记录采样
// 3. 维护订阅表，按通道分发采样数据到 UI 组件
// 4. 执行完成时将 buffer_ 序列化为 .etlog 的 monitors[] 段
//
// 线程安全：全在主线程访问（信号通过 QueuedConnection 到主线程），无需锁。
// ═══════════════════════════════════════════════════════════════════
class MonitorManager : public QObject {
  Q_OBJECT

 public:
  explicit MonitorManager(QObject* parent = nullptr);

  // ── 加载拓扑文档，重建查表（每次 start() 调用） ──
  // 遍历 monitors[].taps[]，用 tap.deviceId + tap.devicePort 建 lookup_table_
  // 入参为 .etopo JSON 的 root object
  void loadFromTopology(const QJsonObject& topologyDoc);

  // ── 追加拓扑文档的 monitors（累积模式，支持多拓扑合并） ──
  // 不清理已有数据，把入参文档中的 monitors[] 追加到 tree_cache_ 和 lookup_table_
  // monitorIndex 会按当前 tree_cache_ 大小偏移，避免跨拓扑索引冲突
  void appendFromTopology(const QJsonObject& topologyDoc);

  // ── 订阅某个通道的采样通知（主线程同步调用） ──
  using SampleCallback = std::function<void(const MonitorSample&)>;
  void subscribe(int monitorIndex, int channelIndex, SampleCallback cb);
  void unsubscribe(int monitorIndex, int channelIndex);

  // ── 硬件操作完成回调（由 StepRunner 信号触发，主线程执行） ──
  void onHardwareOpFinished(const QString& deviceId,
                             const QString& portName,
                             const QByteArray& rawFrame,
                             double rawValue,
                             double engValue);

  // ── 获取监听器树状结构（供 SignalTreePanel 构建树） ──
  struct MonitorTreeEntry {
    int monitorIndex;
    QString name;
    QString deviceType;
    int channelCount;
  };
  QList<MonitorTreeEntry> monitorTree() const;

  // ── 查询某通道 tap 的 displayMode（供 UI 创建对应可视化组件） ──
  // 找不到返回空字符串，调用方可回退到 "auto"
  QString displayMode(int monitorIndex, int channelIndex) const;

  // ── 获取缓冲数据用于 .etlog 序列化（主线程调用） ──
  // 返回 JSON array，格式见方案 2.3
  QJsonArray flushSamples();

  // ── 清空运行时数据（保留结构和订阅） ──
  void clearRuntime();

  // ── 清空结构和订阅（保留运行时数据） ──
  void clearStructure();

 private:
  // key = (deviceId, portName)
  QHash<QPair<QString, QString>, QList<MonitorTapInfo>> lookup_table_;
  // key = (monitorIndex, channelIndex)
  QHash<QPair<int, int>, SampleCallback> subscribers_;
  QList<MonitorSample> buffer_;
  QList<MonitorTreeEntry> tree_cache_;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_MONITOR_MANAGER_H_
