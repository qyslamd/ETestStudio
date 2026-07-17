#include "TestProgramManagerWidget.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>

#include "TestProgramData.h"
#include "project/ProjectManager.h"
#include "logger/Logger.h"

namespace etest::app {

using namespace etest::core::project;

TestProgramManagerWidget::TestProgramManagerWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
  initSignals();
}

void TestProgramManagerWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // ── 顶部工具栏 ──
  auto* toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("testProgramToolbar"));
  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(8, 4, 8, 4);
  toolbar_layout->setSpacing(4);

  new_btn_ = new QPushButton(QStringLiteral("+ 新建"), this);

  toolbar_layout->addWidget(new_btn_);
  toolbar_layout->addStretch();

  layout->addWidget(toolbar);

  // ── 用例树 ──
  tree_ = new QTreeWidget(this);
  tree_->setHeaderHidden(true);
  tree_->setRootIsDecorated(true);
  tree_->setIndentation(16);
  tree_->setAnimated(false);
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_->header()->setStretchLastSection(true);
  tree_->setExpandsOnDoubleClick(false);

  layout->addWidget(tree_);
}

void TestProgramManagerWidget::initSignals() {
  connect(tree_, &QTreeWidget::itemDoubleClicked, this,
          &TestProgramManagerWidget::onItemDoubleClicked);
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &TestProgramManagerWidget::onCustomContextMenu);
  connect(tree_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/) {
            QString path;
            if (current) {
              path = current->data(0, Qt::UserRole).toString();
            }
            emit programSelectionChanged(path);
          });

  connect(new_btn_, &QPushButton::clicked, this,
          &TestProgramManagerWidget::onNewTestProgram);
}

QString TestProgramManagerWidget::selectedProgramPath() const {
  QTreeWidgetItem* item = tree_->currentItem();
  if (!item) {
    return {};
  }
  return item->data(0, Qt::UserRole).toString();
}

TestProgramData TestProgramManagerWidget::loadSelectedProgramData() const {
  QString path = selectedProgramPath();
  if (path.isEmpty()) {
    return {};
  }
  return loadTestProgram(path);
}

QStringList TestProgramManagerWidget::checkedProgramPaths() const {
  QStringList paths;
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* item = tree_->topLevelItem(i);
    if (item->checkState(0) == Qt::Checked) {
      QString path = item->data(0, Qt::UserRole).toString();
      if (!path.isEmpty()) {
        paths.append(path);
      }
    }
  }
  return paths;
}

void TestProgramManagerWidget::refreshList() {
  tree_->clear();

  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen())
    return;

  auto* project = pm.currentProject();
  if (!project)
    return;

  const QStringList tcaseFiles =
      project->scanDirectory(QStringLiteral("cases"), QStringLiteral("etprog"));
  for (const QString& absPath : tcaseFiles) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(0, QFileInfo(absPath).completeBaseName());
    item->setData(0, Qt::UserRole, absPath);
    item->setToolTip(0, absPath);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, Qt::Unchecked);

    // 解析该文件中的测试用例作为子节点
    TestProgramData suite = loadTestProgram(absPath);
    for (const auto& tc : suite.cases) {
      auto* caseItem = new QTreeWidgetItem(item);
      caseItem->setText(0, tc.name);
      caseItem->setData(0, Qt::UserRole, absPath);
      caseItem->setToolTip(0, tc.description);
    }
  }

  tree_->expandAll();
}

void TestProgramManagerWidget::onItemDoubleClicked(QTreeWidgetItem* item,
                                                   int column) {
  LOG_INFO("PROJECT_UI", "测试程序文件双击打开");
  Q_UNUSED(column);
  if (!item)
    return;

  QString filePath = item->data(0, Qt::UserRole).toString();
  if (filePath.isEmpty())
    return;

  emit openFileRequested(filePath);
}

void TestProgramManagerWidget::onCustomContextMenu(const QPoint& pos) {
  LOG_INFO("PROJECT_UI", "测试程序右键菜单");
  QTreeWidgetItem* item = tree_->itemAt(pos);
  if (!item)
    return;

  // 只在文件节点（顶层节点）上显示右键菜单
  if (item->parent())
    return;

  QString filePath = item->data(0, Qt::UserRole).toString();
  if (filePath.isEmpty())
    return;

  auto* menu = new QMenu(this);

  auto* openAction = menu->addAction(QStringLiteral("打开"));
  connect(openAction, &QAction::triggered, this,
          [this, filePath]() {
            LOG_INFO("PROJECT_UI", "打开测试程序 [path={}]", QFileInfo(filePath).fileName().toStdString());
            emit openFileRequested(filePath);
          });

  menu->addSeparator();

  auto* renameAction = menu->addAction(QStringLiteral("重命名"));
  connect(renameAction, &QAction::triggered, this,
          [this, filePath]() {
            LOG_INFO("PROJECT_UI", "重命名测试程序 [path={}]", QFileInfo(filePath).fileName().toStdString());
            renameTestProgramFile(filePath);
          });

  auto* removeAction = menu->addAction(QStringLiteral("删除"));
  connect(removeAction, &QAction::triggered, this,
          [this, filePath]() {
            LOG_INFO("PROJECT_UI", "删除测试程序 [path={}]", QFileInfo(filePath).fileName().toStdString());
            removeTestProgramFile(filePath);
          });

  menu->exec(tree_->mapToGlobal(pos));
  menu->deleteLater();
}

void TestProgramManagerWidget::onNewTestProgram() {
  LOG_INFO("PROJECT_UI", "新建测试程序");
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先打开项目"));
    return;
  }

  QString rootPath = pm.currentProjectRoot();
  if (rootPath.isEmpty())
    return;

  bool ok;
  QString name = QInputDialog::getText(
      this, QStringLiteral("新建测试用例"),
      QStringLiteral("文件名称（不含扩展名）:"), QLineEdit::Normal,
      QStringLiteral("new_test_program"), &ok);
  if (!ok || name.trimmed().isEmpty())
    return;

  name = name.trimmed();
  QString fileName = name + QStringLiteral(".etprog");
  QString dirPath = QDir(rootPath).absoluteFilePath(QStringLiteral("cases"));
  QDir casesDir(dirPath);
  if (!casesDir.exists()) {
    casesDir.mkpath(".");
  }
  QString filePath = casesDir.absoluteFilePath(fileName);

  if (QFile::exists(filePath)) {
    QMessageBox::warning(this, QStringLiteral("新建失败"),
                         QStringLiteral("文件已存在：%1").arg(filePath));
    return;
  }

  // 创建空的 .tcase 文件
  TestProgramData suite;
  suite.name = name;
  if (!saveTestProgram(filePath, suite)) {
    QMessageBox::warning(this, QStringLiteral("新建失败"),
                         QStringLiteral("无法创建文件：%1").arg(filePath));
    return;
  }

  refreshList();

  // 打开新建的文件
  emit openFileRequested(filePath);
}

bool TestProgramManagerWidget::renameTestProgramFile(const QString& oldPath) {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen())
    return false;

  bool ok;
  QString newName = QInputDialog::getText(
      this, QStringLiteral("重命名"), QStringLiteral("新名称（不含扩展名）:"),
      QLineEdit::Normal, QFileInfo(oldPath).completeBaseName(), &ok);
  if (!ok || newName.trimmed().isEmpty())
    return false;

  newName = newName.trimmed();
  QString newFileName = newName + QStringLiteral(".etprog");
  QFileInfo fi(oldPath);
  QString newPath = fi.absolutePath() + QStringLiteral("/") + newFileName;

  if (oldPath == newPath)
    return true;

  if (QFile::exists(newPath)) {
    QMessageBox::warning(this, QStringLiteral("重命名失败"),
                         QStringLiteral("文件已存在：%1").arg(newPath));
    return false;
  }

  // 重命名文件
  if (!QFile::rename(oldPath, newPath)) {
    QMessageBox::warning(this, QStringLiteral("重命名失败"),
                         QStringLiteral("无法重命名文件"));
    return false;
  }

  refreshList();
  return true;
}

bool TestProgramManagerWidget::removeTestProgramFile(const QString& filePath) {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen())
    return false;

  int ret = QMessageBox::question(
      this, QStringLiteral("确认删除"),
      QStringLiteral("确定要删除测试用例文件吗？\n%1\n\n文件将被删除。")
          .arg(filePath),
      QMessageBox::Yes | QMessageBox::No);
  if (ret != QMessageBox::Yes)
    return false;

  // 删除文件
  QFile::remove(filePath);

  refreshList();
  return true;
}

}  // namespace etest::app
