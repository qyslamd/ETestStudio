#ifndef ETEST_ENGINE_MONITOR_MANAGER_H_
#define ETEST_ENGINE_MONITOR_MANAGER_H_

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>

namespace etest::engine {

// ── 查表条目：记录哪个监听器挂载在哪个设备端口上 ──
struct MonitorTapInfo {
  int monitorIndex = -1;
  QString deviceId;         // UUID
  QString devicePort;
  QString displayMode;
};

// ── 单次采样记录 ──
struct MonitorSample {
  int monitorIndex = -1;
  double engValue = 0.0;

  // 原始值（二选一，视信号类型使用哪个字段）：
  double rawValue = 0.0;       // AD/DA 单整数
  QByteArray rawFrame;         // CAN/Serial/A429 帧字节

  QDateTime timestamp;
};

class MonitorManager : public QObject {
  Q_OBJECT

 public:
  explicit MonitorManager(QObject* parent = nullptr);
  ~MonitorManager() override;

  void loadFromTopology(const QJsonObject& topologyDoc);
  void appendFromTopology(const QJsonObject& topologyDoc);

  using SampleCallback = std::function<void(const MonitorSample&)>;
  void subscribe(int monitorIndex, SampleCallback cb);
  void unsubscribe(int monitorIndex);

  void onHardwareOpFinished(const QString& deviceId,
                             const QString& portName,
                             const QByteArray& rawFrame,
                             double rawValue,
                             double engValue);

  struct MonitorTreeEntry {
    int monitorIndex;
    QString name;
    QString deviceType;
  };
  QList<MonitorTreeEntry> monitorTree() const;

  // 查询某个监听器的 displayMode
  QString displayMode(int monitorIndex) const;

  QJsonArray flushSamples();
  void clearRuntime();
  void clearData();
  void clearStructure();
  void flushNow();

 private:
  void onFlushTimer();

  // key = (deviceId, portName)
  QHash<QPair<QString, QString>, QList<MonitorTapInfo>> lookup_table_;
  // key = monitorIndex
  QHash<int, SampleCallback> subscribers_;
  // CVT: key = monitorIndex
  QHash<int, MonitorSample> buffer_;
  QList<MonitorSample> history_buffer_;
  QList<MonitorTreeEntry> tree_cache_;

  QTimer flush_timer_;
  bool pending_flush_ = false;

  static constexpr int kFlushIntervalMs = 33;
  static constexpr int kMaxHistorySamples = 50000;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_MONITOR_MANAGER_H_
