#include "TopologyCleanupController.h"

#include <QUndoCommand>

#include "TopologyDocument.h"
#include "UndoCommands.h"

namespace etest::topology {

QUndoCommand* TopologyCleanupController::createCleanupCommand(
    TopologyDocument* doc, QVector<InvalidEntry> invalid) {
  ConnectionCleanup::sortForRemoval(&invalid);
  auto* batchCmd = new QUndoCommand(QStringLiteral("清理无效连线"));

  for (const auto& entry : invalid) {
    switch (entry.type) {
      case InvalidEntry::Connection:
        new RemoveConnectionCommand(doc, entry.index, batchCmd);
        break;
      case InvalidEntry::MonitorTap:
        new RemoveMonitorCommand(doc, entry.monIdx, batchCmd);
        // entry.index 无意义（connectionId 模式下仅作索引占位）
        break;
    }
  }

  return batchCmd;
}

}  // namespace etest::topology
