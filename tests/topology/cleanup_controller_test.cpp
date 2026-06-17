#include <gtest/gtest.h>

#include "topology/TopologyCleanupController.h"
#include "topology/TopologyDocument.h"

using namespace etest::topology;

namespace {

TopologyMonitorTap makeTap(const QString& productName) {
  TopologyMonitorTap tap;
  tap.productName = productName;
  tap.portName = QStringLiteral("P");
  tap.deviceName = QStringLiteral("D");
  tap.devicePort = QStringLiteral("DP");
  return tap;
}

}  // namespace

TEST(TopologyCleanupControllerTest, CleanupCommandRemovesInvalidItemsSafely) {
  TopologyDocument doc;
  doc.addConnection({QStringLiteral("P0"), QStringLiteral("P"),
                     QStringLiteral("D"), QStringLiteral("DP")});
  doc.addConnection({QStringLiteral("P1"), QStringLiteral("P"),
                     QStringLiteral("D"), QStringLiteral("DP")});
  doc.addConnection({QStringLiteral("P2"), QStringLiteral("P"),
                     QStringLiteral("D"), QStringLiteral("DP")});

  TopologyMonitor monitor;
  monitor.name = QStringLiteral("Monitor");
  monitor.taps.append(makeTap(QStringLiteral("T0")));
  monitor.taps.append(makeTap(QStringLiteral("T1")));
  monitor.taps.append(makeTap(QStringLiteral("T2")));
  doc.addMonitor(monitor);

  QVector<InvalidEntry> invalid;
  invalid.append({InvalidEntry::Connection, 1, -1, QStringLiteral("conn 1")});
  invalid.append({InvalidEntry::MonitorTap, 1, 0, QStringLiteral("tap 1")});

  doc.undoStack()->push(
      TopologyCleanupController::createCleanupCommand(&doc, invalid));

  ASSERT_EQ(doc.connectionCount(), 2);
  EXPECT_EQ(doc.connection(0)->productName, QStringLiteral("P0"));
  EXPECT_EQ(doc.connection(1)->productName, QStringLiteral("P2"));
  ASSERT_EQ(doc.monitor(0)->taps.size(), 2);
  EXPECT_EQ(doc.monitor(0)->taps[0].productName, QStringLiteral("T0"));
  EXPECT_EQ(doc.monitor(0)->taps[1].productName, QStringLiteral("T2"));

  doc.undoStack()->undo();
  ASSERT_EQ(doc.connectionCount(), 3);
  ASSERT_EQ(doc.monitor(0)->taps.size(), 3);
}
