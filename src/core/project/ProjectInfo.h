#ifndef ETEST_CORE_PROJECT_PROJECTINFO_H_
#define ETEST_CORE_PROJECT_PROJECTINFO_H_

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariantMap>

namespace etest {
namespace core {
namespace project {

// ── 拓扑引用 ──
struct TopologyRef {
  QString id;
  QString name;
  QString filePath;  // 相对于项目根目录

  QJsonObject toJson() const;
  static TopologyRef fromJson(const QJsonObject& obj);
};

// ── ICD协议引用 ──
struct ProtocolRef {
  QString id;
  QString name;
  QString filePath;  // 相对于项目根目录

  QJsonObject toJson() const;
  static ProtocolRef fromJson(const QJsonObject& obj);
};

// ── 测试程序引用 ──
struct TestProgramRef {
  QString id;
  QString name;
  QString filePath;  // 相对于项目根目录
  QString type;      // "json" | "lua" | "excel"

  QJsonObject toJson() const;
  static TestProgramRef fromJson(const QJsonObject& obj);
};

// ── 测试报告引用 ──
struct ReportRef {
  QString id;
  QString name;
  QString filePath;  // 相对于项目根目录
  QString format;    // "text" | "html" | "pdf" | "excel"
  QDateTime generateTime;

  QJsonObject toJson() const;
  static ReportRef fromJson(const QJsonObject& obj);
};

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

  // ── 目录路径助手 ──
  QString scriptsPath() const;
  QString protocolPath() const;
  QString configPath() const;
  QString backupPath() const;
  QString topologyPath() const;
  QString reportsPath() const;
  QString casesPath() const;

  // ── 拓扑引用 ──
  QVector<TopologyRef> topologies() const;
  void setTopologies(const QVector<TopologyRef>& refs);
  void addTopology(const TopologyRef& ref);
  void removeTopology(const QString& id);
  void clearTopologies();

  // ── 协议引用 ──
  QVector<ProtocolRef> protocols() const;
  void setProtocols(const QVector<ProtocolRef>& refs);
  void addProtocol(const ProtocolRef& ref);
  void removeProtocol(const QString& id);
  void clearProtocols();

  // ── 测试程序引用 ──
  QVector<TestProgramRef> testPrograms() const;
  void setTestPrograms(const QVector<TestProgramRef>& refs);
  void addTestProgram(const TestProgramRef& ref);
  void removeTestProgram(const QString& id);
  void clearTestPrograms();

  // ── 测试报告引用 ──
  QVector<ReportRef> reports() const;
  void setReports(const QVector<ReportRef>& refs);
  void addReport(const ReportRef& ref);
  void removeReport(const QString& id);
  void clearReports();

 private:
  QString version_{"1.0"};
  QString name_;
  QDateTime create_time_;
  QString root_path_;
  QString project_file_path_;
  QStringList recent_files_;
  QVariantMap settings_;

  QVector<TopologyRef> topologies_;
  QVector<ProtocolRef> protocols_;
  QVector<TestProgramRef> test_programs_;
  QVector<ReportRef> reports_;
};

}  // namespace project
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_PROJECT_PROJECTINFO_H_
