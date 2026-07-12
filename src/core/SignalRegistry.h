#ifndef ETEST_CORE_SIGNAL_REGISTRY_H_
#define ETEST_CORE_SIGNAL_REGISTRY_H_

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace etest::core {

// ── 解析结果：UUID → 4 元组 ──
struct ResolvedSignal {
  QString uuid;        // 32 字符 SHA-1 hex
  QString deviceId;    // 拓扑设备持久 id
  QString deviceName;  // 设备显示名（通过 registerDevice 注册）
  QString deviceType;  // 设备类型（serial/can/a429/ad/da，来自拓扑）
  QString portName;    // 设备端口名
  QString frameName;   // ICD 帧名
  QString nodePath;    // 信号节点路径（frame 内的 / 分隔路径）
};

// ── 批量注册用：4 元组的 key-value 形式 ──
struct SignalEntry {
  QString deviceId;
  QString portName;
  QString frameName;
  QString nodePath;
};

class SignalRegistry : public QObject {
  Q_OBJECT
 public:
  explicit SignalRegistry(QObject* parent = nullptr);

  // ── 设备注册 ──
  void registerDevice(const QString& deviceId, const QString& deviceName,
                      const QString& deviceType = QString());
  QStringList registeredDeviceIds() const;
  QString deviceName(const QString& deviceId) const;
  QString deviceType(const QString& deviceId) const;

  // ── 端口绑定 ──
  void bindPortToFrames(const QString& deviceId, const QString& portName,
                        const QStringList& frameNames);
  void unbindPort(const QString& deviceId, const QString& portName);

  // ── 信号索引填充 ──
  void registerSignals(const QVector<SignalEntry>& entries);

  // ── 暴露端口绑定给上层遍历 ──
  using PortBindingCallback =
      std::function<void(const QString& /*deviceId*/,
                         const QString& /*portName*/,
                         const QStringList& /*frameNames*/)>;
  void forEachPortBinding(PortBindingCallback cb) const;

  // ── UUID 计算（确定性纯函数） ──
  static QString computeUuid(const QString& deviceId, const QString& portName,
                             const QString& frameName,
                             const QString& nodePath);

  // ── 查询 ──
  std::optional<ResolvedSignal> resolve(const QString& uuid) const;
  QString resolveByTuple(const QString& deviceId, const QString& portName,
                         const QString& frameName,
                         const QString& nodePath) const;
  QVector<ResolvedSignal> findByNode(const QString& frameName,
                                     const QString& nodePath) const;
  QVector<ResolvedSignal> findByPort(const QString& deviceId,
                                     const QString& portName) const;

  // ── 清理 ──
  void clearSignals();         // 仅清信号索引，保留设备和端口绑定
  void clear();                // 清空所有

 signals:
  void bindingsChanged();

 private:
  // UUID → 4 元组
  QHash<QString, ResolvedSignal> uuid_index_;
  // (deviceId, portName) → 绑定的帧名列表
  QHash<QPair<QString, QString>, QStringList> port_to_frames_;
  // deviceId → 设备显示名
  QHash<QString, QString> device_names_;
  // deviceId → 设备类型（serial/can/a429/ad/da）
  QHash<QString, QString> device_types_;
  // (frameName, nodePath) → 该路径下的所有 UUID（跨设备）
  QHash<QPair<QString, QString>, QStringList> node_to_uuids_;
};

}  // namespace etest::core

#endif  // ETEST_CORE_SIGNAL_REGISTRY_H_
