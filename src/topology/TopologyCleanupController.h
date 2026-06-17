#pragma once

#include <QVector>

#include "ConnectionCleanup.h"

class QUndoCommand;

namespace etest::topology {

class TopologyDocument;

class TopologyCleanupController {
 public:
  static QUndoCommand* createCleanupCommand(TopologyDocument* doc,
                                            QVector<InvalidEntry> invalid);
};

}  // namespace etest::topology
