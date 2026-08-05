#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "project/ProjectInfo.h"
#include "project/ProjectManager.h"

using namespace etest::core::project;
using namespace etest::core::config;

namespace {
QString testDir() {
  return QDir::temp().filePath("etest_project_test");
}
void cleanupTestDir() {
  QDir dir(testDir());
  if (dir.exists()) {
    dir.removeRecursively();
  }
}
}  // namespace

// ==================== ProjectInfo 测试 ====================

class ProjectInfoTest : public ::testing::Test {
 protected:
  void SetUp() override { cleanupTestDir(); }
  void TearDown() override { cleanupTestDir(); }
};

TEST_F(ProjectInfoTest, DefaultValues) {
  ProjectInfo info;
  EXPECT_EQ(info.version(), "1.0");
  EXPECT_TRUE(info.name().isEmpty());
  EXPECT_FALSE(info.isValid());
}

TEST_F(ProjectInfoTest, SettersAndGetters) {
  ProjectInfo info;
  info.setName("test_project");
  info.setProjectFilePath("/tmp/test/test.etproj");
  info.setCreateTime(QDateTime(QDate(2026, 4, 23), QTime(15, 0, 0)));

  EXPECT_EQ(info.name(), "test_project");
  EXPECT_EQ(info.rootPath(), "/tmp/test");
  EXPECT_EQ(info.createTime().date(), QDate(2026, 4, 23));
}

TEST_F(ProjectInfoTest, ToJsonFromJson) {
  ProjectInfo original;
  original.setName("my_project");
  original.setProjectFilePath("/home/user/my_project/my_project.etproj");
  original.setCreateTime(QDateTime(QDate(2026, 4, 23), QTime(10, 30, 0)));

  QJsonObject json = original.toJson();
  EXPECT_EQ(json["name"].toString(), "my_project");
  EXPECT_EQ(json["version"].toString(), "1.0");
  EXPECT_FALSE(json.contains("root_path"));
  EXPECT_FALSE(json.contains("recent_files"));

  ProjectInfo restored;
  ASSERT_TRUE(restored.fromJson(json));
  EXPECT_EQ(restored.name(), original.name());
  EXPECT_EQ(restored.version(), original.version());
}

TEST_F(ProjectInfoTest, FromJsonInvalid) {
  QJsonObject empty;
  ProjectInfo info;
  EXPECT_FALSE(info.fromJson(empty));

  QJsonObject noName;
  noName["version"] = "1.0";
  EXPECT_FALSE(info.fromJson(noName));
}

TEST_F(ProjectInfoTest, SaveAndLoadFile) {
  QString dir = testDir();
  QDir().mkpath(dir);
  QString filePath = dir + "/test.etproj";

  ProjectInfo original;
  original.setName("file_test");
  original.setProjectFilePath(filePath);
  original.setCreateTime(QDateTime::currentDateTime());

  ASSERT_TRUE(original.saveToFile());
  ASSERT_TRUE(QFile::exists(filePath));

  ProjectInfo loaded;
  ASSERT_TRUE(loaded.loadFromFile(filePath));
  EXPECT_EQ(loaded.name(), "file_test");
  EXPECT_EQ(loaded.version(), "1.0");
}

TEST_F(ProjectInfoTest, LoadNonExistentFile) {
  ProjectInfo info;
  EXPECT_FALSE(info.loadFromFile("/nonexistent/path/test.etproj"));
}

TEST_F(ProjectInfoTest, LoadInvalidJsonFile) {
  QString dir = testDir();
  QDir().mkpath(dir);
  QString filePath = dir + "/invalid.etproj";

  QFile file(filePath);
  file.open(QIODevice::WriteOnly);
  file.write("not valid json {{{");
  file.close();

  ProjectInfo info;
  EXPECT_FALSE(info.loadFromFile(filePath));
}

TEST_F(ProjectInfoTest, DirectoryPathHelpers) {
  ProjectInfo info;
  info.setProjectFilePath("/home/user/project/test.etproj");

  EXPECT_EQ(info.scriptsPath(), QDir("/home/user/project").filePath("scripts"));
  EXPECT_EQ(info.protocolPath(),
            QDir("/home/user/project").filePath("protocol"));
  EXPECT_EQ(info.configPath(), QDir("/home/user/project").filePath("config"));
  EXPECT_EQ(info.backupPath(), QDir("/home/user/project").filePath("backup"));
  EXPECT_EQ(info.topologyPath(),
            QDir("/home/user/project").filePath("topology"));
  EXPECT_EQ(info.reportsPath(),
            QDir("/home/user/project").filePath("reports"));
  EXPECT_EQ(info.casesPath(), QDir("/home/user/project").filePath("cases"));
}

TEST_F(ProjectInfoTest, ScanDirectory) {
  // 创建临时目录结构
  QString dir = testDir();
  QDir().mkpath(dir);
  QDir().mkpath(dir + "/protocol");
  QDir().mkpath(dir + "/cases");
  QDir().mkpath(dir + "/empty_dir");

  // 创建 .eproto 文件
  QFile(dir + "/protocol/a.eproto").open(QIODevice::WriteOnly);
  QFile(dir + "/protocol/b.eproto").open(QIODevice::WriteOnly);
  // 创建 .txt 文件（应被过滤）
  QFile(dir + "/protocol/note.txt").open(QIODevice::WriteOnly);
  // 创建 .tcase 文件
  QFile(dir + "/cases/test1.tcase").open(QIODevice::WriteOnly);

  ProjectInfo info;
  info.setProjectFilePath(dir + "/test.etproj");

  // scanDirectory：协议文件
  QStringList protoFiles = info.scanDirectory("protocol", "eproto");
  EXPECT_EQ(protoFiles.size(), 2);
  EXPECT_TRUE(protoFiles[0].endsWith("a.eproto") || protoFiles[0].endsWith("b.eproto"));
  // 返回绝对路径
  EXPECT_TRUE(QDir::isAbsolutePath(protoFiles[0]));

  // scanDirectory：测试用例
  QStringList tcaseFiles = info.scanDirectory("cases", "tcase");
  EXPECT_EQ(tcaseFiles.size(), 1);
  EXPECT_TRUE(tcaseFiles[0].endsWith("test1.tcase"));

  // scanDirectory：空目录
  QStringList empty = info.scanDirectory("empty_dir", "eproto");
  EXPECT_TRUE(empty.isEmpty());

  // scanDirectory：不存在的目录
  QStringList notExist = info.scanDirectory("nonexistent", "eproto");
  EXPECT_TRUE(notExist.isEmpty());
}

TEST_F(ProjectInfoTest, IsValid) {
  ProjectInfo info;
  EXPECT_FALSE(info.isValid());

  info.setName("test");
  EXPECT_FALSE(info.isValid());

  info.setProjectFilePath("/tmp/test/test.etproj");
  EXPECT_TRUE(info.isValid());
}

// ==================== ProjectManager 测试 ====================

class ProjectManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    cleanupTestDir();
    pm_ = &ProjectManager::instance();
  }
  void TearDown() override {
    if (pm_->isProjectOpen()) {
      pm_->closeProject();
    }
    cleanupTestDir();
  }

  ProjectManager* pm_;
};

TEST_F(ProjectManagerTest, SingletonInstance) {
  ProjectManager& a = ProjectManager::instance();
  ProjectManager& b = ProjectManager::instance();
  EXPECT_EQ(&a, &b);
}

TEST_F(ProjectManagerTest, NotOpenInitially) {
  EXPECT_FALSE(pm_->isProjectOpen());
  EXPECT_EQ(pm_->currentProject(), nullptr);
}

TEST_F(ProjectManagerTest, CreateProjectSuccess) {
  QString location = testDir();
  QDir().mkpath(location);

  bool result = pm_->createProject("new_project", location);
  ASSERT_TRUE(result);
  EXPECT_TRUE(pm_->isProjectOpen());

  auto* project = pm_->currentProject();
  ASSERT_NE(project, nullptr);
  EXPECT_EQ(project->name(), "new_project");

  QString projectDir = QDir(location).filePath("new_project");
  EXPECT_TRUE(QDir(projectDir).exists());
  EXPECT_TRUE(QDir(projectDir + "/scripts").exists());
  EXPECT_TRUE(QDir(projectDir + "/protocol").exists());
  EXPECT_TRUE(QDir(projectDir + "/config").exists());
  EXPECT_TRUE(QDir(projectDir + "/backup").exists());
  EXPECT_TRUE(QDir(projectDir + "/topology").exists());
  EXPECT_TRUE(QDir(projectDir + "/reports").exists());
  EXPECT_TRUE(QDir(projectDir + "/cases").exists());

  QString etprojPath = projectDir + "/new_project.etproj";
  EXPECT_TRUE(QFile::exists(etprojPath));
}

TEST_F(ProjectManagerTest, CreateProjectEmptyNameFails) {
  QString location = testDir();
  QDir().mkpath(location);
  EXPECT_FALSE(pm_->createProject("", location));
}

TEST_F(ProjectManagerTest, CreateProjectEmptyLocationFails) {
  EXPECT_FALSE(pm_->createProject("test", ""));
}

TEST_F(ProjectManagerTest, CreateProjectDuplicateFails) {
  QString location = testDir();
  QDir().mkpath(location);

  ASSERT_TRUE(pm_->createProject("dup_project", location));
  pm_->closeProject();
  EXPECT_FALSE(pm_->createProject("dup_project", location));
}

TEST_F(ProjectManagerTest, CreateProjectSignals) {
  QString location = testDir();
  QDir().mkpath(location);

  QSignalSpy createdSpy(pm_, &ProjectManager::projectCreated);
  QSignalSpy openedSpy(pm_, &ProjectManager::projectOpened);

  pm_->createProject("signal_test", location);

  EXPECT_EQ(createdSpy.count(), 1);
  EXPECT_EQ(openedSpy.count(), 1);
}

TEST_F(ProjectManagerTest, OpenProjectSuccess) {
  QString location = testDir();
  QDir().mkpath(location);

  ASSERT_TRUE(pm_->createProject("open_test", location));
  QString etprojPath = pm_->currentProject()->projectFilePath();
  pm_->closeProject();

  EXPECT_FALSE(pm_->isProjectOpen());
  ASSERT_TRUE(pm_->openProject(etprojPath));
  EXPECT_TRUE(pm_->isProjectOpen());
  EXPECT_EQ(pm_->currentProject()->name(), "open_test");
}

TEST_F(ProjectManagerTest, OpenProjectNonExistentFails) {
  EXPECT_FALSE(pm_->openProject("/nonexistent/test.etproj"));
}

TEST_F(ProjectManagerTest, OpenProjectInvalidExtensionFails) {
  QString location = testDir();
  QDir().mkpath(location);
  EXPECT_FALSE(pm_->openProject(location + "/test.txt"));
}

TEST_F(ProjectManagerTest, OpenProjectCorruptedFileFails) {
  QString location = testDir();
  QDir().mkpath(location);
  QString filePath = location + "/corrupt.etproj";

  QFile file(filePath);
  file.open(QIODevice::WriteOnly);
  file.write("invalid json content");
  file.close();

  EXPECT_FALSE(pm_->openProject(filePath));
}

TEST_F(ProjectManagerTest, CloseProject) {
  QString location = testDir();
  QDir().mkpath(location);

  ASSERT_TRUE(pm_->createProject("close_test", location));
  EXPECT_TRUE(pm_->isProjectOpen());

  QSignalSpy closedSpy(pm_, &ProjectManager::projectClosed);
  pm_->closeProject();

  EXPECT_FALSE(pm_->isProjectOpen());
  EXPECT_EQ(closedSpy.count(), 1);
}

TEST_F(ProjectManagerTest, CloseProjectWhenNotOpen) {
  EXPECT_TRUE(pm_->closeProject());
}

TEST_F(ProjectManagerTest, SaveProject) {
  QString location = testDir();
  QDir().mkpath(location);

  ASSERT_TRUE(pm_->createProject("save_test", location));
  EXPECT_TRUE(pm_->saveProject());
}

TEST_F(ProjectManagerTest, SaveProjectWhenNotOpen) {
  EXPECT_FALSE(pm_->saveProject());
}

// ==================== 最近项目测试 ====================

TEST_F(ProjectManagerTest, AddToRecentProjects) {
  pm_->clearRecentProjects();
  pm_->addToRecentProjects("/path/to/project1.etproj");
  pm_->addToRecentProjects("/path/to/project2.etproj");

  QStringList recent = pm_->recentProjects();
  EXPECT_GE(recent.size(), 2);
  // 最近添加的在最前面
  EXPECT_EQ(recent.first(), "/path/to/project2.etproj");
}

TEST_F(ProjectManagerTest, AddToRecentProjectsDedup) {
  pm_->clearRecentProjects();
  pm_->addToRecentProjects("/path/to/project1.etproj");
  pm_->addToRecentProjects("/path/to/project2.etproj");
  pm_->addToRecentProjects("/path/to/project1.etproj");

  QStringList recent = pm_->recentProjects();
  int count = recent.filter("/path/to/project1.etproj").size();
  EXPECT_EQ(count, 1);
  // 重复添加后移到最前
  EXPECT_EQ(recent.first(), "/path/to/project1.etproj");
}

TEST_F(ProjectManagerTest, RecentProjectsMaxCount) {
  pm_->clearRecentProjects();
  for (int i = 0; i < 15; ++i) {
    pm_->addToRecentProjects(QString("/path/project%1.etproj").arg(i));
  }

  QStringList recent = pm_->recentProjects();
  EXPECT_EQ(recent.size(), CONFIG_RECENT_MAX_COUNT);
}

TEST_F(ProjectManagerTest, RemoveFromRecentProjects) {
  pm_->clearRecentProjects();
  pm_->addToRecentProjects("/path/to/remove.etproj");
  pm_->removeFromRecentProjects("/path/to/remove.etproj");

  QStringList recent = pm_->recentProjects();
  EXPECT_FALSE(recent.contains("/path/to/remove.etproj"));
}

TEST_F(ProjectManagerTest, ClearRecentProjects) {
  pm_->addToRecentProjects("/path/to/project1.etproj");
  pm_->clearRecentProjects();

  QStringList recent = pm_->recentProjects();
  EXPECT_TRUE(recent.isEmpty());
}

TEST_F(ProjectManagerTest, RecentProjectsSignal) {
  pm_->clearRecentProjects();
  QSignalSpy spy(pm_, &ProjectManager::recentProjectsChanged);
  pm_->addToRecentProjects("/path/test.etproj");
  EXPECT_EQ(spy.count(), 1);
}

// ==================== 脏文件检查测试 ====================

TEST_F(ProjectManagerTest, NoDirtyCheckByDefault) {
  EXPECT_FALSE(pm_->hasUnsavedChanges());
}

TEST_F(ProjectManagerTest, DirtyCheckCallbackReturnsTrue) {
  pm_->setDirtyCheckCallback([]() { return true; });
  EXPECT_TRUE(pm_->hasUnsavedChanges());
}

TEST_F(ProjectManagerTest, DirtyCheckCallbackReturnsFalse) {
  pm_->setDirtyCheckCallback([]() { return false; });
  EXPECT_FALSE(pm_->hasUnsavedChanges());
}

TEST_F(ProjectManagerTest, ResetDirtyCheckCallback) {
  pm_->setDirtyCheckCallback([]() { return true; });
  EXPECT_TRUE(pm_->hasUnsavedChanges());

  pm_->setDirtyCheckCallback(nullptr);
  EXPECT_FALSE(pm_->hasUnsavedChanges());
}
