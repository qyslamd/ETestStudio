#include "ConnectionCleanup.h"
#include "TopologyDocument.h"

#include <algorithm>

namespace etest::topology {

QVector<InvalidEntry> ConnectionCleanup::findInvalid(
    const TopologyDocument* doc) {
  QVector<InvalidEntry> invalid;

  if (!doc)
    return invalid;

  // 检查无效连线
  for (int i = 0; i < doc->connectionCount(); ++i) {
    const auto* conn = doc->connection(i);
    if (!conn) continue;

    int pi = doc->findProductIndex(conn->productName);
    int di = doc->findDeviceIndex(conn->deviceName);

    if (pi < 0) {
      invalid.append({InvalidEntry::Connection, i, -1,
          QStringLiteral("连线: %1 → 引用的 UUT \"%2\" 不存在")
              .arg(i + 1).arg(conn->productName)});
      continue;
    }
    if (di < 0) {
      invalid.append({InvalidEntry::Connection, i, -1,
          QStringLiteral("连线: %1 → 引用的设备 \"%2\" 不存在")
              .arg(i + 1).arg(conn->deviceName)});
      continue;
    }

    const auto* prod = doc->product(pi);
    const auto* dev = doc->device(di);
    bool portFound = false;
    bool devPortFound = false;
    for (const auto& p : prod->ports)
      if (p.name == conn->portName) { portFound = true; break; }
    for (const auto& dp : dev->ports)
      if (dp.name == conn->devicePort) { devPortFound = true; break; }

    if (!portFound) {
      invalid.append({InvalidEntry::Connection, i, -1,
          QStringLiteral("连线: %1 → UUT \"%2\" 中找不到端口 \"%3\"")
              .arg(i + 1).arg(conn->productName).arg(conn->portName)});
    } else if (!devPortFound) {
      invalid.append({InvalidEntry::Connection, i, -1,
          QStringLiteral("连线: %1 → 设备 \"%2\" 中找不到端口 \"%3\"")
              .arg(i + 1).arg(conn->deviceName).arg(conn->devicePort)});
    }
  }

  // 检查无效监听器挂载（按 connectionId 验证）
  for (int mi = 0; mi < doc->monitorCount(); ++mi) {
    const auto* mon = doc->monitor(mi);
    if (!mon) continue;

    if (mon->connectionId.isEmpty()) {
      invalid.append({InvalidEntry::MonitorTap, 0, mi,
          QStringLiteral("监听器 \"%1\" 未关联任何连线").arg(mon->name)});
      continue;
    }

    bool connFound = false;
    for (int ci = 0; ci < doc->connectionCount(); ++ci) {
      const auto* c = doc->connection(ci);
      if (c && c->id == mon->connectionId) {
        connFound = true;
        break;
      }
    }
    if (!connFound) {
      invalid.append({InvalidEntry::MonitorTap, 0, mi,
          QStringLiteral("监听器 \"%1\" 关联的连线（ID: %2）已不存在")
              .arg(mon->name, mon->connectionId)});
    }
  }

  return invalid;
}

void ConnectionCleanup::sortForRemoval(QVector<InvalidEntry>* entries) {
  if (!entries)
    return;

  std::sort(entries->begin(), entries->end(), [](const InvalidEntry& a,
                                                 const InvalidEntry& b) {
    if (a.type != b.type)
      return a.type == InvalidEntry::Connection;
    if (a.type == InvalidEntry::Connection)
      return a.index > b.index;
    if (a.monIdx != b.monIdx)
      return a.monIdx > b.monIdx;
    return a.index > b.index;
  });
}

}  // namespace etest::topology
