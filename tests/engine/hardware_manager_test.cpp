#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QTextStream>

#include "HardwareManager.h"
#include "SignalResolver.h"

#include "plugin/IADevicePlugin.h"
#include "plugin/IArinc429Plugin.h"
#include "plugin/ICANPlugin.h"
#include "plugin/IDADevicePlugin.h"
#include "plugin/IDevicePlugin.h"
#include "plugin/ISerialDevicePlugin.h"
#include "plugin/PluginManager.h"

using namespace etest::engine;
using namespace etest::core::plugin;

// ============================================================================
// Helper: friend access to HardwareManager internal device_pool_.
// Must live in etest::engine namespace to match friend declaration.
// ============================================================================

namespace etest::engine {

class HardwareManagerTestHelper {
 public:
  static void injectDevice(HardwareManager& hm, const QString& deviceId,
                           IDevicePlugin* plugin,
                           etest::engine::DeviceStatus status) {
    HardwareManager::DeviceEntry entry;
    entry.plugin = plugin;
    entry.status = status;
    hm.device_pool_.insert(deviceId, entry);
  }

  static void injectDevice(HardwareManager& hm, const QString& deviceId,
                           IDevicePlugin* plugin) {
    injectDevice(hm, deviceId, plugin,
                 etest::engine::DeviceStatus::Online);
  }
};

}  // namespace etest::engine

using HardwareManagerTestHelper = etest::engine::HardwareManagerTestHelper;

// ============================================================================
// Concrete mock device implementations
//
// IMPORTANT: Each mock class inherits ONLY from its specific plugin interface.
// The plugin interfaces already inherit from IDevicePlugin, so there is
// a single unambiguous inheritance chain — no diamond problem.
// ============================================================================

// --- Mock IDevicePlugin (plain, no subtype) ---
class MockPlainDevice : public IDevicePlugin {
 public:
  bool initialize() override { return true; }
  bool start() override { return true; }
  void stop() override {}
  void uninitialize() override {}
  bool isRunning() const override { return true; }

  PluginMetaData metaData() const override {
    PluginMetaData m;
    m.id = "test.mock.device";
    m.name = "Mock Device";
    m.version = "1.0";
    return m;
  }

  bool openDevice() override {
    opened_ = true;
    return true;
  }
  void closeDevice() override {
    opened_ = false;
    closed_ = true;
  }
  DeviceInfo deviceInfo() const override {
    DeviceInfo info;
    info.model = "MockDevice";
    info.manufacturer = "Test";
    info.channel_count = 8;
    return info;
  }
  ::etest::core::plugin::DeviceStatus deviceStatus() const override {
    return opened_ ? ::etest::core::plugin::DeviceStatus::Online
                   : ::etest::core::plugin::DeviceStatus::Offline;
  }

  bool opened_ = false;
  bool closed_ = false;
};

// --- Mock AD device (IADevicePlugin) ---
class MockADDevice : public IADevicePlugin {
 public:
  // IPlugin
  bool initialize() override { return true; }
  bool start() override { return true; }
  void stop() override {}
  void uninitialize() override {}
  bool isRunning() const override { return true; }
  PluginMetaData metaData() const override {
    PluginMetaData m;
    m.id = "test.mock.ad";
    m.name = "Mock AD";
    m.version = "1.0";
    m.device_type = "ad";
    return m;
  }

  // IDevicePlugin
  bool openDevice() override {
    opened_ = true;
    return true;
  }
  void closeDevice() override {
    opened_ = false;
    closed_ = true;
  }
  DeviceInfo deviceInfo() const override {
    DeviceInfo info;
    info.model = "MockAD";
    info.channel_count = 4;
    return info;
  }
  ::etest::core::plugin::DeviceStatus deviceStatus() const override {
    return opened_ ? ::etest::core::plugin::DeviceStatus::Online
                   : ::etest::core::plugin::DeviceStatus::Offline;
  }

  // IADevicePlugin
  bool setSampleRate(double) override { return true; }
  double sampleRate() const override { return 1000.0; }
  bool setSampleLength(int) override { return true; }
  int sampleLength() const override { return 1024; }
  bool setChannelConfig(int, const ADChannelConfig&) override { return true; }
  ADChannelConfig channelConfig(int) const override { return ADChannelConfig(); }
  bool setTriggerConfig(const ADTriggerConfig&) override { return true; }
  ADTriggerConfig triggerConfig() const override { return ADTriggerConfig(); }
  bool softwareTrigger() override { return true; }
  bool startAcquisition() override { return true; }
  void stopAcquisition() override {}
  bool isAcquiring() const override { return false; }
  ADSampleStatus sampleStatus() const override { return ADSampleStatus::Idle; }
  bool setReadMode(ADReadMode) override { return true; }
  ADReadMode readMode() const override { return ADReadMode::Direct; }
  bool setMemoryMode(ADMemoryMode) override { return true; }
  ADMemoryMode memoryMode() const override { return ADMemoryMode::ChannelStorage; }
  bool setScanList(const QVector<int>&) override { return true; }
  QVector<int> scanList() const override { return {}; }
  int maxScanDepth() const override { return 0; }

  double readChannel(int channel) override {
    read_channel_ = channel;
    return 3.3;
  }
  QVector<double> readAllChannels() override { return {3.3, 3.3, 3.3, 3.3}; }
  QVector<double> readChannelData(int, int) override { return {3.3}; }
  QVector<double> readAllChannelsData(int) override { return {}; }
  QVector<qint16> readChannelRaw(int, int) override { return {}; }
  QVector<qint16> readAllChannelsRaw(int) override { return {}; }

  int read_channel_ = -1;
  bool opened_ = false;
  bool closed_ = false;
};

// --- Mock DA device (IDADevicePlugin) ---
class MockDADevice : public IDADevicePlugin {
 public:
  bool initialize() override { return true; }
  bool start() override { return true; }
  void stop() override {}
  void uninitialize() override {}
  bool isRunning() const override { return true; }
  PluginMetaData metaData() const override {
    PluginMetaData m;
    m.id = "test.mock.da";
    m.name = "Mock DA";
    m.version = "1.0";
    m.device_type = "da";
    return m;
  }
  bool openDevice() override { opened_ = true; return true; }
  void closeDevice() override { opened_ = false; closed_ = true; }
  DeviceInfo deviceInfo() const override {
    DeviceInfo info; info.model = "MockDA"; info.channel_count = 4; return info;
  }
  ::etest::core::plugin::DeviceStatus deviceStatus() const override {
    return opened_ ? ::etest::core::plugin::DeviceStatus::Online
                   : ::etest::core::plugin::DeviceStatus::Offline;
  }

  bool writeChannel(int channel, double value) override {
    last_channel_ = channel;
    last_value_ = value;
    return true;
  }
  double readbackChannel(int channel) const override {
    const_cast<MockDADevice*>(this)->last_channel_ = channel;
    return last_value_;
  }

  int last_channel_ = -1;
  double last_value_ = 0.0;
  bool opened_ = false;
  bool closed_ = false;
};

// --- Mock CAN device (ICANPlugin) ---
class MockCANDevice : public ICANPlugin {
 public:
  bool initialize() override { return true; }
  bool start() override { return true; }
  void stop() override {}
  void uninitialize() override {}
  bool isRunning() const override { return true; }
  PluginMetaData metaData() const override {
    PluginMetaData m;
    m.id = "test.mock.can";
    m.name = "Mock CAN";
    m.version = "1.0";
    m.device_type = "can";
    return m;
  }
  bool openDevice() override { opened_ = true; return true; }
  void closeDevice() override { opened_ = false; closed_ = true; }
  DeviceInfo deviceInfo() const override {
    DeviceInfo info; info.model = "MockCAN"; return info;
  }
  ::etest::core::plugin::DeviceStatus deviceStatus() const override {
    return opened_ ? ::etest::core::plugin::DeviceStatus::Online
                   : ::etest::core::plugin::DeviceStatus::Offline;
  }

  bool sendMessage(quint32 id, const QByteArray& data, bool) override {
    last_id_ = id;
    last_data_ = data;
    return true;
  }
  QByteArray receiveMessage(quint32 id) override {
    last_id_ = id;
    return QByteArray::fromHex("AABBCCDD");
  }
  bool setBitrate(int) override { return true; }
  int bitrate() const override { return 250000; }

  quint32 last_id_ = 0;
  QByteArray last_data_;
  bool opened_ = false;
  bool closed_ = false;
};

// --- Mock Serial device (ISerialDevicePlugin) ---
class MockSerialDevice : public ISerialDevicePlugin {
 public:
  bool initialize() override { return true; }
  bool start() override { return true; }
  void stop() override {}
  void uninitialize() override {}
  bool isRunning() const override { return true; }
  PluginMetaData metaData() const override {
    PluginMetaData m;
    m.id = "test.mock.serial";
    m.name = "Mock Serial";
    m.version = "1.0";
    m.device_type = "serial";
    return m;
  }
  bool openDevice() override { opened_ = true; return true; }
  void closeDevice() override { opened_ = false; closed_ = true; }
  DeviceInfo deviceInfo() const override {
    DeviceInfo info; info.model = "MockSerial"; return info;
  }
  ::etest::core::plugin::DeviceStatus deviceStatus() const override {
    return opened_ ? ::etest::core::plugin::DeviceStatus::Online
                   : ::etest::core::plugin::DeviceStatus::Offline;
  }

  qint64 writeData(const QByteArray& data) override {
    last_data_ = data;
    return data.size();
  }
  QByteArray readData(int) override { return QByteArray::fromHex("11223344"); }
  bool setBaudRate(int) override { return true; }
  int baudRate() const override { return 115200; }
  bool setPortName(const QString&) override { return true; }
  QString portName() const override { return "COM1"; }

  QByteArray last_data_;
  bool opened_ = false;
  bool closed_ = false;
};

// --- Mock A429 device (IArinc429Plugin) ---
class MockA429Device : public IArinc429Plugin {
 public:
  bool initialize() override { return true; }
  bool start() override { return true; }
  void stop() override {}
  void uninitialize() override {}
  bool isRunning() const override { return true; }
  PluginMetaData metaData() const override {
    PluginMetaData m;
    m.id = "test.mock.a429";
    m.name = "Mock A429";
    m.version = "1.0";
    m.device_type = "a429";
    return m;
  }
  bool openDevice() override { opened_ = true; return true; }
  void closeDevice() override { opened_ = false; closed_ = true; }
  DeviceInfo deviceInfo() const override {
    DeviceInfo info; info.model = "MockA429"; return info;
  }
  ::etest::core::plugin::DeviceStatus deviceStatus() const override {
    return opened_ ? ::etest::core::plugin::DeviceStatus::Online
                   : ::etest::core::plugin::DeviceStatus::Offline;
  }

  bool sendLabel(int label, const QByteArray& data) override {
    last_label_ = label;
    last_data_ = data;
    return true;
  }
  QByteArray receiveLabel(int label) override {
    last_label_ = label;
    return QByteArray::fromHex("DEADBEEF");
  }
  bool setSpeed(Arinc429Speed) override { return true; }
  Arinc429Speed speed() const override { return Arinc429Speed::High; }

  int last_label_ = 0;
  QByteArray last_data_;
  bool opened_ = false;
  bool closed_ = false;
};

// ============================================================================
// Helper: create a temporary .etopo JSON file with the given devices array
// ============================================================================

static QJsonObject createTestTopology(const QJsonArray& devices) {
  QJsonObject root;
  root["version"] = 1;
  root["products"] = QJsonArray();
  root["devices"] = devices;
  root["connections"] = QJsonArray();
  root["monitors"] = QJsonArray();
  return root;
}

// ============================================================================
// Test Fixture
// ============================================================================

class HardwareManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    hm_.shutdown();
    // Clean up mock devices AFTER shutdown, so pool pointers stay valid.
    for (auto* d : owned_devices_) {
      delete d;
    }
    owned_devices_.clear();
  }

  // Create a mock device owned by the fixture (lives until TearDown).
  template <typename T>
  T* addDevice() {
    T* dev = new T();
    owned_devices_.push_back(static_cast<IDevicePlugin*>(dev));
    return dev;
  }

  HardwareManager hm_;

 private:
  std::vector<IDevicePlugin*> owned_devices_;
};

// ============================================================================
// Tests
// ============================================================================

// --- Test 1: loadFromTopology with valid JSON but no plugins loaded ---
TEST_F(HardwareManagerTest, LoadFromTopologyWithValidJsonButNoPlugins) {
  QJsonArray devices;
  QJsonObject dev;
  dev["id"] = "dev-001";
  dev["name"] = "TestADCard";
  dev["deviceType"] = "ad";
  dev["pluginId"] = "test.mock.ad";
  dev["positionX"] = 100;
  dev["positionY"] = 200;
  QJsonArray props;
  QJsonObject prop;
  prop["key"] = "sampleRate";
  prop["value"] = "1000";
  props.append(prop);
  dev["properties"] = props;
  dev["ports"] = QJsonArray();
  devices.append(dev);

  QJsonObject root = createTestTopology(devices);
  bool result = hm_.loadFromTopology(root);
  // PluginManager has no plugins loaded, so instantiateDevice fails
  EXPECT_FALSE(result);
}

// --- Test 2: loadFromTopology with empty JSON object ---
TEST_F(HardwareManagerTest, LoadFromTopologyEmptyObject) {
  QJsonObject emptyRoot;
  bool result = hm_.loadFromTopology(emptyRoot);
  EXPECT_FALSE(result);
}

// --- Test 3: loadFromTopology with empty devices array ---
TEST_F(HardwareManagerTest, LoadFromTopologyEmptyDevicesArray) {
  QJsonObject root;
  root["devices"] = QJsonArray();
  root["version"] = 1;
  bool result = hm_.loadFromTopology(root);
  EXPECT_FALSE(result);
}

// --- Test 4: deviceStatus returns Offline for unknown device ---
TEST_F(HardwareManagerTest, DeviceStatusUnknownReturnsOffline) {
  EXPECT_EQ(hm_.deviceStatus("nonexistent-device"),
            etest::engine::DeviceStatus::Offline);
}

// --- Test 5: onlineDevices returns empty list when pool is empty ---
TEST_F(HardwareManagerTest, OnlineDevicesEmptyByDefault) {
  EXPECT_TRUE(hm_.onlineDevices().isEmpty());
}

// --- Test 6: read on unknown device throws DeviceException ---
TEST_F(HardwareManagerTest, ReadUnknownDeviceThrows) {
  ResolvedSignal signal;
  signal.deviceId = "unknown-device";
  signal.signalType = SignalType::AD;
  signal.channel = 0;

  EXPECT_THROW(hm_.read(signal), DeviceException);
}

// --- Test 7: read on offline device throws DeviceException ---
TEST_F(HardwareManagerTest, ReadOfflineDeviceThrows) {
  MockPlainDevice* offlineDev = addDevice<MockPlainDevice>();
  offlineDev->opened_ = false;  // offline

  HardwareManagerTestHelper::injectDevice(hm_, "offline-dev", offlineDev,
                                           etest::engine::DeviceStatus::Offline);

  ResolvedSignal signal;
  signal.deviceId = "offline-dev";
  signal.signalType = SignalType::AD;
  signal.channel = 0;

  EXPECT_THROW(hm_.read(signal), DeviceException);
}

// --- Test 8: write on unknown device returns false ---
TEST_F(HardwareManagerTest, WriteUnknownDeviceReturnsFalse) {
  ResolvedSignal signal;
  signal.deviceId = "unknown-device";
  signal.signalType = SignalType::DA;
  signal.channel = 0;

  EXPECT_FALSE(hm_.write(signal, 5.0));
}

// --- Test 9: shutdown clears the pool ---
TEST_F(HardwareManagerTest, ShutdownClearsPool) {
  MockPlainDevice* dev = addDevice<MockPlainDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "dev-shutdown", dev);

  EXPECT_NO_THROW(hm_.shutdown());
  EXPECT_EQ(hm_.deviceStatus("dev-shutdown"),
            etest::engine::DeviceStatus::Offline);
  EXPECT_TRUE(hm_.onlineDevices().isEmpty());
}

// --- Test 10: shutdown calls closeDevice on each device ---
TEST_F(HardwareManagerTest, ShutdownClosesDevices) {
  MockPlainDevice* dev = addDevice<MockPlainDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "dev-close", dev);

  hm_.shutdown();
  EXPECT_TRUE(dev->closed_);
}

// --- Test 11: read/write round-trip through mock AD device ---
TEST_F(HardwareManagerTest, ReadThroughMockADDevice) {
  MockADDevice* mockAD = addDevice<MockADDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "ad-dev", mockAD);

  ResolvedSignal signal;
  signal.deviceId = "ad-dev";
  signal.signalType = SignalType::AD;
  signal.channel = 2;

  QVariant result = hm_.read(signal);
  EXPECT_DOUBLE_EQ(result.toDouble(), 3.3);
  EXPECT_EQ(mockAD->read_channel_, 2);
}

// --- Test 12: write through mock DA device ---
TEST_F(HardwareManagerTest, WriteThroughMockDADevice) {
  MockDADevice* mockDA = addDevice<MockDADevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "da-dev", mockDA);

  ResolvedSignal signal;
  signal.deviceId = "da-dev";
  signal.signalType = SignalType::DA;
  signal.channel = 1;

  EXPECT_TRUE(hm_.write(signal, 2.5));
  EXPECT_EQ(mockDA->last_channel_, 1);
  EXPECT_DOUBLE_EQ(mockDA->last_value_, 2.5);
}

// --- Test 13: readAndWait delegates to read (Phase 1 mock) ---
TEST_F(HardwareManagerTest, ReadAndWaitDelegatesToRead) {
  MockADDevice* mockAD = addDevice<MockADDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "ad-dev", mockAD);

  ResolvedSignal signal;
  signal.deviceId = "ad-dev";
  signal.signalType = SignalType::AD;
  signal.channel = 0;

  QVariant fromRead = hm_.read(signal);
  QVariant fromReadAndWait = hm_.readAndWait(signal, 1000);

  EXPECT_DOUBLE_EQ(fromReadAndWait.toDouble(), fromRead.toDouble());
  EXPECT_DOUBLE_EQ(fromReadAndWait.toDouble(), 3.3);
}

// --- Test 14: writeFrame through mock CAN device ---
TEST_F(HardwareManagerTest, WriteFrameThroughMockCAN) {
  MockCANDevice* mockCAN = addDevice<MockCANDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "can-dev", mockCAN);

  ResolvedSignal signal;
  signal.deviceId = "can-dev";
  signal.signalType = SignalType::CAN;
  signal.frameId = 0x123;

  QByteArray frameData = QByteArray::fromHex("AABBCCDD");
  EXPECT_TRUE(hm_.writeFrame(signal, frameData));
  EXPECT_EQ(mockCAN->last_id_, 0x123u);
  EXPECT_EQ(mockCAN->last_data_, frameData);
}

// --- Test 15: writeFrame through mock Serial device ---
TEST_F(HardwareManagerTest, WriteFrameThroughMockSerial) {
  MockSerialDevice* mockSerial = addDevice<MockSerialDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "serial-dev", mockSerial);

  ResolvedSignal signal;
  signal.deviceId = "serial-dev";
  signal.signalType = SignalType::SERIAL;

  QByteArray data = QByteArray::fromHex("11223344");
  EXPECT_TRUE(hm_.writeFrame(signal, data));
  EXPECT_EQ(mockSerial->last_data_, data);
}

// --- Test 16: writeFrame through mock A429 device ---
TEST_F(HardwareManagerTest, WriteFrameThroughMockA429) {
  MockA429Device* mockA429 = addDevice<MockA429Device>();
  HardwareManagerTestHelper::injectDevice(hm_, "a429-dev", mockA429);

  ResolvedSignal signal;
  signal.deviceId = "a429-dev";
  signal.signalType = SignalType::A429;
  signal.frameId = 0x3E;  // label 076 in decimal

  QByteArray data = QByteArray::fromHex("DEADBEEF");
  EXPECT_TRUE(hm_.writeFrame(signal, data));
  EXPECT_EQ(mockA429->last_label_, 0x3E);
  EXPECT_EQ(mockA429->last_data_, data);
}

// --- Test 17: write on AD device returns false (input-only) ---
TEST_F(HardwareManagerTest, WriteToADDeviceReturnsFalse) {
  MockADDevice* mockAD = addDevice<MockADDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "ad-dev", mockAD);

  ResolvedSignal signal;
  signal.deviceId = "ad-dev";
  signal.signalType = SignalType::AD;
  signal.channel = 0;

  EXPECT_FALSE(hm_.write(signal, 1.0));
}

// --- Test 18: writeFrame on AD device returns false (not a frame-type) ---
TEST_F(HardwareManagerTest, WriteFrameToADDeviceReturnsFalse) {
  MockADDevice* mockAD = addDevice<MockADDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "ad-dev", mockAD);

  ResolvedSignal signal;
  signal.deviceId = "ad-dev";
  signal.signalType = SignalType::AD;

  EXPECT_FALSE(hm_.writeFrame(signal, QByteArray()));
}

// --- Test 19: deviceStatus returns correct status for injected devices ---
TEST_F(HardwareManagerTest, DeviceStatusAfterInjection) {
  MockPlainDevice* dev1 = addDevice<MockPlainDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "dev-online", dev1);

  EXPECT_EQ(hm_.deviceStatus("dev-online"),
            etest::engine::DeviceStatus::Online);

  MockPlainDevice* dev2 = addDevice<MockPlainDevice>();
  dev2->opened_ = false;
  HardwareManagerTestHelper::injectDevice(hm_, "dev-offline", dev2,
                                           etest::engine::DeviceStatus::Offline);

  EXPECT_EQ(hm_.deviceStatus("dev-offline"),
            etest::engine::DeviceStatus::Offline);
}

// --- Test 20: onlineDevices returns only online devices ---
TEST_F(HardwareManagerTest, OnlineDevicesReturnsCorrectList) {
  MockPlainDevice* dev1 = addDevice<MockPlainDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "dev-A", dev1);

  MockPlainDevice* dev2 = addDevice<MockPlainDevice>();
  dev2->opened_ = false;
  HardwareManagerTestHelper::injectDevice(hm_, "dev-B", dev2,
                                           etest::engine::DeviceStatus::Offline);

  MockPlainDevice* dev3 = addDevice<MockPlainDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "dev-C", dev3);

  QList<QString> online = hm_.onlineDevices();
  EXPECT_EQ(online.size(), 2);
  EXPECT_TRUE(online.contains("dev-A"));
  EXPECT_TRUE(online.contains("dev-C"));
  EXPECT_FALSE(online.contains("dev-B"));
}

// --- Test 21: read on frame-type device (CAN) ---
TEST_F(HardwareManagerTest, ReadThroughMockCANDevice) {
  MockCANDevice* mockCAN = addDevice<MockCANDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "can-dev", mockCAN);

  ResolvedSignal signal;
  signal.deviceId = "can-dev";
  signal.signalType = SignalType::CAN;
  signal.frameId = 0x456;

  QVariant result = hm_.read(signal);
  EXPECT_EQ(result.toByteArray(), QByteArray::fromHex("AABBCCDD"));
  EXPECT_EQ(mockCAN->last_id_, 0x456u);
}

// --- Test 22: read throws when dynamic_cast fails (type mismatch) ---
TEST_F(HardwareManagerTest, ReadTypeMismatchThrows) {
  MockDADevice* mockDA = addDevice<MockDADevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "da-dev", mockDA);

  ResolvedSignal signal;
  signal.deviceId = "da-dev";
  signal.signalType = SignalType::AD;  // AD read on DA-only device
  signal.channel = 0;

  // dynamic_cast<IADevicePlugin*> will fail
  EXPECT_THROW(hm_.read(signal), DeviceException);
}

// --- Test 23: loadFromTopology with empty devices array ---
TEST_F(HardwareManagerTest, LoadFromTopologyEmptyDevices) {
  QJsonArray devices;  // empty
  QJsonObject root = createTestTopology(devices);

  EXPECT_FALSE(hm_.loadFromTopology(root));
}

// --- Test 24: deviceStatusChanged signal emitted on shutdown ---
TEST_F(HardwareManagerTest, DeviceStatusChangedSignalEmitted) {
  MockPlainDevice* mock = addDevice<MockPlainDevice>();

  int signalCount = 0;
  QObject::connect(&hm_, &HardwareManager::deviceStatusChanged,
                   [&](const QString&, etest::engine::DeviceStatus) {
                     ++signalCount;
                   });

  HardwareManagerTestHelper::injectDevice(hm_, "sig-dev", mock);
  hm_.shutdown();

  EXPECT_GE(signalCount, 1);
}

// --- Test 25: multiple shutdown calls are safe (idempotent) ---
TEST_F(HardwareManagerTest, MultipleShutdownIsSafe) {
  MockPlainDevice* dev = addDevice<MockPlainDevice>();
  HardwareManagerTestHelper::injectDevice(hm_, "dev-safe", dev);

  EXPECT_NO_THROW(hm_.shutdown());
  EXPECT_NO_THROW(hm_.shutdown());
  EXPECT_NO_THROW(hm_.shutdown());
}

// --- Test 26: destructor calls shutdown ---
TEST_F(HardwareManagerTest, DestructorCallsShutdown) {
  auto* nestedHm = new HardwareManager();
  MockPlainDevice* dev = addDevice<MockPlainDevice>();

  HardwareManagerTestHelper::injectDevice(*nestedHm, "dev-dtor", dev);
  delete nestedHm;

  EXPECT_TRUE(dev->closed_);
}
