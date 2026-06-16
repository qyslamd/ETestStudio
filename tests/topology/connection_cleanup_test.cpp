#include <gtest/gtest.h>
#include <QPointF>

#include "topology/TopologyDocument.h"
#include "topology/ConnectionCleanup.h"

using namespace etest::topology;

// 辅助：创建一个带端口的基础 UUT
static TopologyProduct makeProduct(const QString& name) {
  TopologyProduct prod;
  prod.name = name;
  prod.position = QPointF(0, 0);
  prod.ports.append({QStringLiteral("Port_IN"),  TopologyPort::Input,
                     {QStringLiteral("A429")}});
  prod.ports.append({QStringLiteral("Port_OUT"), TopologyPort::Output,
                     {QStringLiteral("A429")}});
  return prod;
}

// 辅助：创建一个带端口的基础设备
static TopologyDevice makeDevice(const QString& name,
                                  const QString& deviceType) {
  TopologyDevice dev;
  dev.name = name;
  dev.deviceType = deviceType;
  dev.position = QPointF(0, 0);
  dev.ports.append({QStringLiteral("CH01"), TopologyPort::Output,
                    FunctionType::A429});
  dev.ports.append({QStringLiteral("CH02"), TopologyPort::Input,
                    FunctionType::A429});
  return dev;
}

TEST(ConnectionCleanupTest, NoInvalidConnections) {
  TopologyDocument doc;
  doc.addProduct(makeProduct(QStringLiteral("UUT_1")));
  doc.addDevice(makeDevice(QStringLiteral("DEV_1"), QStringLiteral("EPH6272T")));

  // 添加一条有效连线
  TopologyConnection conn;
  conn.productName = QStringLiteral("UUT_1");
  conn.portName = QStringLiteral("Port_OUT");
  conn.deviceName = QStringLiteral("DEV_1");
  conn.devicePort = QStringLiteral("CH01");
  doc.addConnection(conn);

  auto result = ConnectionCleanup::findInvalid(&doc);
  EXPECT_TRUE(result.isEmpty());
}

TEST(ConnectionCleanupTest, InvalidDevicePort) {
  TopologyDocument doc;
  doc.addProduct(makeProduct(QStringLiteral("UUT_1")));
  doc.addDevice(makeDevice(QStringLiteral("DEV_1"), QStringLiteral("EPH6272T")));

  // 设备端口 CH99 不存在
  TopologyConnection conn;
  conn.productName = QStringLiteral("UUT_1");
  conn.portName = QStringLiteral("Port_OUT");
  conn.deviceName = QStringLiteral("DEV_1");
  conn.devicePort = QStringLiteral("CH99");
  doc.addConnection(conn);

  auto result = ConnectionCleanup::findInvalid(&doc);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].type, InvalidEntry::Connection);
  EXPECT_EQ(result[0].index, 0);
}

TEST(ConnectionCleanupTest, InvalidProductPort) {
  TopologyDocument doc;
  doc.addProduct(makeProduct(QStringLiteral("UUT_1")));
  doc.addDevice(makeDevice(QStringLiteral("DEV_1"), QStringLiteral("EPH6272T")));

  // UUT 端口 Port_XXX 不存在
  TopologyConnection conn;
  conn.productName = QStringLiteral("UUT_1");
  conn.portName = QStringLiteral("Port_XXX");
  conn.deviceName = QStringLiteral("DEV_1");
  conn.devicePort = QStringLiteral("CH01");
  doc.addConnection(conn);

  auto result = ConnectionCleanup::findInvalid(&doc);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].type, InvalidEntry::Connection);
}

TEST(ConnectionCleanupTest, NonExistentProduct) {
  TopologyDocument doc;
  doc.addDevice(makeDevice(QStringLiteral("DEV_1"), QStringLiteral("EPH6272T")));

  // 引用的 UUT 不存在
  TopologyConnection conn;
  conn.productName = QStringLiteral("UUT_MISSING");
  conn.portName = QStringLiteral("Port_OUT");
  conn.deviceName = QStringLiteral("DEV_1");
  conn.devicePort = QStringLiteral("CH01");
  doc.addConnection(conn);

  auto result = ConnectionCleanup::findInvalid(&doc);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].type, InvalidEntry::Connection);
}

TEST(ConnectionCleanupTest, NonExistentDevice) {
  TopologyDocument doc;
  doc.addProduct(makeProduct(QStringLiteral("UUT_1")));

  // 引用的设备不存在
  TopologyConnection conn;
  conn.productName = QStringLiteral("UUT_1");
  conn.portName = QStringLiteral("Port_OUT");
  conn.deviceName = QStringLiteral("DEV_MISSING");
  conn.devicePort = QStringLiteral("CH01");
  doc.addConnection(conn);

  auto result = ConnectionCleanup::findInvalid(&doc);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].type, InvalidEntry::Connection);
}

TEST(ConnectionCleanupTest, MultipleInvalidConnections) {
  TopologyDocument doc;
  doc.addProduct(makeProduct(QStringLiteral("UUT_1")));
  doc.addDevice(makeDevice(QStringLiteral("DEV_1"), QStringLiteral("EPH6272T")));

  // 有效
  TopologyConnection c1;
  c1.productName = QStringLiteral("UUT_1");
  c1.portName = QStringLiteral("Port_OUT");
  c1.deviceName = QStringLiteral("DEV_1");
  c1.devicePort = QStringLiteral("CH01");
  doc.addConnection(c1);

  // 无效：设备端口不存在
  TopologyConnection c2;
  c2.productName = QStringLiteral("UUT_1");
  c2.portName = QStringLiteral("Port_OUT");
  c2.deviceName = QStringLiteral("DEV_1");
  c2.devicePort = QStringLiteral("CH99");
  doc.addConnection(c2);

  // 无效：设备不存在
  TopologyConnection c3;
  c3.productName = QStringLiteral("UUT_1");
  c3.portName = QStringLiteral("Port_OUT");
  c3.deviceName = QStringLiteral("DEV_MISSING");
  c3.devicePort = QStringLiteral("CH01");
  doc.addConnection(c3);

  auto result = ConnectionCleanup::findInvalid(&doc);
  ASSERT_EQ(result.size(), 2);
}

TEST(ConnectionCleanupTest, InvalidMonitorTaps) {
  TopologyDocument doc;
  doc.addProduct(makeProduct(QStringLiteral("UUT_1")));
  doc.addDevice(makeDevice(QStringLiteral("DEV_1"), QStringLiteral("EPH6272T")));

  // 添加有效连线
  TopologyConnection conn;
  conn.productName = QStringLiteral("UUT_1");
  conn.portName = QStringLiteral("Port_OUT");
  conn.deviceName = QStringLiteral("DEV_1");
  conn.devicePort = QStringLiteral("CH01");
  doc.addConnection(conn);

  // 添加监听器，挂载到一个不存在的连线
  TopologyMonitor mon;
  mon.name = QStringLiteral("Mon_1");
  mon.deviceType = QStringLiteral("Monitor");
  doc.addMonitor(mon);

  TopologyMonitorTap tap;
  tap.productName = QStringLiteral("UUT_1");
  tap.portName = QStringLiteral("Port_GHOST");
  tap.deviceName = QStringLiteral("DEV_1");
  tap.devicePort = QStringLiteral("CH01");
  doc.addTap(0, tap);

  auto result = ConnectionCleanup::findInvalid(&doc);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].type, InvalidEntry::MonitorTap);
}

TEST(ConnectionCleanupTest, NullDoc) {
  auto result = ConnectionCleanup::findInvalid(nullptr);
  EXPECT_TRUE(result.isEmpty());
}
