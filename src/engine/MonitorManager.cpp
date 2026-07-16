#include "MonitorManager.h"

#include <QJsonArray>
#include <QJsonValue>

#include "logger/Logger.h"

namespace etest::engine {

MonitorManager::MonitorManager(QObject* parent)
    : QObject(parent) {}

// ═══════════════════════════════════════════════════════════════════
// loadFromTopology — 从拓扑 JSON 重建查表和树缓存
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::loadFromTopology(const QJsonObject& topologyDoc) {
  lookup_table_.clear();
  tree_cache_.clear();
  buffer_.clear();
  subscribers_.clear();

  QJsonArray monitorsArr = topologyDoc.value(QStringLiteral("monitors")).toArray();
  for (int mi = 0; mi < monitorsArr.size(); ++mi) {
    QJsonObject mobj = monitorsArr[mi].toObject();

    MonitorTreeEntry entry;
    entry.monitorIndex = mi;
    entry.name = mobj.value(QStringLiteral("name")).toString();
    entry.deviceType = mobj.value(QStringLiteral("deviceType")).toString();
    entry.channelCount = mobj.value(QStringLiteral("channelCount")).toInt(1);
    tree_cache_.append(entry);

    QJsonArray tapsArr = mobj.value(QStringLiteral("taps")).toArray();
    for (int ci = 0; ci < tapsArr.size(); ++ci) {
      QJsonObject tapObj = tapsArr[ci].toObject();
      QString deviceId = tapObj.value(QStringLiteral("deviceId")).toString();
      QString devicePort = tapObj.value(QStringLiteral("devicePort")).toString();

      if (deviceId.isEmpty() || devicePort.isEmpty()) {
        LOG_DEBUG("MONITOR", "跳过无效 tap: monitor={} tap={}", mi, ci);
        continue;
      }

      MonitorTapInfo info;
      info.monitorIndex = mi;
      info.channelIndex = ci;
      info.deviceId = deviceId;
      info.devicePort = devicePort;

      auto key = qMakePair(deviceId, devicePort);
      lookup_table_[key].append(info);
    }
  }

  LOG_INFO("MONITOR", "加载 {} 个监听器, {} 个 tap 条目",
           tree_cache_.size(), lookup_table_.size());
}

// ═══════════════════════════════════════════════════════════════════
// subscribe / unsubscribe — 通道订阅
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::subscribe(int monitorIndex, int channelIndex,
                                SampleCallback cb) {
  auto key = qMakePair(monitorIndex, channelIndex);
  subscribers_[key] = std::move(cb);
}

void MonitorManager::unsubscribe(int monitorIndex, int channelIndex) {
  subscribers_.remove(qMakePair(monitorIndex, channelIndex));
}

// ═══════════════════════════════════════════════════════════════════
// onHardwareOpFinished — 查表 + 记录 + 分发
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::onHardwareOpFinished(const QString& deviceId,
                                           const QString& portName,
                                           const QByteArray& rawFrame,
                                           double rawValue,
                                           double engValue) {
  auto key = qMakePair(deviceId, portName);
  auto it = lookup_table_.constFind(key);
  if (it == lookup_table_.constEnd()) {
    return;  // 该设备端口没有挂载监听器
  }

  const QList<MonitorTapInfo>& tapInfos = it.value();
  for (const auto& tapInfo : tapInfos) {
    MonitorSample sample;
    sample.monitorIndex = tapInfo.monitorIndex;
    sample.channelIndex = tapInfo.channelIndex;
    sample.engValue = engValue;
    sample.rawValue = rawValue;
    sample.rawFrame = rawFrame;
    sample.timestamp = QDateTime::currentDateTime();

    buffer_.append(sample);

    // 按通道分发（如果有 subscriber）
    auto subKey = qMakePair(tapInfo.monitorIndex, tapInfo.channelIndex);
    auto subIt = subscribers_.constFind(subKey);
    if (subIt != subscribers_.constEnd() && subIt.value()) {
      subIt.value()(sample);
    }
  }
}

// ═══════════════════════════════════════════════════════════════════
// monitorTree — 返回监听器树状结构
// ═══════════════════════════════════════════════════════════════════
QList<MonitorManager::MonitorTreeEntry> MonitorManager::monitorTree() const {
  return tree_cache_;
}

// ═══════════════════════════════════════════════════════════════════
// flushSamples — 序列化 buffer 为 JSON，清空 buffer
// ═══════════════════════════════════════════════════════════════════
// 输出格式（每个有数据的 monitor 一个条目）：
// [
//   {
//     "name": "...", "deviceType": "...", "channelCount": N,
//     "channels": [
//       {
//         "index": ci,
//         "samples": [
//           { "raw": ..., "eng": ..., "ts": "..." }
//         ]
//       }
//     ]
//   }
// ]
// ═══════════════════════════════════════════════════════════════════
QJsonArray MonitorManager::flushSamples() {
  // 按 (monitorIndex, channelIndex) 分组
  struct ChannelSamples {
    int monitorIndex;
    int channelIndex;
    QList<MonitorSample> samples;
  };
  QHash<int, QHash<int, QList<MonitorSample>>> grouped;

  for (const auto& sample : buffer_) {
    grouped[sample.monitorIndex][sample.channelIndex].append(sample);
  }

  // 构建 JSON：仅输出有数据的 monitor
  QJsonArray monitorsArr;

  for (auto mit = grouped.constBegin(); mit != grouped.constEnd(); ++mit) {
    int mi = mit.key();

    // 从 tree_cache_ 查找 monitor 元信息
    QString name;
    QString deviceType;
    int channelCount = 0;
    for (const auto& entry : tree_cache_) {
      if (entry.monitorIndex == mi) {
        name = entry.name;
        deviceType = entry.deviceType;
        channelCount = entry.channelCount;
        break;
      }
    }

    QJsonObject mObj;
    mObj[QStringLiteral("name")] = name;
    mObj[QStringLiteral("deviceType")] = deviceType;
    mObj[QStringLiteral("channelCount")] = channelCount;

    QJsonArray channelsArr;
    const auto& chMap = mit.value();

    for (auto cit = chMap.constBegin(); cit != chMap.constEnd(); ++cit) {
      int ci = cit.key();
      const auto& samples = cit.value();

      QJsonObject chObj;
      chObj[QStringLiteral("index")] = ci;

      QJsonArray samplesArr;
      for (const auto& sample : samples) {
        QJsonObject sObj;
        sObj[QStringLiteral("eng")] = sample.engValue;
        if (sample.rawFrame.isEmpty()) {
          sObj[QStringLiteral("raw")] = sample.rawValue;
        } else {
          sObj[QStringLiteral("raw")] =
              QString::fromLatin1(sample.rawFrame.toHex(' ').toUpper());
        }
        sObj[QStringLiteral("ts")] = sample.timestamp.toString(Qt::ISODate);
        samplesArr.append(sObj);
      }
      chObj[QStringLiteral("samples")] = samplesArr;
      channelsArr.append(chObj);
    }

    mObj[QStringLiteral("channels")] = channelsArr;
    monitorsArr.append(mObj);
  }

  buffer_.clear();
  return monitorsArr;
}

// ═══════════════════════════════════════════════════════════════════
// clear — 完全重置
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::clear() {
  lookup_table_.clear();
  tree_cache_.clear();
  buffer_.clear();
  subscribers_.clear();
}

}  // namespace etest::engine
