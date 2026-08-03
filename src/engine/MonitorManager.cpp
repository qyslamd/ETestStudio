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
// loadMonitors — 从项目监听器数组 + 拓扑 JSON 重建查表和树缓存（幂等）
// ═══════════════════════════════════════════════════════════════════
// connectionId 为监听器 key（一连接一监听器）；connectionId 在拓扑中不存在
// 的连接标记 invalid，进 tree_cache_ 但不进 lookup_table_（不订阅不路由）。
void MonitorManager::loadMonitors(const QJsonArray& monitors,
                                  const QJsonObject& topologyDoc) {
  clearStructure();
  clearRuntime();

  // deviceName -> (id, deviceType)
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

  // connectionId -> (deviceName, devicePort)
  QHash<QString, QPair<QString, QString>> connIdToNamePort;
  QJsonArray connsArr = topologyDoc.value(QStringLiteral("connections")).toArray();
  for (const auto& cv : connsArr) {
    QJsonObject cobj = cv.toObject();
    QString cid = cobj.value(QStringLiteral("id")).toString();
    if (!cid.isEmpty()) {
      connIdToNamePort.insert(
          cid, qMakePair(cobj.value(QStringLiteral("device")).toString(),
                         cobj.value(QStringLiteral("devicePort")).toString()));
    }
  }

  QSet<QString> seen;
  for (const auto& mv : monitors) {
    QJsonObject mobj = mv.toObject();
    QString connectionId = mobj.value(QStringLiteral("connectionId")).toString();
    QString name = mobj.value(QStringLiteral("name")).toString();
    QString displayMode = mobj.value(QStringLiteral("displayMode")).toString();

    if (seen.contains(connectionId)) {
      LOG_WARN("MONITOR", "监听器 connectionId={} 重复，跳过（审查 🟡7）",
               connectionId.toStdString());
      continue;
    }
    seen.insert(connectionId);

    MonitorTreeEntry entry;
    entry.connectionId = connectionId;
    entry.name = name;
    entry.displayMode = displayMode;

    auto connIt = connIdToNamePort.constFind(connectionId);
    if (connIt != connIdToNamePort.constEnd()) {
      QString deviceName = connIt.value().first;
      QString devicePort = connIt.value().second;
      QString deviceId = nameToId.value(deviceName);
      entry.deviceType = nameToType.value(deviceName);

      MonitorTapInfo info;
      info.connectionId = connectionId;
      info.deviceId = deviceId;
      info.devicePort = devicePort;
      info.displayMode = displayMode;
      lookup_table_[qMakePair(deviceId, devicePort)].append(info);
    } else {
      entry.invalid = true;
      invalid_ids_.insert(connectionId);
      LOG_WARN("MONITOR", "监听器 {} 引用不存在的 connectionId={}",
               name.toStdString(), connectionId.toStdString());
    }
    tree_cache_.append(entry);
  }

  LOG_INFO("MONITOR", "加载 {} 个监听器，{} 个 tap 条目，{} 个失效",
           monitors.size(), lookup_table_.size(), invalid_ids_.size());
}

// ═══════════════════════════════════════════════════════════════════
// addMonitor — 单条增量添加（执行页配置监听器时调用）
// ═══════════════════════════════════════════════════════════════════
bool MonitorManager::addMonitor(const MonitorConfig& config,
                                const QString& deviceId,
                                const QString& devicePort,
                                const QString& deviceType) {
  if (config.connectionId.isEmpty()) {
    return false;
  }
  for (const auto& entry : tree_cache_) {
    if (entry.connectionId == config.connectionId) {
      LOG_WARN("MONITOR", "连接 {} 已配置监听器，拒绝重复添加",
               config.connectionId.toStdString());
      return false;
    }
  }

  MonitorTreeEntry entry;
  entry.connectionId = config.connectionId;
  entry.name = config.name;
  entry.displayMode = config.displayMode;
  entry.deviceType = deviceType;

  MonitorTapInfo info;
  info.connectionId = config.connectionId;
  info.deviceId = deviceId;
  info.devicePort = devicePort;
  info.displayMode = config.displayMode;
  lookup_table_[qMakePair(deviceId, devicePort)].append(info);

  tree_cache_.append(entry);
  return true;
}

// ═══════════════════════════════════════════════════════════════════
// removeMonitor — 按 connectionId 删除监听器
// ═══════════════════════════════════════════════════════════════════
bool MonitorManager::removeMonitor(const QString& connectionId) {
  // 空串 key 仅当确实存在于 tree_cache_ 时允许删除（.etproj 手工损坏可能出现
  // {"connectionId":""} 的失效监听器，必须可删，审查 🟡3）
  int removed = 0;
  for (int i = tree_cache_.size() - 1; i >= 0; --i) {
    if (tree_cache_[i].connectionId == connectionId) {
      tree_cache_.removeAt(i);
      ++removed;
    }
  }
  if (removed == 0) {
    return false;
  }
  for (auto it = lookup_table_.begin(); it != lookup_table_.end(); ++it) {
    auto& taps = it.value();
    for (int i = taps.size() - 1; i >= 0; --i) {
      if (taps[i].connectionId == connectionId) {
        taps.removeAt(i);
      }
    }
  }
  subscribers_.remove(connectionId);
  buffer_.remove(connectionId);
  invalid_ids_.remove(connectionId);
  return true;
}

// ═══════════════════════════════════════════════════════════════════
// setDisplayMode — 修改监听器展示方式
// ═══════════════════════════════════════════════════════════════════
bool MonitorManager::setDisplayMode(const QString& connectionId,
                                    const QString& displayMode) {
  if (connectionId.isEmpty()) {
    return false;
  }
  bool found = false;
  for (auto& entry : tree_cache_) {
    if (entry.connectionId == connectionId) {
      entry.displayMode = displayMode;
      found = true;
      break;
    }
  }
  for (auto it = lookup_table_.begin(); it != lookup_table_.end(); ++it) {
    auto& taps = it.value();
    for (auto& tap : taps) {
      if (tap.connectionId == connectionId) {
        tap.displayMode = displayMode;
      }
    }
  }
  return found;
}

// ═══════════════════════════════════════════════════════════════════
// renameMonitor — 修改监听器主标题
// ═══════════════════════════════════════════════════════════════════
bool MonitorManager::renameMonitor(const QString& connectionId,
                                   const QString& name) {
  if (connectionId.isEmpty()) {
    return false;
  }
  for (auto& entry : tree_cache_) {
    if (entry.connectionId == connectionId) {
      entry.name = name;
      return true;
    }
  }
  return false;
}

// ═══════════════════════════════════════════════════════════════════
// subscribe / unsubscribe — 通道订阅（按 connectionId）
// ═══════════════════════════════════════════════════════════════════
void MonitorManager::subscribe(const QString& connectionId,
                               SampleCallback cb) {
  subscribers_[connectionId] = std::move(cb);
}

void MonitorManager::unsubscribe(const QString& connectionId) {
  subscribers_.remove(connectionId);
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
    sample.connectionId = tapInfo.connectionId;
    sample.engValue = engValue;
    sample.rawValue = rawValue;
    sample.rawFrame = rawFrame;
    sample.timestamp = QDateTime::currentDateTime();

    // CVT: 按 connectionId 覆盖写，只保留最新值
    buffer_[tapInfo.connectionId] = sample;

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
// monitorTree — 返回监听器树状结构（含失效标记）
// ═══════════════════════════════════════════════════════════════════
QList<MonitorManager::MonitorTreeEntry> MonitorManager::monitorTree() const {
  return tree_cache_;
}

// ═══════════════════════════════════════════════════════════════════
// displayMode - 查询某监听器的 displayMode（含失效监听器）
// ═══════════════════════════════════════════════════════════════════
QString MonitorManager::displayMode(const QString& connectionId) const {
  for (const auto& entry : tree_cache_) {
    if (entry.connectionId == connectionId) {
      return entry.displayMode;
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
//     "name": "...", "deviceType": "...",
//     "samples": [
//       { "raw": ..., "eng": ..., "ts": "..." }
//     ]
//   }
// ]
// ═══════════════════════════════════════════════════════════════════
QJsonArray MonitorManager::flushSamples() {
  // 按 connectionId 分组
  QHash<QString, QList<MonitorSample>> grouped;
  for (const auto& sample : history_buffer_) {
    grouped[sample.connectionId].append(sample);
  }

  QJsonArray monitorsArr;
  for (auto mit = grouped.constBegin(); mit != grouped.constEnd(); ++mit) {
    QString connectionId = mit.key();
    const auto& samples = mit.value();

    QString name;
    QString deviceType;
    for (const auto& entry : tree_cache_) {
      if (entry.connectionId == connectionId) {
        name = entry.name;
        deviceType = entry.deviceType;
        break;
      }
    }

    QJsonObject mObj;
    mObj[QStringLiteral("name")] = name;
    mObj[QStringLiteral("deviceType")] = deviceType;

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
    mObj[QStringLiteral("samples")] = samplesArr;
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
  invalid_ids_.clear();
}

}  // namespace etest::engine
