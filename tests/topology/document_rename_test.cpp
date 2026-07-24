#include <gtest/gtest.h>

#include "topology/TopologyDocument.h"

using namespace etest::topology;

namespace {

TopologyProduct makeProduct() {
  TopologyProduct product;
  product.name = QStringLiteral("OldProduct");
  TopologyPort port;
  port.name = QStringLiteral("OldPort");
  port.allowedDeviceTypes << QStringLiteral("DeviceType");
  product.ports.append(port);
  return product;
}

TopologyDevice makeDevice() {
  TopologyDevice device;
  device.name = QStringLiteral("OldDevice");
  device.deviceType = QStringLiteral("DeviceType");
  TopologyDevicePort port;
  port.name = QStringLiteral("OldDevicePort");
  port.direction = TopologyPort::Direction::Output;
  device.ports.append(port);
  return device;
}

TopologyConnection makeConnection() {
  TopologyConnection connection;
  connection.productName = QStringLiteral("OldProduct");
  connection.portName = QStringLiteral("OldPort");
  connection.deviceName = QStringLiteral("OldDevice");
  connection.devicePort = QStringLiteral("OldDevicePort");
  return connection;
}

void populateConnectedDocument(TopologyDocument* doc) {
  doc->addProduct(makeProduct());
  doc->addDevice(makeDevice());
  doc->addConnection(makeConnection());
}

}  // namespace

TEST(DocumentRenameTest, RenameProductUpdatesConnections) {
  TopologyDocument doc;
  populateConnectedDocument(&doc);

  ASSERT_TRUE(doc.renameProduct(0, QStringLiteral("NewProduct")));

  EXPECT_EQ(doc.product(0)->name, QStringLiteral("NewProduct"));
  EXPECT_EQ(doc.connection(0)->productName, QStringLiteral("NewProduct"));
}

TEST(DocumentRenameTest, RenameProductPortUpdatesConnections) {
  TopologyDocument doc;
  populateConnectedDocument(&doc);

  ASSERT_TRUE(doc.renameProductPort(0, 0, QStringLiteral("NewPort")));

  EXPECT_EQ(doc.product(0)->ports[0].name, QStringLiteral("NewPort"));
  EXPECT_EQ(doc.connection(0)->portName, QStringLiteral("NewPort"));
}

TEST(DocumentRenameTest, RenameDeviceUpdatesConnections) {
  TopologyDocument doc;
  populateConnectedDocument(&doc);

  ASSERT_TRUE(doc.renameDevice(0, QStringLiteral("NewDevice")));

  EXPECT_EQ(doc.device(0)->name, QStringLiteral("NewDevice"));
  EXPECT_EQ(doc.connection(0)->deviceName, QStringLiteral("NewDevice"));
}

TEST(DocumentRenameTest, RenameDevicePortUpdatesConnections) {
  TopologyDocument doc;
  populateConnectedDocument(&doc);

  ASSERT_TRUE(doc.renameDevicePort(0, 0, QStringLiteral("NewDevicePort")));

  EXPECT_EQ(doc.device(0)->ports[0].name, QStringLiteral("NewDevicePort"));
  EXPECT_EQ(doc.connection(0)->devicePort, QStringLiteral("NewDevicePort"));
}

TEST(DocumentRenameTest, RenameRejectsInvalidIndexes) {
  TopologyDocument doc;
  populateConnectedDocument(&doc);

  EXPECT_FALSE(doc.renameProduct(-1, QStringLiteral("X")));
  EXPECT_FALSE(doc.renameProductPort(0, -1, QStringLiteral("X")));
  EXPECT_FALSE(doc.renameDevice(99, QStringLiteral("X")));
  EXPECT_FALSE(doc.renameDevicePort(0, 99, QStringLiteral("X")));
}
