#ifndef ETEST_ENGINE_MONITOR_MANAGER_H_
#define ETEST_ENGINE_MONITOR_MANAGER_H_

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <functional>

namespace etest::engine {

// ── 监听器配置（.etproj monitors 数组每项）──
struct MonitorConfig {
  QString connectionId;  // 拓扑连接 UUID，即监听器 key（一连接一监听器）
  QString name;          // 主标题
  QString displayMode;   // waveform/led/meter/frame/gauge
};

// ── 查表条目：记录哪个监听器挂载在哪个设备端口上 ──
struct MonitorTapInfo {
  QString connectionId;  // 监听器 key = 连接 UUID
  QString deviceId;      // 设备实例 UUID
  QString devicePort;
  QString displayMode;
};

// ── 单次采样记录 ──
struct MonitorSample {
  QString connectionId;  // 监听器 key = 连接 UUID
  double engValue = 0.0;

  // 原始值（二选一，视信号类型使用哪个字段）：
  double rawValue = 0.0;  // AD/DA 单整数
  QByteArray rawFrame;    // CAN/Serial/A429 帧字节

  QDateTime timestamp;
};

class MonitorManager : public QObject {
  Q_OBJECT

 public:
  explicit MonitorManager(QObject* parent = nullptr);
  ~MonitorManager() override;

  // 从项目监听器数组 + 拓扑 JSON 重建查表和树缓存（幂等：开头清结构）
  void loadMonitors(const QJsonArray& monitors, const QJsonObject& topologyDoc);
  // 单条增量添加（执行页配置监听器时调用；一连接一监听器，重复拒绝）
  bool addMonitor(const MonitorConfig& config, const QString& deviceId,
                  const QString& devicePort, const QString& deviceType);
  // 按 connectionId 删除监听器（查表/树/订阅/缓冲/失效标记）
  bool removeMonitor(const QString& connectionId);
  // 修改监听器展示方式（改 tree_cache_ 与 lookup_table_ 对应条目）
  bool setDisplayMode(const QString& connectionId, const QString& displayMode);
  // 修改监听器主标题（改 tree_cache_ 的 name）
  bool renameMonitor(const QString& connectionId, const QString& name);

  using SampleCallback = std::function<void(const MonitorSample&)>;
  void subscribe(const QString& connectionId, SampleCallback cb);
  void unsubscribe(const QString& connectionId);

  void onHardwareOpFinished(const QString& deviceId, const QString& portName,
                            const QByteArray& rawFrame, double rawValue,
                            double engValue);

  struct MonitorTreeEntry {
    QString connectionId;  // key
    QString name;
    QString deviceType;
    QString displayMode;
    bool invalid = false;  // connectionId 在拓扑中不存在
  };
  QList<MonitorTreeEntry> monitorTree() const;

  // 查询某监听器的 displayMode（含失效监听器，供对话框展示）
  QString displayMode(const QString& connectionId) const;

  QJsonArray flushSamples();
  void clearRuntime();
  void clearData();
  void clearStructure();
  void flushNow();

 private:
  void onFlushTimer();

  // key = (deviceId, portName)
  QHash<QPair<QString, QString>, QList<MonitorTapInfo>> lookup_table_;
  // key = connectionId
  QHash<QString, SampleCallback> subscribers_;
  // CVT: key = connectionId
  QHash<QString, MonitorSample> buffer_;
  QList<MonitorSample> history_buffer_;
  QList<MonitorTreeEntry> tree_cache_;
  QSet<QString> invalid_ids_;

  QTimer flush_timer_;
  bool pending_flush_ = false;

  static constexpr int kFlushIntervalMs = 33;
  static constexpr int kMaxHistorySamples = 50000;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_MONITOR_MANAGER_H_
