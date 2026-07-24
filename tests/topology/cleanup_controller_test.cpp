#include <gtest/gtest.h>

#include "topology/TopologyCleanupController.h"
#include "topology/TopologyDocument.h"

using namespace etest::topology;

TEST(TopologyCleanupControllerTest, CleanupCommandRemovesInvalidConnections) {
  TopologyDocument doc;
  doc.addConnection({QStringLiteral("P0"), QStringLiteral("P"),
                     QStringLiteral("D"), QStringLiteral("DP")});
  doc.addConnection({QStringLiteral("P1"), QStringLiteral("P"),
                     QStringLiteral("D"), QStringLiteral("DP")});
  doc.addConnection({QStringLiteral("P2"), QStringLiteral("P"),
                     QStringLiteral("D"), QStringLiteral("DP")});

  QVector<InvalidEntry> invalid;
  invalid.append({InvalidEntry::Connection, 1, -1, QStringLiteral("conn 1")});

  doc.undoStack()->push(
      TopologyCleanupController::createCleanupCommand(&doc, invalid));

  ASSERT_EQ(doc.connectionCount(), 2);
  EXPECT_EQ(doc.connection(0)->productName, QStringLiteral("P0"));
  EXPECT_EQ(doc.connection(1)->productName, QStringLiteral("P2"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.connectionCount(), 3);
}

TEST(TopologyCleanupControllerTest, CleanupCommandRemovesMonitorWithInvalidConnectionId) {
  TopologyDocument doc;
  doc.addConnection({QStringLiteral("P0"), QStringLiteral("P"),
                     QStringLiteral("D"), QStringLiteral("DP")});

  TopologyMonitor monitor;
  monitor.name = QStringLiteral("Monitor");
  monitor.connectionId = QStringLiteral("non-existent-id");
  doc.addMonitor(monitor);

  QVector<InvalidEntry> invalid;
  // MonitorTap entry now flags monitors with invalid connectionId for removal
  invalid.append({InvalidEntry::MonitorTap, 0, 0, QStringLiteral("invalid connectionId")});

  doc.undoStack()->push(
      TopologyCleanupController::createCleanupCommand(&doc, invalid));

  // Monitor should be removed
  ASSERT_EQ(doc.monitorCount(), 0);

  doc.undoStack()->undo();
  ASSERT_EQ(doc.monitorCount(), 1);
}
