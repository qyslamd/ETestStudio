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
  prod.ports.append({QStringLiteral("Port_IN"),  TopologyPort::Direction::Input,
                     {QStringLiteral("A429")}});
  prod.ports.append({QStringLiteral("Port_OUT"), TopologyPort::Direction::Output,
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
  dev.ports.append({QStringLiteral("CH01"), TopologyPort::Direction::Output,
                    FunctionType::A429});
  dev.ports.append({QStringLiteral("CH02"), TopologyPort::Direction::Input,
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

TEST(ConnectionCleanupTest, InvalidMonitorConnectionId) {
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

  // 添加监听器，connectionId 指向已存在的连线（应视为有效）
  {
    TopologyMonitor mon;
    mon.name = QStringLiteral("Mon_Valid");
    mon.connectionId = conn.id;
    doc.addMonitor(mon);
  }

  // 添加监听器，connectionId 为空（应视为无效）
  {
    TopologyMonitor mon;
    mon.name = QStringLiteral("Mon_EmptyId");
    doc.addMonitor(mon);
  }

  auto result = ConnectionCleanup::findInvalid(&doc);
  ASSERT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].type, InvalidEntry::MonitorTap);
  ASSERT_EQ(result[0].monIdx, 1);
}

TEST(ConnectionCleanupTest, SortForRemovalDeletesUnstableIndexesSafely) {
  QVector<InvalidEntry> entries;
  entries.append({InvalidEntry::MonitorTap, 1, 0, QStringLiteral("tap 1")});
  entries.append({InvalidEntry::Connection, 2, -1, QStringLiteral("conn 2")});
  entries.append({InvalidEntry::MonitorTap, 4, 0, QStringLiteral("tap 4")});
  entries.append({InvalidEntry::Connection, 5, -1, QStringLiteral("conn 5")});
  entries.append({InvalidEntry::MonitorTap, 3, 1, QStringLiteral("tap 3")});

  ConnectionCleanup::sortForRemoval(&entries);

  ASSERT_EQ(entries.size(), 5);
  EXPECT_EQ(entries[0].type, InvalidEntry::Connection);
  EXPECT_EQ(entries[0].index, 5);
  EXPECT_EQ(entries[1].type, InvalidEntry::Connection);
  EXPECT_EQ(entries[1].index, 2);
  EXPECT_EQ(entries[2].type, InvalidEntry::MonitorTap);
  EXPECT_EQ(entries[2].monIdx, 1);
  EXPECT_EQ(entries[2].index, 3);
  EXPECT_EQ(entries[3].type, InvalidEntry::MonitorTap);
  EXPECT_EQ(entries[3].monIdx, 0);
  EXPECT_EQ(entries[3].index, 4);
  EXPECT_EQ(entries[4].type, InvalidEntry::MonitorTap);
  EXPECT_EQ(entries[4].monIdx, 0);
  EXPECT_EQ(entries[4].index, 1);
}

TEST(ConnectionCleanupTest, NullDoc) {
  auto result = ConnectionCleanup::findInvalid(nullptr);
  EXPECT_TRUE(result.isEmpty());
}
