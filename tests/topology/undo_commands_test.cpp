#include <gtest/gtest.h>

#include "topology/TopologyDocument.h"
#include "topology/UndoCommands.h"

using namespace etest::topology;

namespace {

TopologyProduct makeProduct(const QString& name) {
  TopologyProduct product;
  product.name = name;
  TopologyPort port;
  port.name = QStringLiteral("P1");
  product.ports.append(port);
  return product;
}

TopologyDevice makeDevice(const QString& name) {
  TopologyDevice device;
  device.name = name;
  TopologyDevicePort port;
  port.name = QStringLiteral("D1");
  device.ports.append(port);
  return device;
}

TopologyConnection makeConnection(const QString& productName,
                                  const QString& portName,
                                  const QString& deviceName,
                                  const QString& devicePort) {
  TopologyConnection connection;
  connection.productName = productName;
  connection.portName = portName;
  connection.deviceName = deviceName;
  connection.devicePort = devicePort;
  return connection;
}

TopologyMonitorTap makeTap(const QString& productName,
                           const QString& portName,
                           const QString& deviceName,
                           const QString& devicePort) {
  TopologyMonitorTap tap;
  tap.productName = productName;
  tap.portName = portName;
  tap.deviceName = deviceName;
  tap.devicePort = devicePort;
  return tap;
}

TopologyMonitor makeMonitorWithTaps(const QString& name) {
  TopologyMonitor monitor;
  monitor.name = name;
  monitor.taps.append(makeTap(QStringLiteral("P1"), QStringLiteral("P1"),
                              QStringLiteral("D1"), QStringLiteral("D1")));
  monitor.taps.append(makeTap(QStringLiteral("P2"), QStringLiteral("P1"),
                              QStringLiteral("D2"), QStringLiteral("D1")));
  return monitor;
}

}  // namespace

TEST(UndoCommandsTest, RemoveProductRedoDeletesOriginalProductAfterUndo) {
  TopologyDocument doc;
  doc.addProduct(makeProduct(QStringLiteral("A")));
  doc.addProduct(makeProduct(QStringLiteral("B")));

  doc.undoStack()->push(new RemoveProductCommand(&doc, 0));
  ASSERT_EQ(doc.productCount(), 1);
  EXPECT_EQ(doc.product(0)->name, QStringLiteral("B"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.productCount(), 2);

  doc.undoStack()->redo();
  ASSERT_EQ(doc.productCount(), 1);
  EXPECT_EQ(doc.product(0)->name, QStringLiteral("B"));
}

TEST(UndoCommandsTest, RemoveDeviceRedoDeletesOriginalDeviceAfterUndo) {
  TopologyDocument doc;
  doc.addDevice(makeDevice(QStringLiteral("A")));
  doc.addDevice(makeDevice(QStringLiteral("B")));

  doc.undoStack()->push(new RemoveDeviceCommand(&doc, 0));
  ASSERT_EQ(doc.deviceCount(), 1);
  EXPECT_EQ(doc.device(0)->name, QStringLiteral("B"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.deviceCount(), 2);

  doc.undoStack()->redo();
  ASSERT_EQ(doc.deviceCount(), 1);
  EXPECT_EQ(doc.device(0)->name, QStringLiteral("B"));
}

TEST(UndoCommandsTest, RemoveConnectionRedoDeletesOriginalConnectionAfterUndo) {
  TopologyDocument doc;
  doc.addConnection(makeConnection(QStringLiteral("P1"), QStringLiteral("P1"),
                                   QStringLiteral("D1"), QStringLiteral("D1")));
  doc.addConnection(makeConnection(QStringLiteral("P2"), QStringLiteral("P1"),
                                   QStringLiteral("D2"), QStringLiteral("D1")));

  doc.undoStack()->push(new RemoveConnectionCommand(&doc, 0));
  ASSERT_EQ(doc.connectionCount(), 1);
  EXPECT_EQ(doc.connection(0)->productName, QStringLiteral("P2"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.connectionCount(), 2);

  doc.undoStack()->redo();
  ASSERT_EQ(doc.connectionCount(), 1);
  EXPECT_EQ(doc.connection(0)->productName, QStringLiteral("P2"));
}

TEST(UndoCommandsTest, RemoveProductPortRedoDeletesOriginalPortAfterUndo) {
  TopologyDocument doc;
  TopologyProduct product = makeProduct(QStringLiteral("UUT"));
  TopologyPort secondPort;
  secondPort.name = QStringLiteral("P2");
  product.ports.append(secondPort);
  doc.addProduct(product);

  doc.undoStack()->push(new RemoveProductPortCommand(&doc, 0, 0));
  ASSERT_EQ(doc.product(0)->ports.size(), 1);
  EXPECT_EQ(doc.product(0)->ports[0].name, QStringLiteral("P2"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.product(0)->ports.size(), 2);

  doc.undoStack()->redo();
  ASSERT_EQ(doc.product(0)->ports.size(), 1);
  EXPECT_EQ(doc.product(0)->ports[0].name, QStringLiteral("P2"));
}

TEST(UndoCommandsTest, RemoveDevicePortRedoDeletesOriginalPortAfterUndo) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice(QStringLiteral("Device"));
  TopologyDevicePort secondPort;
  secondPort.name = QStringLiteral("D2");
  device.ports.append(secondPort);
  doc.addDevice(device);

  doc.undoStack()->push(new RemoveDevicePortCommand(&doc, 0, 0));
  ASSERT_EQ(doc.device(0)->ports.size(), 1);
  EXPECT_EQ(doc.device(0)->ports[0].name, QStringLiteral("D2"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.device(0)->ports.size(), 2);

  doc.undoStack()->redo();
  ASSERT_EQ(doc.device(0)->ports.size(), 1);
  EXPECT_EQ(doc.device(0)->ports[0].name, QStringLiteral("D2"));
}

TEST(UndoCommandsTest, UnTapRedoDeletesOriginalTapAfterUndo) {
  TopologyDocument doc;
  doc.addMonitor(makeMonitorWithTaps(QStringLiteral("Monitor")));

  doc.undoStack()->push(new UnTapConnectionCommand(&doc, 0, 0));
  ASSERT_EQ(doc.monitor(0)->taps.size(), 1);
  EXPECT_EQ(doc.monitor(0)->taps[0].productName, QStringLiteral("P2"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.monitor(0)->taps.size(), 2);

  doc.undoStack()->redo();
  ASSERT_EQ(doc.monitor(0)->taps.size(), 1);
  EXPECT_EQ(doc.monitor(0)->taps[0].productName, QStringLiteral("P2"));
}
