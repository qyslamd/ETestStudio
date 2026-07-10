#ifndef ETEST_ENGINE_HARDWARE_MANAGER_H_
#define ETEST_ENGINE_HARDWARE_MANAGER_H_

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>
#include <memory>
#include <stdexcept>
#include <vector>

namespace etest::core::plugin {
class IDevicePlugin;
}  // namespace etest::core::plugin

namespace etest::engine {

class MockUUT;
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

  // Load devices from parsed topology JSON object
  bool loadFromTopology(const QJsonObject& root);

  // Set MockUUT instances from Builder (takes ownership)
  void setMockUUT(std::vector<std::unique_ptr<MockUUT>> uuts);

  // Close all devices and clear the pool (used for rollback on error)
  void closeAllDevices();

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
                         const QVariantMap& properties, bool mock);
  etest::core::plugin::IDevicePlugin* pluginForDevice(
      const QString& deviceId) const;

  MockUUT* findMockUUTForFrame(const QString& deviceId, int frameId) const;

  struct DeviceEntry {
    etest::core::plugin::IDevicePlugin* plugin = nullptr;
    DeviceStatus status = DeviceStatus::Offline;
    bool is_mock = false;
  };
  QMap<QString, DeviceEntry> device_pool_;
  QVector<MockUUT*> mock_uuts_;  // raw pointers into mock_uut_holders_
  std::vector<std::unique_ptr<MockUUT>> mock_uut_holders_;

  // Allow test helper to inject devices for unit testing
  friend class HardwareManagerTestHelper;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_HARDWARE_MANAGER_H_
