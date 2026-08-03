#include <gtest/gtest.h>

#include "topology/TopologyCleanupController.h"
#include "topology/TopologyDocument.h"

using namespace etest::topology;

TEST(TopologyCleanupControllerTest, CleanupCommandRemovesInvalidConnections) {
  TopologyDocument doc;
  auto conn = [](const QString& productName) {
    TopologyConnection c;
    c.productName = productName;
    c.portName = QStringLiteral("P");
    c.deviceName = QStringLiteral("D");
    c.devicePort = QStringLiteral("DP");
    return c;
  };
  doc.addConnection(conn(QStringLiteral("P0")));
  doc.addConnection(conn(QStringLiteral("P1")));
  doc.addConnection(conn(QStringLiteral("P2")));

  QVector<InvalidEntry> invalid;
  invalid.append({InvalidEntry::Connection, 1, QStringLiteral("conn 1")});

  doc.undoStack()->push(
      TopologyCleanupController::createCleanupCommand(&doc, invalid));

  ASSERT_EQ(doc.connectionCount(), 2);
  EXPECT_EQ(doc.connection(0)->productName, QStringLiteral("P0"));
  EXPECT_EQ(doc.connection(1)->productName, QStringLiteral("P2"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.connectionCount(), 3);
}
