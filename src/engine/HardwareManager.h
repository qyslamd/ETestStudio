#ifndef ETEST_ENGINE_HARDWARE_MANAGER_H_
#define ETEST_ENGINE_HARDWARE_MANAGER_H_

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <stdexcept>

namespace etest::core::plugin {
class IDevicePlugin;
}  // namespace etest::core::plugin

namespace etest::engine {

struct ResolvedSignal;

enum class DeviceStatus { Online, Offline, Error };

// Custom exception types
class DeviceException : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class TimeoutException : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class HardwareManager : public QObject {
  Q_OBJECT

 public:
  explicit HardwareManager(QObject* parent = nullptr);
  ~HardwareManager() override;

  // Load devices from topology JSON file
  bool loadFromTopology(const QString& etopoPath);

  // Unified read/write interface
  QVariant read(const ResolvedSignal& signal);  // throws DeviceException
  QVariant readAndWait(const ResolvedSignal& signal,
                       int timeoutMs);  // throws TimeoutException
  bool write(const ResolvedSignal& signal,
             double engValue);  // returns false on error
  bool writeFrame(const ResolvedSignal& signal,
                  const QByteArray& frameData);

  // Device status
  DeviceStatus deviceStatus(const QString& deviceId) const;
  QList<QString> onlineDevices() const;

  // Lifecycle
  void shutdown();

 signals:
  void deviceStatusChanged(const QString& deviceId, DeviceStatus status);
  void deviceError(const QString& deviceId, const QString& message);

 private:
  bool instantiateDevice(const QString& deviceId, const QString& pluginId,
                         const QVariantMap& properties);
  etest::core::plugin::IDevicePlugin* pluginForDevice(
      const QString& deviceId) const;

  struct DeviceEntry {
    etest::core::plugin::IDevicePlugin* plugin = nullptr;
    DeviceStatus status = DeviceStatus::Offline;
  };
  QMap<QString, DeviceEntry> device_pool_;

  // Allow test helper to inject devices for unit testing
  friend class HardwareManagerTestHelper;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_HARDWARE_MANAGER_H_
