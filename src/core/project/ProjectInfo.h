#ifndef ETEST_CORE_PROJECT_PROJECTINFO_H_
#define ETEST_CORE_PROJECT_PROJECTINFO_H_

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace etest {
namespace core {
namespace project {

class ProjectInfo {
 public:
  ProjectInfo() = default;
  explicit ProjectInfo(const QString& filePath);
  ~ProjectInfo() = default;

  // Getters
  QString version() const;
  QString name() const;
  QDateTime createTime() const;
  QString rootPath() const;
  QString projectFilePath() const;
  QStringList recentFiles() const;
  QVariantMap settings() const;

  // Setters
  void setVersion(const QString& v);
  void setName(const QString& n);
  void setCreateTime(const QDateTime& t);
  void setRootPath(const QString& p);
  void setProjectFilePath(const QString& p);
  void setRecentFiles(const QStringList& files);
  void setSettings(const QVariantMap& s);

  // 序列化
  QJsonObject toJson() const;
  bool fromJson(const QJsonObject& json);
  bool loadFromFile(const QString& filePath);
  bool saveToFile(const QString& filePath = QString()) const;

  // 工具方法
  bool isValid() const;
  QString scriptsPath() const;
  QString protocolPath() const;
  QString configPath() const;
  QString backupPath() const;

 private:
  QString version_{"1.0"};
  QString name_;
  QDateTime create_time_;
  QString root_path_;
  QString project_file_path_;
  QStringList recent_files_;
  QVariantMap settings_;
};

}  // namespace project
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_PROJECT_PROJECTINFO_H_
