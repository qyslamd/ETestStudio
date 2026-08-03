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
QString ProjectInfo::rootPath() const {
  if (project_file_path_.isEmpty()) return {};
  QFileInfo fi(project_file_path_);
  return fi.absoluteDir().absolutePath();
}
QString ProjectInfo::projectFilePath() const { return project_file_path_; }
QVariantMap ProjectInfo::settings() const { return settings_; }
QJsonArray ProjectInfo::monitors() const { return monitors_; }

void ProjectInfo::setVersion(const QString& v) { version_ = v; }
void ProjectInfo::setName(const QString& n) { name_ = n; }
void ProjectInfo::setCreateTime(const QDateTime& t) { create_time_ = t; }
void ProjectInfo::setProjectFilePath(const QString& p) { project_file_path_ = p; }
void ProjectInfo::setSettings(const QVariantMap& s) { settings_ = s; }
void ProjectInfo::setMonitors(const QJsonArray& monitors) { monitors_ = monitors; }

QJsonObject ProjectInfo::toJson() const {
  QJsonObject obj;
  obj["version"] = version_;
  obj["name"] = name_;
  obj["create_time"] = create_time_.toString("yyyy-MM-dd HH:mm:ss");
  obj["settings"] = QJsonObject::fromVariantMap(settings_);
  obj["monitors"] = monitors_;

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

  settings_.clear();
  if (json.contains("settings")) {
    settings_ = json["settings"].toObject().toVariantMap();
  }

  monitors_ = QJsonArray();
  if (json.contains("monitors")) {
    monitors_ = json["monitors"].toArray();
  }

  // 旧版 .etproj 文件可能包含 root_path / recent_files / topology / protocols
  // 及 test_programs / reports 字段，这些已废弃，从文件系统读取。
  // 此处静默忽略以保证向后兼容。

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
  return !name_.isEmpty() && !project_file_path_.isEmpty();
}

QString ProjectInfo::scriptsPath() const {
  return QDir(rootPath()).filePath("scripts");
}

QString ProjectInfo::protocolPath() const {
  return QDir(rootPath()).filePath("protocol");
}

QString ProjectInfo::configPath() const {
  return QDir(rootPath()).filePath("config");
}

QString ProjectInfo::backupPath() const {
  return QDir(rootPath()).filePath("backup");
}

QString ProjectInfo::topologyPath() const {
  return QDir(rootPath()).filePath("topology");
}

QString ProjectInfo::reportsPath() const {
  return QDir(rootPath()).filePath("reports");
}

QString ProjectInfo::casesPath() const {
  return QDir(rootPath()).filePath("cases");
}

QStringList ProjectInfo::scanDirectory(const QString& subDir,
                                       const QString& suffix) const {
  QDir dir(QDir(rootPath()).filePath(subDir));
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
  QDir dir(QDir(rootPath()).filePath(subDir));
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
