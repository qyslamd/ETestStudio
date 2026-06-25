#include "ProjectInfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "common/FileException.h"
#include "utils/FileUtil.h"

namespace etest {
namespace core {
namespace project {

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

  // 旧版 .etproj 文件可能包含 topology/protocols/test_programs/reports 字段，
  // 这些已废弃，从文件系统读取。此处静默忽略以保证向后兼容。

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

QStringList ProjectInfo::scanDirectory(const QString& subDir,
                                       const QString& suffix) const {
  QDir dir(QDir(root_path_).filePath(subDir));
  if (!dir.exists()) return {};

  QStringList result;
  const auto entries =
      dir.entryInfoList({QStringLiteral("*.") + suffix},
                        QDir::Files | QDir::NoDotAndDotDot);
  for (const auto& fi : entries) {
    result.append(fi.absoluteFilePath());
  }
  return result;
}

QStringList ProjectInfo::scanDirectory(const QString& subDir,
                                        const QStringList& suffixes) const {
  QDir dir(QDir(root_path_).filePath(subDir));
  if (!dir.exists()) return {};

  QStringList filters;
  for (const auto& suffix : suffixes) {
    filters << (QStringLiteral("*.") + suffix);
  }

  QStringList result;
  const auto entries =
      dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
  for (const auto& fi : entries) {
    result.append(fi.absoluteFilePath());
  }
  return result;
}

}  // namespace project
}  // namespace core
}  // namespace etest
