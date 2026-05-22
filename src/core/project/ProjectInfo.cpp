#include "ProjectInfo.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include "common/FileException.h"
#include "utils/FileUtil.h"

namespace etest {
namespace core {
namespace project {

// ── TopologyRef ──────────────────────────────────────────────

QJsonObject TopologyRef::toJson() const {
  QJsonObject obj;
  obj["id"] = id;
  obj["name"] = name;
  obj["file"] = filePath;
  return obj;
}

TopologyRef TopologyRef::fromJson(const QJsonObject& obj) {
  TopologyRef ref;
  ref.id = obj["id"].toString();
  ref.name = obj["name"].toString();
  ref.filePath = obj["file"].toString();
  // 兼容旧数据：如果没有 id 则生成一个
  if (ref.id.isEmpty()) {
    ref.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  return ref;
}

// ── ProtocolRef ──────────────────────────────────────────────

QJsonObject ProtocolRef::toJson() const {
  QJsonObject obj;
  obj["id"] = id;
  obj["name"] = name;
  obj["file"] = filePath;
  return obj;
}

ProtocolRef ProtocolRef::fromJson(const QJsonObject& obj) {
  ProtocolRef ref;
  ref.id = obj["id"].toString();
  ref.name = obj["name"].toString();
  ref.filePath = obj["file"].toString();
  if (ref.filePath.isEmpty()) {
    ref.filePath = ref.id;          // 兼容旧数据：id 字段曾用作路径
  }
  if (ref.id.isEmpty()) {
    ref.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  return ref;
}

// ── TestProgramRef ─────────────────────────────────────────────

QJsonObject TestProgramRef::toJson() const {
  QJsonObject obj;
  obj["id"] = id;
  obj["name"] = name;
  obj["file"] = filePath;
  obj["type"] = type;
  return obj;
}

TestProgramRef TestProgramRef::fromJson(const QJsonObject& obj) {
  TestProgramRef ref;
  ref.id = obj["id"].toString();
  ref.name = obj["name"].toString();
  ref.filePath = obj["file"].toString();
  ref.type = obj["type"].toString("json");
  if (ref.id.isEmpty()) {
    ref.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  return ref;
}

// ── ReportRef ────────────────────────────────────────────────

QJsonObject ReportRef::toJson() const {
  QJsonObject obj;
  obj["id"] = id;
  obj["name"] = name;
  obj["file"] = filePath;
  obj["format"] = format;
  obj["generate_time"] = generateTime.toString("yyyy-MM-dd HH:mm:ss");
  return obj;
}

ReportRef ReportRef::fromJson(const QJsonObject& obj) {
  ReportRef ref;
  ref.id = obj["id"].toString();
  ref.name = obj["name"].toString();
  ref.filePath = obj["file"].toString();
  ref.format = obj["format"].toString("html");
  ref.generateTime = QDateTime::fromString(obj["generate_time"].toString(),
                                           "yyyy-MM-dd HH:mm:ss");
  if (ref.id.isEmpty()) {
    ref.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  return ref;
}

ProjectInfo::ProjectInfo(const QString& filePath) {
  loadFromFile(filePath);
}

QString ProjectInfo::version() const { return version_; }
QString ProjectInfo::name() const { return name_; }
QDateTime ProjectInfo::createTime() const { return create_time_; }
QString ProjectInfo::rootPath() const { return root_path_; }
QString ProjectInfo::projectFilePath() const { return project_file_path_; }
QStringList ProjectInfo::recentFiles() const { return recent_files_; }
QVariantMap ProjectInfo::settings() const { return settings_; }

void ProjectInfo::setVersion(const QString& v) { version_ = v; }
void ProjectInfo::setName(const QString& n) { name_ = n; }
void ProjectInfo::setCreateTime(const QDateTime& t) { create_time_ = t; }
void ProjectInfo::setRootPath(const QString& p) { root_path_ = p; }
void ProjectInfo::setProjectFilePath(const QString& p) { project_file_path_ = p; }
void ProjectInfo::setRecentFiles(const QStringList& files) { recent_files_ = files; }
void ProjectInfo::setSettings(const QVariantMap& s) { settings_ = s; }

QJsonObject ProjectInfo::toJson() const {
  QJsonObject obj;
  obj["version"] = version_;
  obj["name"] = name_;
  obj["create_time"] = create_time_.toString("yyyy-MM-dd HH:mm:ss");
  obj["root_path"] = root_path_;
  obj["recent_files"] = QJsonArray::fromStringList(recent_files_);
  obj["settings"] = QJsonObject::fromVariantMap(settings_);

  // 工件引用列表
  QJsonArray topoArr;
  for (const auto& t : topologies_) {
    topoArr.append(t.toJson());
  }
  obj["topology"] = topoArr;

  QJsonArray protoArr;
  for (const auto& p : protocols_) {
    protoArr.append(p.toJson());
  }
  obj["protocols"] = protoArr;

  QJsonArray caseArr;
  for (const auto& c : test_programs_) {
    caseArr.append(c.toJson());
  }
  obj["test_programs"] = caseArr;

  QJsonArray reportArr;
  for (const auto& r : reports_) {
    reportArr.append(r.toJson());
  }
  obj["reports"] = reportArr;

  return obj;
}

bool ProjectInfo::fromJson(const QJsonObject& json) {
  if (!json.contains("version") || !json.contains("name")) {
    return false;
  }

  version_ = json["version"].toString();
  name_ = json["name"].toString();
  create_time_ = QDateTime::fromString(json["create_time"].toString(),
                                       "yyyy-MM-dd HH:mm:ss");
  root_path_ = json["root_path"].toString("./");

  recent_files_.clear();
  if (json.contains("recent_files")) {
    QJsonArray arr = json["recent_files"].toArray();
    for (const auto& item : arr) {
      recent_files_.append(item.toString());
    }
  }

  settings_.clear();
  if (json.contains("settings")) {
    settings_ = json["settings"].toObject().toVariantMap();
  }

  // ── 工件引用列表 ──
  topologies_.clear();
  if (json.contains("topology")) {
    for (const auto& item : json["topology"].toArray()) {
      topologies_.append(TopologyRef::fromJson(item.toObject()));
    }
  }

  protocols_.clear();
  if (json.contains("protocols")) {
    for (const auto& item : json["protocols"].toArray()) {
      protocols_.append(ProtocolRef::fromJson(item.toObject()));
    }
  }

  test_programs_.clear();
  if (json.contains("test_programs")) {
    for (const auto& item : json["test_programs"].toArray()) {
      test_programs_.append(TestProgramRef::fromJson(item.toObject()));
    }
  }

  reports_.clear();
  if (json.contains("reports")) {
    for (const auto& item : json["reports"].toArray()) {
      reports_.append(ReportRef::fromJson(item.toObject()));
    }
  }

  return true;
}

bool ProjectInfo::loadFromFile(const QString& filePath) {
  QString content;
  try {
    content = utils::FileUtil::readTextFile(filePath);
  } catch (const common::FileException&) {
    return false;
  }
  if (content.isEmpty()) {
    return false;
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError) {
    return false;
  }
  if (!doc.isObject()) {
    return false;
  }

  if (!fromJson(doc.object())) {
    return false;
  }

  project_file_path_ = filePath;
  return true;
}

bool ProjectInfo::saveToFile(const QString& filePath) const {
  QString savePath = filePath.isEmpty() ? project_file_path_ : filePath;
  if (savePath.isEmpty()) {
    return false;
  }

  QJsonDocument doc(toJson());
  QString content = doc.toJson(QJsonDocument::Indented);
  return utils::FileUtil::writeTextFile(savePath, content);
}

bool ProjectInfo::isValid() const {
  return !name_.isEmpty() && !root_path_.isEmpty();
}

QString ProjectInfo::scriptsPath() const {
  return QDir(root_path_).filePath("scripts");
}

QString ProjectInfo::protocolPath() const {
  return QDir(root_path_).filePath("protocol");
}

QString ProjectInfo::configPath() const {
  return QDir(root_path_).filePath("config");
}

QString ProjectInfo::backupPath() const {
  return QDir(root_path_).filePath("backup");
}

QString ProjectInfo::topologyPath() const {
  return QDir(root_path_).filePath("topology");
}

QString ProjectInfo::reportsPath() const {
  return QDir(root_path_).filePath("reports");
}

QString ProjectInfo::casesPath() const {
  return QDir(root_path_).filePath("cases");
}

// ── TopologyRef 访问器 ──

QVector<TopologyRef> ProjectInfo::topologies() const { return topologies_; }

void ProjectInfo::setTopologies(const QVector<TopologyRef>& refs) {
  topologies_ = refs;
}

void ProjectInfo::addTopology(const TopologyRef& ref) {
  topologies_.append(ref);
}

void ProjectInfo::removeTopology(const QString& id) {
  topologies_.erase(std::remove_if(topologies_.begin(), topologies_.end(),
                                   [&](const TopologyRef& t) {
                                     return t.id == id;
                                   }),
                    topologies_.end());
}

void ProjectInfo::clearTopologies() { topologies_.clear(); }

// ── ProtocolRef 访问器 ──

QVector<ProtocolRef> ProjectInfo::protocols() const { return protocols_; }

void ProjectInfo::setProtocols(const QVector<ProtocolRef>& refs) {
  protocols_ = refs;
}

void ProjectInfo::addProtocol(const ProtocolRef& ref) {
  protocols_.append(ref);
}

void ProjectInfo::removeProtocol(const QString& id) {
  protocols_.erase(std::remove_if(protocols_.begin(), protocols_.end(),
                                  [&](const ProtocolRef& p) {
                                    return p.id == id;
                                  }),
                   protocols_.end());
}

void ProjectInfo::clearProtocols() { protocols_.clear(); }

// ── TestProgramRef 访问器 ──

QVector<TestProgramRef> ProjectInfo::testPrograms() const { return test_programs_; }

void ProjectInfo::setTestPrograms(const QVector<TestProgramRef>& refs) {
  test_programs_ = refs;
}

void ProjectInfo::addTestProgram(const TestProgramRef& ref) {
  test_programs_.append(ref);
}

void ProjectInfo::removeTestProgram(const QString& id) {
  test_programs_.erase(std::remove_if(test_programs_.begin(), test_programs_.end(),
                                      [&](const TestProgramRef& c) {
                                        return c.id == id;
                                      }),
                       test_programs_.end());
}

void ProjectInfo::clearTestPrograms() { test_programs_.clear(); }

// ── ReportRef 访问器 ──

QVector<ReportRef> ProjectInfo::reports() const { return reports_; }

void ProjectInfo::setReports(const QVector<ReportRef>& refs) {
  reports_ = refs;
}

void ProjectInfo::addReport(const ReportRef& ref) { reports_.append(ref); }

void ProjectInfo::removeReport(const QString& id) {
  reports_.erase(std::remove_if(reports_.begin(), reports_.end(),
                                [&](const ReportRef& r) { return r.id == id; }),
                 reports_.end());
}

void ProjectInfo::clearReports() { reports_.clear(); }

}  // namespace project
}  // namespace core
}  // namespace etest
