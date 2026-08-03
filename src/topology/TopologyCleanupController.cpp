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
    }
  }

  return batchCmd;
}

}  // namespace etest::topology
