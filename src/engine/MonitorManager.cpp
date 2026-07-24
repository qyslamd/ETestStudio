#include "MonitorManager.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonValue>

#include "logger/Logger.h"

namespace etest::engine {

MonitorManager::MonitorManager(QObject* parent)
    : QObject(parent) {
  // 30fps 定时器：批量把 CVT buffer_ 最新值推给 subscribers_，避免逐点 QueuedConnection 积压
  flush_timer_.setInterval(kFlushIntervalMs);
  connect(&flush_timer_, &QTimer::timeout, this, &MonitorManager::onFlushTimer);
  // 仅在有事件循环的环境启动定时器（测试环境无 QCoreApplication 时跳过，避免告警）
  if (QCoreApplication::instance()) {
    flush_timer_.start();
  }
}

MonitorManager::~MonitorManager() {
  flush_timer_.stop();
}

// ═══════════════════════════════════════════════════════════════════
// loadFromTopology — 从拓扑 JSON 重建查表和树缓存
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::loadFromTopology(const QJsonObject& topologyDoc) {
  clearStructure();
  clearRuntime();

  appendFromTopology(topologyDoc);
}

// ═══════════════════════════════════════════════════════════════════
// appendFromTopology - 追加拓扑中的 monitors（累积模式）
// ═══════════════════════════════════════════════════════════════════
// 不清理已有数据。monitorIndex 按当前 tree_cache_ 大小偏移，
// 确保多拓扑合并时索引不冲突。channelIndex 保持拓扑内原值。
void MonitorManager::appendFromTopology(const QJsonObject& topologyDoc) {
  int indexOffset = tree_cache_.size();

  // 建立 deviceName -> deviceId / deviceType 映射
  QHash<QString, QString> nameToId;
  QHash<QString, QString> nameToType;
  QJsonArray devicesArr = topologyDoc.value(QStringLiteral("devices")).toArray();
  for (const auto& dv : devicesArr) {
    QJsonObject dobj = dv.toObject();
    QString id = dobj.value(QStringLiteral("id")).toString();
    QString name = dobj.value(QStringLiteral("name")).toString();
    if (!id.isEmpty() && !name.isEmpty()) {
      nameToId.insert(name, id);
      nameToType.insert(name, dobj.value(QStringLiteral("deviceType")).toString());
    }
  }

  // 建立 connectionId -> (deviceName, devicePort) 映射
  QHash<QString, QPair<QString, QString>> connIdToNamePort;
  QJsonArray connsArr = topologyDoc.value(QStringLiteral("connections")).toArray();
  for (const auto& cv : connsArr) {
    QJsonObject cobj = cv.toObject();
    QString cid = cobj.value(QStringLiteral("id")).toString();
    if (!cid.isEmpty()) {
      connIdToNamePort.insert(cid,
          qMakePair(cobj.value(QStringLiteral("device")).toString(),
                    cobj.value(QStringLiteral("devicePort")).toString()));
    }
  }

  QJsonArray monitorsArr = topologyDoc.value(QStringLiteral("monitors")).toArray();
  for (int mi = 0; mi < monitorsArr.size(); ++mi) {
    QJsonObject mobj = monitorsArr[mi].toObject();

    int globalIndex = indexOffset + mi;
    MonitorTreeEntry entry;
    entry.monitorIndex = globalIndex;
    entry.name = mobj.value(QStringLiteral("name")).toString();

    // 新格式（connectionId）或旧格式（taps）解析
    QString connectionId = mobj.value(QStringLiteral("connectionId")).toString();
    // 从 connectionId 派生 tap
    auto connIt = connIdToNamePort.constFind(connectionId);
    if (connIt != connIdToNamePort.constEnd()) {
      QString deviceName = connIt.value().first;
      QString devicePort = connIt.value().second;
      entry.deviceType = nameToType.value(deviceName);
      QString deviceId = nameToId.value(deviceName);

      MonitorTapInfo info;
      info.monitorIndex = globalIndex;
      info.channelIndex = 0;
      info.deviceId = deviceId;
      info.devicePort = devicePort;
      info.displayMode = mobj.value(QStringLiteral("displayMode")).toString();
      auto key = qMakePair(deviceId, devicePort);
      lookup_table_[key].append(info);
      entry.channelCount = 1;
    } else {
      LOG_WARN("MONITOR", "监听器 {} 引用不存在的 connectionId={}",
               entry.name.toStdString(), connectionId.toStdString());
      entry.channelCount = 0;
    }
    tree_cache_.append(entry);
  }

  LOG_INFO("MONITOR", "追加 {} 个监听器，累计 {} 个监听器, {} 个 tap 条目",
           monitorsArr.size(), tree_cache_.size(), lookup_table_.size());
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
    LOG_DEBUG("MONITOR", "忽略: deviceId={} portName={} (无匹配 tap)",
              deviceId.toStdString(), portName.toStdString());
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

    // CVT: 按 (mi, ci) 覆盖写，只保留最新值
    buffer_[qMakePair(tapInfo.monitorIndex, tapInfo.channelIndex)] = sample;

    // history: 追加写，上限保护（全局上限，长时多通道运行会截断旧数据）
    history_buffer_.append(sample);
    if (history_buffer_.size() > kMaxHistorySamples) {
      // 超出上限删除最旧的，保留最新 kMaxHistorySamples 条
      int removeCount = history_buffer_.size() - kMaxHistorySamples;
      for (int i = 0; i < removeCount; ++i) {
        history_buffer_.removeFirst();
      }
      static bool warned = false;
      if (!warned) {
        LOG_WARN("MONITOR",
                 "history_buffer_ 已达上限 {}，旧采样数据将被丢弃（报告仅保留最新段）",
                 kMaxHistorySamples);
        warned = true;
      }
    }

    // 标记有待推送，定时器到点批量推
    pending_flush_ = true;
  }
}

// ═══════════════════════════════════════════════════════════════════
// flushNow - 立即推送（测试用，正常由定时器触发）
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::flushNow() {
  onFlushTimer();
}

// ═══════════════════════════════════════════════════════════════════
// onFlushTimer - 30fps 批量推送 CVT buffer_ 最新值给 subscribers_
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::onFlushTimer() {
  if (!pending_flush_) {
    return;
  }
  pending_flush_ = false;
  for (auto it = buffer_.constBegin(); it != buffer_.constEnd(); ++it) {
    auto subIt = subscribers_.constFind(it.key());
    if (subIt != subscribers_.constEnd() && subIt.value()) {
      subIt.value()(it.value());
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
// displayMode - 查询某通道 tap 的 displayMode
// ═══════════════════════════════════════════════════════════════════
QString MonitorManager::displayMode(int monitorIndex, int channelIndex) const {
  for (auto it = lookup_table_.constBegin(); it != lookup_table_.constEnd(); ++it) {
    for (const auto& info : it.value()) {
      if (info.monitorIndex == monitorIndex && info.channelIndex == channelIndex) {
        return info.displayMode;
      }
    }
  }
  return QString();
}

// ═══════════════════════════════════════════════════════════════════
// flushSamples - 序列化 history_buffer_ 为 JSON，清空 history_buffer_
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
  // 按 (monitorIndex, channelIndex) 分组，从 history_buffer_ 读取完整历史
  struct ChannelSamples {
    int monitorIndex;
    int channelIndex;
    QList<MonitorSample> samples;
  };
  QHash<int, QHash<int, QList<MonitorSample>>> grouped;

  for (const auto& sample : history_buffer_) {
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

  history_buffer_.clear();
  return monitorsArr;
}

// ═══════════════════════════════════════════════════════════════════
// clearRuntime — 清运行时数据（保留结构和订阅）
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::clearRuntime() {
  buffer_.clear();
  history_buffer_.clear();
  pending_flush_ = false;
}

// ═══════════════════════════════════════════════════════════════════
// clearData - 清 CVT buffer_（波形归零），保留 history_buffer_（报告不受影响）
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::clearData() {
  buffer_.clear();
  pending_flush_ = false;
}

// ═══════════════════════════════════════════════════════════════════
// clearStructure — 清结构和订阅（保留运行时数据）
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::clearStructure() {
  lookup_table_.clear();
  tree_cache_.clear();
  subscribers_.clear();
}

}  // namespace etest::engine
