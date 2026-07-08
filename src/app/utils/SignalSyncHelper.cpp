#include "SignalSyncHelper.h"

#include <QStringList>

#include "core/SignalRegistry.h"
#include "icd/node.hpp"
#include "icd/repository.hpp"

namespace etest::app {

QString buildNodePath(const icd::Node* node) {
  QStringList segments;
  const icd::Node* cur = node;
  while (cur) {
    segments.prepend(QString::fromUtf8(cur->name().data(),
                                       static_cast<int>(cur->name().size())));
    cur = cur->parent();
  }
  return segments.join('/');
}

void synchronizeRegistry(etest::core::SignalRegistry& registry,
                         const icd::Repository* repo) {
  if (!repo) return;

  // 全量重建前先清旧信号索引，保证幂等
  registry.clearSignals();

  QVector<etest::core::SignalEntry> entries;

  registry.forEachPortBinding(
      [&](const QString& deviceId, const QString& portName,
          const QStringList& frameNames) {
        for (const QString& frameName : frameNames) {
          const auto* frame = repo->find(frameName.toStdString());
          if (!frame) continue;
          // Frame::nodes() 返回 flat 列表，覆盖所有节点（含深层子节点）
          for (const auto* node : frame->nodes()) {
            QString nodePath = buildNodePath(node);
            entries.push_back(
                {deviceId, portName, frameName, nodePath});
          }
        }
      });

  registry.registerSignals(entries);
}

}  // namespace etest::app
