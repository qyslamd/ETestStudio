#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "topology/TopologyDocument.h"
#include "topology/TopologyJsonSerializer.h"

using namespace etest::topology;

namespace {

TopologyDevice makeDevice() {
  TopologyDevice device;
  device.name = QStringLiteral("EPH5272");
  device.deviceType = QStringLiteral("A429");
  device.pluginId = QStringLiteral("plugin_eph5272");
  TopologyDevicePort port;
  port.name = QStringLiteral("ch0");
  port.direction = TopologyPort::Direction::Output;
  device.ports.append(port);
  return device;
}

}  // namespace

// ══════════════════════════════════════════════════════════════════════════════
//  TopologyDevice.id 自动生成
// ══════════════════════════════════════════════════════════════════════════════

TEST(TopologyM2Test, AddDeviceAutoGeneratesId) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice();
  // id 为空 → addDevice 应自动生成
  EXPECT_TRUE(device.id.isEmpty());
  int idx = doc.addDevice(device);
  ASSERT_GE(idx, 0);
  const auto* dev = doc.device(idx);
  ASSERT_NE(dev, nullptr);
  // id 非空，且符合 UUID v4 格式（8-4-4-4-12 hex 加连字符）
  EXPECT_FALSE(dev->id.isEmpty());
  EXPECT_EQ(dev->id.length(), 36);  // UUID v4 with braces removed
  EXPECT_TRUE(dev->id.contains('-'));
}

TEST(TopologyM2Test, AddDeviceKeepExistingId) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice();
  device.id = QStringLiteral("custom-id-123");
  int idx = doc.addDevice(device);
  const auto* dev = doc.device(idx);
  ASSERT_NE(dev, nullptr);
  EXPECT_EQ(dev->id, "custom-id-123");
}

// ══════════════════════════════════════════════════════════════════════════════
//  findDeviceIndexById
// ══════════════════════════════════════════════════════════════════════════════

TEST(TopologyM2Test, FindDeviceIndexById) {
  TopologyDocument doc;
  TopologyDevice dev1 = makeDevice();
  dev1.id = QStringLiteral("id-001");
  TopologyDevice dev2 = makeDevice();
  dev2.id = QStringLiteral("id-002");
  TopologyDevice dev3 = makeDevice();
  dev3.id = QStringLiteral("id-003");
  doc.addDevice(dev1);
  doc.addDevice(dev2);
  doc.addDevice(dev3);

  EXPECT_EQ(doc.findDeviceIndexById("id-001"), 0);
  EXPECT_EQ(doc.findDeviceIndexById("id-002"), 1);
  EXPECT_EQ(doc.findDeviceIndexById("id-003"), 2);
  // 不存在的 id
  EXPECT_EQ(doc.findDeviceIndexById("non-existent"), -1);
}

// ══════════════════════════════════════════════════════════════════════════════
//  TopologyDevicePort.boundFrameNames
// ══════════════════════════════════════════════════════════════════════════════

TEST(TopologyM2Test, PortBoundFrameNames) {
  TopologyDevice device = makeDevice();
  ASSERT_EQ(device.ports.size(), 1);
  // 初始为空
  EXPECT_TRUE(device.ports[0].boundFrameNames.isEmpty());

  // 写入绑定
  device.ports[0].boundFrameNames
      << QStringLiteral("A429_发送") << QStringLiteral("A429_接收");
  EXPECT_EQ(device.ports[0].boundFrameNames.size(), 2);
  EXPECT_TRUE(device.ports[0].boundFrameNames.contains("A429_发送"));
}

// ══════════════════════════════════════════════════════════════════════════════
//  devicePortAdded / devicePortRemoved 信号
// ══════════════════════════════════════════════════════════════════════════════

TEST(TopologyM2Test, DevicePortAddedSignal) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice();
  int devIdx = doc.addDevice(device);

  int signalDeviceIndex = -1;
  int signalPortIndex = -1;
  QObject::connect(&doc, &TopologyDocument::devicePortAdded,
                   [&](int di, int pi) {
                     signalDeviceIndex = di;
                     signalPortIndex = pi;
                   });

  TopologyDevicePort newPort;
  newPort.name = QStringLiteral("ch1");
  doc.addDevicePort(devIdx, newPort);

  EXPECT_EQ(signalDeviceIndex, devIdx);
  EXPECT_EQ(signalPortIndex, 1);  // 第二个端口
  // 验证端口确实被添加
  const auto* dev = doc.device(devIdx);
  ASSERT_NE(dev, nullptr);
  ASSERT_EQ(dev->ports.size(), 2);
  EXPECT_EQ(dev->ports[1].name, "ch1");
}

TEST(TopologyM2Test, DevicePortRemovedSignal) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice();
  int devIdx = doc.addDevice(device);

  int signalDeviceIndex = -1;
  int signalPortIndex = -1;
  QObject::connect(&doc, &TopologyDocument::devicePortRemoved,
                   [&](int di, int pi) {
                     signalDeviceIndex = di;
                     signalPortIndex = pi;
                   });

  doc.removeDevicePort(devIdx, 0);

  EXPECT_EQ(signalDeviceIndex, devIdx);
  EXPECT_EQ(signalPortIndex, 0);
  const auto* dev = doc.device(devIdx);
  ASSERT_NE(dev, nullptr);
  EXPECT_EQ(dev->ports.size(), 0);
}

// ══════════════════════════════════════════════════════════════════════════════
//  JSON 序列化：id + boundFrames 往返
// ══════════════════════════════════════════════════════════════════════════════

TEST(TopologyM2Test, SerializeDeserializeIdAndBoundFrames) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice();
  device.id = QStringLiteral("550e8400-e29b-41d4-a716-446655440000");
  device.ports[0].boundFrameNames
      << QStringLiteral("A429_发送") << QStringLiteral("A429_接收");
  doc.addDevice(device);

  // 序列化
  QJsonObject json = TopologyJsonSerializer::serialize(doc);
  QJsonArray devicesArr = json["devices"].toArray();
  ASSERT_EQ(devicesArr.size(), 1);
  QJsonObject devObj = devicesArr[0].toObject();
  EXPECT_EQ(devObj["id"].toString(), "550e8400-e29b-41d4-a716-446655440000");

  QJsonArray portsArr = devObj["ports"].toArray();
  ASSERT_EQ(portsArr.size(), 1);
  QJsonObject portObj = portsArr[0].toObject();
  ASSERT_TRUE(portObj.contains("boundFrames"));
  QJsonArray framesArr = portObj["boundFrames"].toArray();
  EXPECT_EQ(framesArr.size(), 2);
  EXPECT_EQ(framesArr[0].toString(), "A429_发送");

  // 反序列化回文档
  TopologyDocument doc2;
  EXPECT_TRUE(TopologyJsonSerializer::deserialize(json, &doc2));
  ASSERT_EQ(doc2.deviceCount(), 1);
  const auto* dev2 = doc2.device(0);
  ASSERT_NE(dev2, nullptr);
  EXPECT_EQ(dev2->id, "550e8400-e29b-41d4-a716-446655440000");
  ASSERT_EQ(dev2->ports.size(), 1);
  ASSERT_EQ(dev2->ports[0].boundFrameNames.size(), 2);
  EXPECT_EQ(dev2->ports[0].boundFrameNames[0], "A429_发送");
}

// ══════════════════════════════════════════════════════════════════════════════
//  老文件迁移：缺 id 时自动生成
// ══════════════════════════════════════════════════════════════════════════════

TEST(TopologyM2Test, DeserializeMissingIdAutoGenerate) {
  // 构造不含 id 的旧格式 JSON
  QJsonObject json;
  json["version"] = 1;
  json["products"] = QJsonArray();
  json["connections"] = QJsonArray();
  json["monitors"] = QJsonArray();

  QJsonObject devObj;
  devObj["name"] = QStringLiteral("EPH5272");
  devObj["deviceType"] = QStringLiteral("A429");
  devObj["pluginId"] = QStringLiteral("plugin_eph5272");
  // 没有 "id" 字段 ← 模拟老文件

  QJsonObject portObj;
  portObj["name"] = QStringLiteral("ch0");
  portObj["direction"] = QStringLiteral("output");
  portObj["functionType"] = QStringLiteral("A429");
  // 没有 "boundFrames" 字段 ← 模拟老文件
  QJsonArray portsArr;
  portsArr.append(portObj);
  devObj["ports"] = portsArr;

  QJsonArray devicesArr;
  devicesArr.append(devObj);
  json["devices"] = devicesArr;

  TopologyDocument doc;
  EXPECT_TRUE(TopologyJsonSerializer::deserialize(json, &doc));
  ASSERT_EQ(doc.deviceCount(), 1);
  const auto* dev = doc.device(0);
  ASSERT_NE(dev, nullptr);
  // id 应自动生成
  EXPECT_FALSE(dev->id.isEmpty());
  EXPECT_EQ(dev->id.length(), 36);
  // boundFrames 缺省 → 空数组
  ASSERT_EQ(dev->ports.size(), 1);
  EXPECT_TRUE(dev->ports[0].boundFrameNames.isEmpty());
  // 文档应被标记 modified（迁移标记）
  // doc.isModified() 验证在集成测试中完成
}
