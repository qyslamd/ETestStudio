#include "RunConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include "logger/Logger.h"

namespace etest::app {

QJsonObject RunConfig::toJson() const {
  QJsonObject root;
  root[QStringLiteral("version")] = QStringLiteral("1.0");
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
    mo[QStringLiteral("connectionId")] = m.connectionId;
    mo[QStringLiteral("displayMode")] = m.displayMode;
    mo[QStringLiteral("name")] = m.name;
    monitorsArr.append(mo);
  }
  root[QStringLiteral("monitors")] = monitorsArr;

  QJsonArray layoutArr;
  for (const auto& l : layout) {
    QJsonObject lo;
    lo[QStringLiteral("connectionId")] = l.connectionId;
    lo[QStringLiteral("x")] = l.x;
    lo[QStringLiteral("y")] = l.y;
    lo[QStringLiteral("w")] = l.w;
    lo[QStringLiteral("h")] = l.h;
    layoutArr.append(lo);
  }
  root[QStringLiteral("layout")] = layoutArr;

  root[QStringLiteral("runParams")] = runParams;
  return root;
}

bool RunConfig::fromJson(const QJsonObject& obj) {
  // 归一化（格式不变量）：programs 去重、丢弃空串
  programs.clear();
  const QJsonArray progArr = obj[QStringLiteral("programs")].toArray();
  for (const auto& v : progArr) {
    const QString p = v.toString();
    if (!p.isEmpty() && !programs.contains(p)) {
      programs.append(p);
    }
  }

  // 归一化（格式不变量）：monitors 同 connectionId 去重，保留首个
  monitors.clear();
  const QJsonArray monitorsArr = obj[QStringLiteral("monitors")].toArray();
  QSet<QString> seenMonitors;
  for (const auto& v : monitorsArr) {
    const QJsonObject mo = v.toObject();
    const QString cid = mo[QStringLiteral("connectionId")].toString();
    if (cid.isEmpty() || seenMonitors.contains(cid)) {
      continue;
    }
    seenMonitors.insert(cid);
    Monitor m;
    m.connectionId = cid;
    m.displayMode = mo[QStringLiteral("displayMode")].toString();
    m.name = mo[QStringLiteral("name")].toString();
    monitors.append(m);
  }

  // 归一化：layout 丢弃无对应 monitor 的项 + 同 connectionId 去重（保留首个）
  layout.clear();
  const QJsonArray layoutArr = obj[QStringLiteral("layout")].toArray();
  QSet<QString> seenLayout;
  for (const auto& v : layoutArr) {
    const QJsonObject lo = v.toObject();
    const QString cid = lo[QStringLiteral("connectionId")].toString();
    if (cid.isEmpty() || !seenMonitors.contains(cid) ||
        seenLayout.contains(cid)) {
      continue;
    }
    seenLayout.insert(cid);
    LayoutItem l;
    l.connectionId = cid;
    l.x = lo[QStringLiteral("x")].toDouble();
    l.y = lo[QStringLiteral("y")].toDouble();
    l.w = lo[QStringLiteral("w")].toDouble();
    l.h = lo[QStringLiteral("h")].toDouble();
    layout.append(l);
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

}  // namespace etest::app
