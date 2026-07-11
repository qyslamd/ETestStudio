#include "SignalSyncHelper.h"

#include <QStringList>

#include "core/SignalRegistry.h"
#include "icd/node.hpp"
#include "icd/repository.hpp"

#include "logger/Logger.h"

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
          std::string nameUtf8 = frameName.toStdString();
          const auto* frame = repo->find(nameUtf8);
          if (!frame) {
            // 列举仓库中所有帧的名称和 hex 编码，帮助诊断编码不匹配
            QStringList allNames;
            for (const auto& f : repo->frames()) {
              if (!f) continue;
              allNames << QString::fromUtf8(f->name().data(),
                                            static_cast<int>(f->name().size()));
            }
            LOG_WARN("UUID",
                     "synchronizeRegistry: frame '{}' not found. Repo has {} frames: [{}]",
                     frameName.toStdString(),
                     repo->frames().size(), allNames.join(", ").toStdString());
            continue;
          }
          // Frame::nodes() 返回 flat 列表，覆盖所有节点（含深层子节点）
          for (const auto* node : frame->nodes()) {
            QString nodePath = buildNodePath(node);
            entries.push_back(
                {deviceId, portName, frameName, nodePath});
          }
        }
      });

  registry.registerSignals(entries);

  LOG_DEBUG("UUID", "synchronizeRegistry: {} signals from {} frames across {} port bindings",
            entries.size(),
            repo ? repo->frames().size() : 0,
            [&registry]() {
              int count = 0;
              registry.forEachPortBinding(
                  [&count](const QString&, const QString&, const QStringList&) {
                    ++count;
                  });
              return count;
            }());
}

}  // namespace etest::app
