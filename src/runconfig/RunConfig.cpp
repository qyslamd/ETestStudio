#include "RunConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>

#include "logger/Logger.h"

namespace etest::runconfig {

QJsonObject RunConfig::toJson() const {
  QJsonObject root;
  root[QStringLiteral("version")] = QStringLiteral("2.0");
  if (!programs.isEmpty()) {
    QJsonArray progArr;
    for (const QString& p : programs) {
      progArr.append(p);
    }
    root[QStringLiteral("programs")] = progArr;
  }

  QJsonArray monitorsArr;
  for (const auto& m : monitors) {
    QJsonObject mo;
    mo[QStringLiteral("id")] = m.id;
    if (!m.connectionId.isEmpty()) {
      mo[QStringLiteral("connectionId")] = m.connectionId;
    }
    mo[QStringLiteral("displayMode")] = m.displayMode;
    mo[QStringLiteral("name")] = m.name;
    mo[QStringLiteral("x")] = m.x;
    mo[QStringLiteral("y")] = m.y;
    mo[QStringLiteral("w")] = m.w;
    mo[QStringLiteral("h")] = m.h;
    monitorsArr.append(mo);
  }
  root[QStringLiteral("monitors")] = monitorsArr;

  root[QStringLiteral("runParams")] = runParams;
  return root;
}

bool RunConfig::fromJson(const QJsonObject& obj) {
  // 归一化（格式不变量 2.0）：programs 去重、丢弃空串
  programs.clear();
  const QJsonArray progArr = obj[QStringLiteral("programs")].toArray();
  for (const auto& v : progArr) {
    const QString p = v.toString();
    if (!p.isEmpty() && !programs.contains(p)) {
      programs.append(p);
    }
  }

  // 归一化（格式不变量 2.0）：双去重——
  //   seenIds：卡片身份去重（id 唯一，无 id 兜底生成 UUID）
  //   seenConnectionIds：仅非空 connectionId 去重（保留首个，一连接一监听器）
  // 空 connectionId = 未绑定，合法保留（不进 connectionId 去重集）。
  monitors.clear();
  const QJsonArray monitorsArr = obj[QStringLiteral("monitors")].toArray();
  QSet<QString> seenIds;
  QSet<QString> seenConnectionIds;
  for (const auto& v : monitorsArr) {
    const QJsonObject mo = v.toObject();
    QString id = mo[QStringLiteral("id")].toString();
    if (id.isEmpty()) {
      id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (seenIds.contains(id)) {
      continue;
    }
    seenIds.insert(id);
    const QString cid = mo[QStringLiteral("connectionId")].toString();
    if (!cid.isEmpty()) {
      if (seenConnectionIds.contains(cid)) {
        continue;
      }
      seenConnectionIds.insert(cid);
    }
    Monitor m;
    m.id = id;
    m.connectionId = cid;
    m.displayMode = mo[QStringLiteral("displayMode")].toString();
    m.name = mo[QStringLiteral("name")].toString();
    m.x = mo[QStringLiteral("x")].toDouble();
    m.y = mo[QStringLiteral("y")].toDouble();
    m.w = mo[QStringLiteral("w")].toDouble();
    m.h = mo[QStringLiteral("h")].toDouble();
    monitors.append(m);
  }

  runParams = obj[QStringLiteral("runParams")].toObject();
  return true;
}

bool RunConfig::loadFromFile(const QString& path, RunConfig* out) {
  if (!out) {
    return false;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    LOG_WARN("RUNCONFIG", "解析 .erun 失败: {} ({})", path.toStdString(),
             error.errorString().toStdString());
    return false;
  }
  *out = RunConfig();
  return out->fromJson(doc.object());
}

bool RunConfig::saveToFile(const QString& path, const RunConfig& config) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  const QJsonDocument doc(config.toJson());
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  return true;
}

}  // namespace etest::runconfig
