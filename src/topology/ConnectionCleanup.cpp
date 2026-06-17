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

  // 检查无效监听器挂载
  for (int mi = 0; mi < doc->monitorCount(); ++mi) {
    const auto* mon = doc->monitor(mi);
    if (!mon) continue;

    // 通道数不足的多余挂载标记为无效（旧的 .etopo 文件可能缺少 channelCount）
    for (int ti = mon->channelCount; ti < mon->taps.size(); ++ti) {
      const auto& tap = mon->taps[ti];
      invalid.append({InvalidEntry::MonitorTap, ti, mi,
          QStringLiteral("挂载: UUT \"%1/%2\" → 设备 \"%3/%4\" (超出通道数)")
              .arg(tap.productName, tap.portName,
                   tap.deviceName, tap.devicePort)});
    }

    // 检查未超限的挂载是否有重复
    int maxTi = qMin(mon->channelCount, mon->taps.size());
    for (int ti = 0; ti < maxTi; ++ti) {
      const auto& tap = mon->taps[ti];
      for (int tj = ti + 1; tj < maxTi; ++tj) {
        const auto& tap2 = mon->taps[tj];
        if (tap.productName == tap2.productName &&
            tap.portName == tap2.portName &&
            tap.deviceName == tap2.deviceName &&
            tap.devicePort == tap2.devicePort) {
          invalid.append({InvalidEntry::MonitorTap, tj, mi,
              QStringLiteral("挂载: UUT \"%1/%2\" → 设备 \"%3/%4\" (重复挂载)")
                  .arg(tap2.productName, tap2.portName,
                       tap2.deviceName, tap2.devicePort)});
          break;
        }
      }
    }

    for (int ti = 0; ti < maxTi; ++ti) {
      const auto& tap = mon->taps[ti];

      bool connFound = false;
      for (int ci = 0; ci < doc->connectionCount(); ++ci) {
        const auto* c = doc->connection(ci);
        if (c && c->productName == tap.productName &&
            c->portName == tap.portName &&
            c->deviceName == tap.deviceName &&
            c->devicePort == tap.devicePort) {
          connFound = true;
          break;
        }
      }
      if (!connFound) {
        invalid.append({InvalidEntry::MonitorTap, ti, mi,
            QStringLiteral("挂载: UUT \"%1/%2\" → 设备 \"%3/%4\" (关联连线已不存在)")
                .arg(tap.productName, tap.portName,
                     tap.deviceName, tap.devicePort)});
      }
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
