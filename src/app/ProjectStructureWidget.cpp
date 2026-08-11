#include "ProjectStructureWidget.h"

#include "project/ProjectManager.h"
#include "widgets/ProjectTreeDelegate.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>
#include <memory>
#include <utility>
#include "logger/Logger.h"
#include "format/xml_serializer.hpp"
#include "utils/FileUtil.h"

#include <icd/repository.hpp>

#include "AppIconProvider.h"
#include "ConfigManager.h"
#include "TestProgramData.h"
#include "config/ConfigDefs.h"
#include "wizards/ProtocolFileWizard.h"
#include "wizards/TestProgramWizard.h"
#include "wizards/TopologyFileWizard.h"
#include "topology/TopologyDocument.h"
#include "topology/TopologyJsonSerializer.h"

namespace etest::app {

using etest::core_ui::AppIconProvider;

// ── 新建文件默认基名（自动递增） ──
static QString newFileBaseName(const QString& base, const QString& dir);

ProjectStructureWidget::ProjectStructureWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
  initSignals();
  refreshRecentProjects();
  refreshRecentFiles();
}

void ProjectStructureWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  stack_ = new QStackedWidget(this);

  // ── 模式 0：无项目占位页（可滚动） ──
  page_default_ = new QWidget(this);
  page_default_->setObjectName(QStringLiteral("PhPlaceholder"));
  auto* ph_layout = new QVBoxLayout(page_default_);
  ph_layout->setContentsMargins(0, 0, 0, 0);
  ph_layout->setSpacing(0);

  auto* scroll_area = new QScrollArea(this);
  scroll_area->setWidgetResizable(true);
  scroll_area->setFrameShape(QFrame::NoFrame);
  scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll_area->setObjectName(QStringLiteral("PhScrollArea"));

  auto* scroll_content = new QWidget();
  scroll_content->setObjectName(QStringLiteral("ScrollAreaContent"));
  auto* sc_layout = new QVBoxLayout(scroll_content);
  sc_layout->setContentsMargins(12, 8, 12, 8);
  sc_layout->setSpacing(16);

  // ── 空态提示（原 card1：btn_grid 移除后简化，D1/D2） ──
  auto* empty_card = new QFrame(scroll_content);
  empty_card->setObjectName(QStringLiteral("PhEmptyCard"));
  auto* empty_layout = new QVBoxLayout(empty_card);
  empty_layout->setContentsMargins(14, 20, 14, 20);
  empty_layout->setSpacing(8);

  auto* empty_icon = new QLabel(empty_card);
  empty_icon->setAlignment(Qt::AlignCenter);
  empty_icon->setPixmap(
      etest::core_ui::AppIconProvider::instance()
          .icon(QStringLiteral("folder"))
          .pixmap(22, 22));
  empty_layout->addWidget(empty_icon);

  auto* empty_title = new QLabel(QStringLiteral("没有打开任何项目"), empty_card);
  empty_title->setObjectName(QStringLiteral("PhEmptyTitle"));
  empty_title->setAlignment(Qt::AlignCenter);
  empty_layout->addWidget(empty_title);

  auto* empty_desc =
      new QLabel(QStringLiteral("通过「新建项目」或欢迎页开始"), empty_card);
  empty_desc->setObjectName(QStringLiteral("PhEmptyDesc"));
  empty_desc->setAlignment(Qt::AlignCenter);
  empty_desc->setWordWrap(true);
  empty_layout->addWidget(empty_desc);

  sc_layout->addWidget(empty_card);

  // ── 最近项目（分区标题 + 紧凑列表行，空时整节隐藏 D9） ──
  recent_projects_section_ = new QWidget(scroll_content);
  auto* rp_section_layout = new QVBoxLayout(recent_projects_section_);
  rp_section_layout->setContentsMargins(0, 0, 0, 0);
  rp_section_layout->setSpacing(6);

  auto* rp_label =
      new QLabel(QStringLiteral("最近项目"), recent_projects_section_);
  rp_label->setObjectName(QStringLiteral("PhSectionLabel"));
  rp_section_layout->addWidget(rp_label);

  recent_projects_view_ = new QListView(recent_projects_section_);
  recent_projects_view_->setObjectName(QStringLiteral("PhRecentList"));
  recent_projects_view_->setFrameShape(QFrame::NoFrame);
  recent_projects_view_->setMouseTracking(true);
  recent_projects_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  recent_projects_model_ = new QStandardItemModel(this);
  recent_projects_view_->setModel(recent_projects_model_);
  auto* rp_delegate = new RecentProjOrFileDelegate(this);
  rp_delegate->setCloseButtonVisible(false);
  rp_delegate->setShowTime(true);  // 最近项目显示右侧时间
  recent_projects_view_->setItemDelegate(rp_delegate);
  rp_section_layout->addWidget(recent_projects_view_);

  sc_layout->addWidget(recent_projects_section_);

  // ── 最近文件 ──
  recent_files_section_ = new QWidget(scroll_content);
  auto* rf_section_layout = new QVBoxLayout(recent_files_section_);
  rf_section_layout->setContentsMargins(0, 0, 0, 0);
  rf_section_layout->setSpacing(6);

  auto* rf_label = new QLabel(QStringLiteral("最近文件"), recent_files_section_);
  rf_label->setObjectName(QStringLiteral("PhSectionLabel"));
  rf_section_layout->addWidget(rf_label);

  recent_files_view_ = new QListView(recent_files_section_);
  recent_files_view_->setObjectName(QStringLiteral("PhRecentList"));
  recent_files_view_->setFrameShape(QFrame::NoFrame);
  recent_files_view_->setMouseTracking(true);
  recent_files_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  recent_files_model_ = new QStandardItemModel(this);
  recent_files_view_->setModel(recent_files_model_);
  auto* rf_delegate = new RecentProjOrFileDelegate(this);
  rf_delegate->setCloseButtonVisible(false);
  recent_files_view_->setItemDelegate(rf_delegate);
  recent_files_view_->setFixedHeight(200);
  rf_section_layout->addWidget(recent_files_view_);

  sc_layout->addWidget(recent_files_section_);

  sc_layout->addStretch();

  scroll_area->setWidget(scroll_content);
  ph_layout->addWidget(scroll_area);

  stack_->addWidget(page_default_);  // index 0

  // ── 模式 1：领域分类树 + 已打开文件列表 ──
  page_project_ = new QWidget(this);

  tree_splitter_ = new QSplitter(Qt::Vertical, page_project_);

  // 已打开文件区域
  open_files_widget_ = new QWidget();
  auto* of_layout = new QVBoxLayout(open_files_widget_);
  of_layout->setContentsMargins(0, 0, 0, 0);
  of_layout->setSpacing(0);

  open_files_header_label_ = new QLabel(open_files_widget_);
  open_files_header_label_->setObjectName(QStringLiteral("PhSectionLabel"));
  open_files_header_label_->setText(QStringLiteral("已打开 (0)"));
  of_layout->addWidget(open_files_header_label_);

  open_files_view_ = new QListView();
  open_files_view_->setObjectName(QStringLiteral("PhRecentList"));
  open_files_view_->setFrameShape(QFrame::NoFrame);
  open_files_view_->setMouseTracking(true);
  open_files_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  open_files_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  open_files_model_ = new QStandardItemModel(this);
  open_files_view_->setModel(open_files_model_);
  open_file_delegate_ = new RecentProjOrFileDelegate(this);
  open_files_view_->setItemDelegate(open_file_delegate_);
  of_layout->addWidget(open_files_view_);

  tree_view_ = new QTreeView();
  tree_view_->setHeaderHidden(true);
  tree_view_->setAnimated(true);
  tree_view_->setEditTriggers(QAbstractItemView::SelectedClicked |
                              QAbstractItemView::EditKeyPressed);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->setDragDropMode(QAbstractItemView::NoDragDrop);
  tree_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  tree_view_->setIndentation(16);
  tree_view_->setIconSize(QSize(16, 16));
  tree_view_->setMouseTracking(true);

  tree_delegate_ = new ProjectTreeDelegate(this);
  tree_view_->setItemDelegate(tree_delegate_);

  model_ = new QStandardItemModel(this);
  tree_view_->setModel(model_);

  tree_splitter_->addWidget(tree_view_);
  tree_splitter_->addWidget(open_files_widget_);
  tree_splitter_->setStretchFactor(0, 1);
  tree_splitter_->setStretchFactor(1, 0);
  tree_splitter_->setSizes({600, 200});

  auto* page_layout = new QVBoxLayout(page_project_);
  page_layout->setContentsMargins(0, 0, 0, 0);
  page_layout->setSpacing(0);
  page_layout->addWidget(tree_splitter_);

  stack_->addWidget(page_project_);  // index 1

  layout->addWidget(stack_);

  // ── 文件监视器 ──
  file_watcher_ = new QFileSystemWatcher(this);

  // ── 防抖定时器 ──
  debounce_timer_ = new QTimer(this);
  debounce_timer_->setSingleShot(true);
  debounce_timer_->setInterval(200);

  // 默认显示无项目模式
  stack_->setCurrentIndex(0);
}

void ProjectStructureWidget::initSignals() {
  // 文件监视器
  connect(file_watcher_, &QFileSystemWatcher::directoryChanged, this,
          &ProjectStructureWidget::onDirectoryChanged);

  // 防抖定时器
  connect(debounce_timer_, &QTimer::timeout, this, [this]() {
    for (const auto& path : debounce_timer_queued_paths_) {
      if (suppressed_watch_paths_.contains(path)) {
        // 刚新建文件的目录,跳过 watcher 刷新以保护 inline editor
        suppressed_watch_paths_.remove(path);
        continue;
      }
      refreshCategory(path);
      emit directoryContentChanged(path);
    }
    debounce_timer_queued_paths_.clear();
  });

  // 树视图信号
  connect(tree_view_, &QTreeView::customContextMenuRequested, this,
          &ProjectStructureWidget::onCustomContextMenu);
  connect(tree_view_, &QTreeView::doubleClicked, this,
          &ProjectStructureWidget::onItemDoubleClicked);
  connect(model_, &QStandardItemModel::itemChanged, this,
          &ProjectStructureWidget::onItemChanged);

  // ── 根节点操作按钮（delegate 信号）──
  connect(tree_delegate_, &ProjectTreeDelegate::refreshRequested, this,
          [this]() {
            LOG_INFO("PROJECT_UI", "点击根节点「刷新」按钮");
            model_->clear();
            root_item_ = nullptr;
            tree_delegate_->resetShowAll(true);
            buildTree();
          });
  connect(tree_delegate_, &ProjectTreeDelegate::showAllToggled, this,
          [this](bool showAll) {
            LOG_INFO("PROJECT_UI", "点击根节点「显示全部」按钮 showAll={}",
                     showAll);
            if (!root_item_) {
              return;
            }
            for (int i = 0; i < root_item_->rowCount(); ++i) {
              auto* child = root_item_->child(i);
              if (child && child->data(CategoryIdRole).toString() == "other") {
                tree_view_->setRowHidden(i, root_item_->index(), !showAll);
                break;
              }
            }
          });
  connect(tree_delegate_, &ProjectTreeDelegate::syncRequested, this, [this]() {
    LOG_INFO("PROJECT_UI", "点击根节点「同步文档」按钮");
    emit syncCurrentEditorRequested();
  });
  connect(tree_delegate_, &ProjectTreeDelegate::syncDocEnabledChanged, this,
          &ProjectStructureWidget::syncDocEnabledChanged);

  // ── 已打开文件列表 ──
  connect(open_files_view_, &QListView::clicked, this,
          [this](const QModelIndex& index) {
            QString path = index.data(FilePathRole).toString();
            LOG_INFO("PROJECT_UI", "点击已打开文件 [path={}]",
                     path.toStdString());
            if (!path.isEmpty())
              emit openFileActivateRequested(path);
          });
  connect(open_files_view_, &QListView::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            QModelIndex index = open_files_view_->indexAt(pos);
            if (!index.isValid())
              return;
            QString path = index.data(FilePathRole).toString();
            if (path.isEmpty())
              return;

            QMenu menu(open_files_view_);
            menu.setObjectName(QStringLiteral("PhRecentContextMenu"));
            auto* closeAction = menu.addAction(QStringLiteral("关闭文件"));
            if (menu.exec(open_files_view_->viewport()->mapToGlobal(pos)) ==
                closeAction) {
              emit openFileCloseRequested(path);
            }
          });
  connect(open_file_delegate_, &RecentProjOrFileDelegate::closeRequested, this,
          &ProjectStructureWidget::openFileCloseRequested);

  // ── 最近项目列表 ──
  if (recent_projects_view_) {
    connect(recent_projects_view_, &QListView::clicked, this,
            [this](const QModelIndex& index) {
              QString path = index.data(FilePathRole).toString();
              LOG_INFO("PROJECT_UI", "点击最近项目 [path={}]",
                       QFileInfo(path).fileName().toStdString());
              if (!path.isEmpty())
                emit projectOpenRequested(path);
            });
    connect(
        recent_projects_view_, &QListView::customContextMenuRequested, this,
        [this](const QPoint& pos) {
          QModelIndex index = recent_projects_view_->indexAt(pos);
          if (!index.isValid())
            return;
          QString path = index.data(FilePathRole).toString();
          if (path.isEmpty())
            return;

          QMenu menu(recent_projects_view_);
          menu.setObjectName(QStringLiteral("PhRecentContextMenu"));
          auto* openAction = menu.addAction(QStringLiteral("打开"));
          auto* copyPathAction = menu.addAction(QStringLiteral("复制路径"));
          auto* openDirAction = menu.addAction(QStringLiteral("打开所在目录"));
          menu.addSeparator();
          auto* removeAction = menu.addAction(QStringLiteral("从列表中移除"));
          QAction* chosen =
              menu.exec(recent_projects_view_->viewport()->mapToGlobal(pos));
          if (chosen == openAction) {
            emit projectOpenRequested(path);
          } else if (chosen == copyPathAction) {
            QApplication::clipboard()->setText(path);
          } else if (chosen == openDirAction) {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
          } else if (chosen == removeAction) {
            auto& cfg = etest::core::config::ConfigManager::instance();
            QStringList list = cfg.get<QStringList>(
                etest::core::config::CONFIG_RECENT_PROJECT_LIST);
            list.removeAll(path);
            cfg.set(etest::core::config::CONFIG_RECENT_PROJECT_LIST, list);
            refreshRecentProjects();
          }
        });
  }

  // ── 最近文件列表 ──
  if (recent_files_view_) {
    connect(recent_files_view_, &QListView::clicked, this,
            [this](const QModelIndex& index) {
              QString path = index.data(FilePathRole).toString();
              LOG_INFO("PROJECT_UI", "点击最近文件 [path={}]",
                       QFileInfo(path).fileName().toStdString());
              if (!path.isEmpty())
                emit recentFileOpenRequested(path);
            });
    connect(
        recent_files_view_, &QListView::customContextMenuRequested, this,
        [this](const QPoint& pos) {
          QModelIndex index = recent_files_view_->indexAt(pos);
          if (!index.isValid())
            return;
          QString path = index.data(FilePathRole).toString();
          if (path.isEmpty())
            return;

          QMenu menu(recent_files_view_);
          menu.setObjectName(QStringLiteral("PhRecentContextMenu"));
          auto* openAction = menu.addAction(QStringLiteral("打开"));
          auto* copyPathAction = menu.addAction(QStringLiteral("复制路径"));
          auto* openDirAction = menu.addAction(QStringLiteral("打开所在目录"));
          menu.addSeparator();
          auto* removeAction = menu.addAction(QStringLiteral("从列表中移除"));
          QAction* chosen =
              menu.exec(recent_files_view_->viewport()->mapToGlobal(pos));
          if (chosen == openAction) {
            emit recentFileOpenRequested(path);
          } else if (chosen == copyPathAction) {
            QApplication::clipboard()->setText(path);
          } else if (chosen == openDirAction) {
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
          } else if (chosen == removeAction) {
            auto& cfg = etest::core::config::ConfigManager::instance();
            QStringList list = cfg.get<QStringList>(
                etest::core::config::CONFIG_RECENT_FILE_LIST);
            list.removeAll(path);
            cfg.set(etest::core::config::CONFIG_RECENT_FILE_LIST, list);
            refreshRecentFiles();
          }
        });
  }

  // 最近项目/文件变更时刷新
  connect(
      &etest::core::config::ConfigManager::instance(),
      &etest::core::config::ConfigManager::configChanged, this,
      [this](const QString& key) {
        if (key ==
            QLatin1String(etest::core::config::CONFIG_RECENT_PROJECT_LIST)) {
          refreshRecentProjects();
        } else if (key == QLatin1String(
                              etest::core::config::CONFIG_RECENT_FILE_LIST)) {
          refreshRecentFiles();
        }
      });

  // 硬件节点自动刷新
  connectHardwareRefresh();
}

void ProjectStructureWidget::setProjectPath(const QString& path) {
  if (path == project_path_)
    return;
  project_path_ = path;

  model_->clear();
  root_item_ = nullptr;

  buildTree();

  stack_->setCurrentIndex(1);
}

void ProjectStructureWidget::clearProjectPath() {
  project_path_.clear();

  if (!file_watcher_->files().isEmpty()) {
    file_watcher_->removePaths(file_watcher_->files());
  }
  if (!file_watcher_->directories().isEmpty()) {
    file_watcher_->removePaths(file_watcher_->directories());
  }

  model_->clear();
  root_item_ = nullptr;

  // 项目关闭后重置显示全部状态，避免新项目树与按钮图标不一致
  tree_delegate_->resetShowAll(true);
  // 注意：SyncDoc 自动跟随状态有意跨项目保留（用户偏好），不做重置

  clearOpenFiles();

  stack_->setCurrentIndex(0);
}

QString ProjectStructureWidget::projectPath() const {
  return project_path_;
}

// ── 标准分类定义 ──

QList<CategoryInfo> ProjectStructureWidget::defaultCategories() const {
  return {
      {QStringLiteral("protocol"), QStringLiteral("协议"),
       QStringLiteral("protocol/"), QStringLiteral("protocol"),
       QStringLiteral("eprotox"), QStringLiteral("新建协议文件")},
      {QStringLiteral("topology"), QStringLiteral("拓扑"),
       QStringLiteral("topology/"), QStringLiteral("topo_tap"),
       QStringLiteral("etopo"), QStringLiteral("新建拓扑文件")},
      {QStringLiteral("testprog"), QStringLiteral("测试程序"),
       QStringLiteral("cases/"), QStringLiteral("testprogram"),
       QStringLiteral("etprog"), QStringLiteral("新建测试程序")},
      {QStringLiteral("script"), QStringLiteral("脚本"),
       QStringLiteral("scripts/"), QStringLiteral("file_lua"),
       QStringLiteral("lua"), QStringLiteral("新建脚本")},
      {QStringLiteral("report"), QStringLiteral("报告"),
       QStringLiteral("reports/"), QStringLiteral("file_generic"), QString(),
       QString()},
      {QStringLiteral("config"), QStringLiteral("配置"),
       QStringLiteral("config/"), QStringLiteral("file_json"),
       QStringLiteral("json"), QStringLiteral("新建配置文件")},
      {QStringLiteral("run"), QStringLiteral("运行"), QStringLiteral("run/"),
       QStringLiteral("run"), QStringLiteral("erun"),
       QStringLiteral("新建运行配置")},
      {QStringLiteral("backup"), QStringLiteral("备份"),
       QStringLiteral("backup/"), QStringLiteral("file_generic"), QString(),
       QString()},
  };
}

// ── 树构建 ──

void ProjectStructureWidget::buildTree() {
  QDir projectDir(project_path_);
  if (!projectDir.exists())
    return;

  // 根节点：项目名称
  root_item_ = new QStandardItem(projectDir.dirName());
  root_item_->setData(QStringLiteral("root"), NodeTypeRole);
  root_item_->setEditable(false);
  QFont rootFont = root_item_->font();
  rootFont.setBold(true);
  root_item_->setFont(rootFont);
  root_item_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("project")));
  model_->appendRow(root_item_);

  QStringList watchedDirs;

  for (const auto& cat : defaultCategories()) {
    QString fullPath = projectDir.absoluteFilePath(cat.dirPath);
    QDir catDir(fullPath);

    bool dirExists = catDir.exists();
    int fileCount = 0;
    QStandardItem* catItem = nullptr;

    if (dirExists) {
      QStringList filters;
      filters << QStringLiteral("*");
      QFileInfoList entries =
          catDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
      fileCount = entries.size();

      catItem = createCategoryItem(cat, fileCount);

      if (cat.id == "report") {
        populateReportCategory(catItem, entries, cat.dirPath);
      } else if (cat.id == "backup") {
        populateBackupCategory(catItem, entries, cat.dirPath);
      } else {
        for (const auto& fi : entries) {
          QString relPath = cat.dirPath + fi.fileName();
          catItem->appendRow(createFileItem(fi.fileName(), relPath));
        }
      }

      watchedDirs.append(fullPath);
    } else {
      // 目录缺失，灰色显示
      catItem = createCategoryItem(cat, 0);
      QString displayText = cat.displayName + QStringLiteral(" （目录缺失）");
      catItem->setText(displayText);
      catItem->setForeground(QColor(0x99, 0x99, 0x99));
    }

    catItem->setEditable(false);
    root_item_->appendRow(catItem);
  }

  // "其他文件" 分类
  CategoryInfo otherCat{QStringLiteral("other"),
                        QStringLiteral("其他文件"),
                        QString(),
                        QStringLiteral("file_generic"),
                        QString(),
                        QString()};
  auto* otherItem = createCategoryItem(otherCat, 0);
  otherItem->setEditable(false);

  int otherCount = 0;
  QString projectPathLower = project_path_.toLower() + QStringLiteral("/");
  QStringList skipPrefixes;
  for (const auto& cat : defaultCategories()) {
    QString absDir = QDir(project_path_).absoluteFilePath(cat.dirPath);
    skipPrefixes.append(absDir.toLower() + QStringLiteral("/"));
  }
  // Also skip hardware/ directory (reserved, but no longer a standard category)
  skipPrefixes.append(QDir(project_path_)
                          .absoluteFilePath(QStringLiteral("hardware/"))
                          .toLower() +
                      QStringLiteral("/"));

  QDirIterator it(project_path_, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    it.next();
    QString absPath = it.filePath();
    bool inStandardDir = false;
    for (const auto& prefix : skipPrefixes) {
      if (absPath.toLower().startsWith(prefix)) {
        inStandardDir = true;
        break;
      }
    }
    if (inStandardDir)
      continue;

    QFileInfo fi = it.fileInfo();
    QString relPath = projectDir.relativeFilePath(fi.absoluteFilePath());
    otherItem->appendRow(createFileItem(fi.fileName(), relPath));
    ++otherCount;
  }

  QString otherText = otherCat.displayName + QStringLiteral(" (") +
                      QString::number(otherCount) + QStringLiteral(")");
  if (otherCount == 0) {
    otherItem->setForeground(QColor(0xbb, 0xbb, 0xbb));
  }
  otherItem->setText(otherText);
  root_item_->appendRow(otherItem);

  // ── 硬件节点（从拓扑文件解析，非文件系统）──
  auto* hwItem = createCategoryItem(
      {QStringLiteral("hardware"), QStringLiteral("硬件"), QString(),
       QStringLiteral("hardware"), QString(), QString()},
      0);
  QFont hwFont = hwItem->font();
  hwFont.setItalic(true);
  hwItem->setFont(hwFont);
  hwItem->setToolTip(
      QStringLiteral("项目拓扑文件中引用的硬件设备列表\n"
                     "平台插件加载后自动匹配设备状态"));
  root_item_->appendRow(hwItem);

  // 展开根节点
  tree_view_->expand(root_item_->index());

  // 开始监视
  if (!watchedDirs.isEmpty()) {
    file_watcher_->addPaths(watchedDirs);
  }

  // 首次刷新硬件节点
  refreshHardwareDevices();
  emit fileListChanged();
}

void ProjectStructureWidget::refreshCategory(const QString& dirPath) {
  if (project_path_.isEmpty() || !root_item_)
    return;

  QDir dir(dirPath);
  if (!dir.exists())
    return;

  // 找到对应的分类 item
  QStandardItem* catItem = nullptr;
  QString catId;
  for (const auto& cat : defaultCategories()) {
    QString fullPath = QDir(project_path_).absoluteFilePath(cat.dirPath);
    if (fullPath == dirPath) {
      catId = cat.id;
      break;
    }
  }

  if (catId.isEmpty())
    return;

  for (int i = 0; i < root_item_->rowCount(); ++i) {
    auto* child = root_item_->child(i);
    if (child->data(CategoryIdRole).toString() == catId) {
      catItem = child;
      break;
    }
  }

  if (!catItem)
    return;

  catItem->removeRows(0, catItem->rowCount());

  QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
  int fileCount = 0;

  if (catId == "report") {
    QString dirPath = QDir(project_path_).relativeFilePath(dir.absolutePath()) +
                      QStringLiteral("/");
    populateReportCategory(catItem, entries, dirPath);
    fileCount = catItem->rowCount();
  } else if (catId == "backup") {
    QString dirPath = QDir(project_path_).relativeFilePath(dir.absolutePath()) +
                      QStringLiteral("/");
    populateBackupCategory(catItem, entries, dirPath);
    fileCount = catItem->rowCount();
  } else {
    for (const auto& fi : entries) {
      QString relPath =
          QDir(project_path_).relativeFilePath(fi.absoluteFilePath());
      catItem->appendRow(createFileItem(fi.fileName(), relPath));
      ++fileCount;
    }
  }

  QString baseName = catItem->data(Qt::DisplayRole).toString();
  int parenIdx = baseName.indexOf(QStringLiteral(" ("));
  if (parenIdx > 0) {
    baseName = baseName.left(parenIdx);
  }
  catItem->setText(baseName + QStringLiteral(" (") +
                   QString::number(fileCount) + QStringLiteral(")"));

  // 捕获目录被删除后重建的场景：watcher 不会自动恢复，需重新注册
  if (!file_watcher_->directories().contains(dirPath)) {
    file_watcher_->addPath(dirPath);
  }

  emit fileListChanged();
}

void ProjectStructureWidget::onDirectoryChanged(const QString& path) {
  debounce_timer_queued_paths_.insert(path);
  debounce_timer_->start();
}

// ── 槽函数 ──

void ProjectStructureWidget::onCustomContextMenu(const QPoint& pos) {
  LOG_INFO("PROJECT_UI", "文件树右键菜单");
  QModelIndex index = tree_view_->indexAt(pos);
  QMenu menu(this);

  if (!index.isValid()) {
    // 空白区域右键
    auto* newFileAction =
        menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                       QStringLiteral("新建文件"));
    auto* newDirAction =
        menu.addAction(QIcon::fromTheme(QStringLiteral("folder-new")),
                       QStringLiteral("新建文件夹"));
    menu.addSeparator();
    auto* openInFileManagerAction =
        menu.addAction(QStringLiteral("在文件系统中打开"));

    QAction* chosen = menu.exec(tree_view_->viewport()->mapToGlobal(pos));

    if (chosen == newFileAction) {
      createNewFile(QString(), QString(), QStringLiteral("新建文件"));
    } else if (chosen == newDirAction) {
      // 在项目根创建新目录
      QString dirName =
          newFileBaseName(QStringLiteral("新建文件夹"), project_path_);
      QDir(project_path_).mkdir(dirName);
    } else if (chosen == openInFileManagerAction) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(project_path_));
    }
    return;
  }

  QStandardItem* item = model_->itemFromIndex(index);
  QString nodeType = item->data(NodeTypeRole).toString();

  if (nodeType == QStringLiteral("hardware_device")) {
    // 硬件设备节点右键 — 导航到平台设备树
    auto* navigateAction = menu.addAction(QStringLiteral("跳转到设备树"));

    if (menu.exec(tree_view_->viewport()->mapToGlobal(pos)) == navigateAction) {
      QString deviceType = item->data(Qt::UserRole + 1).toString();
      QString pluginId = item->data(Qt::UserRole + 2).toString();
      emit hardwareDeviceNavigateRequested(deviceType, pluginId);
    }
    return;
  }

  if (nodeType == QStringLiteral("category")) {
    // 分类节点右键
    QString catId = item->data(CategoryIdRole).toString();
    bool hasNewAction = false;

    for (const auto& cat : defaultCategories()) {
      if (cat.id == catId && !cat.newFileExt.isEmpty()) {
        hasNewAction = true;
        auto* newAction = menu.addAction(
            AppIconProvider::instance().icon(cat.iconName), cat.newFileLabel);
        menu.addSeparator();
        auto* openInFmAction =
            menu.addAction(QStringLiteral("在文件系统中打开"));

        QAction* chosen = menu.exec(tree_view_->viewport()->mapToGlobal(pos));
        if (chosen == newAction) {
          createNewFile(catId, cat.newFileExt, cat.newFileLabel);
        } else if (chosen == openInFmAction) {
          QString fullPath = QDir(project_path_).absoluteFilePath(cat.dirPath);
          QDesktopServices::openUrl(QUrl::fromLocalFile(fullPath));
        }
        break;
      }
    }

    if (!hasNewAction) {
      // 不支持新建的分类（硬件节点为计算节点，无文件系统对应目录）
      // 硬件节点不显示"在文件系统中打开"
      if (catId == QLatin1String("hardware")) {
        auto* refreshAction = menu.addAction(QStringLiteral("刷新硬件设备"));
        if (menu.exec(tree_view_->viewport()->mapToGlobal(pos)) ==
            refreshAction) {
          refreshHardwareDevices();
        }
      } else if (catId == QLatin1String("report")) {
        // 测试报告目录：添加"清除所有"菜单项
        auto* clearAllAction = menu.addAction(
            AppIconProvider::instance().icon(QStringLiteral("delete")),
            QStringLiteral("清除所有"));
        clearAllAction->setData(QVariant::fromValue(true));  // 标记为危险操作
        menu.addSeparator();
        auto* openInFmAction =
            menu.addAction(QStringLiteral("在文件系统中打开"));

        QAction* chosen = menu.exec(tree_view_->viewport()->mapToGlobal(pos));
        if (chosen == clearAllAction) {
          clearAllEtlogFiles(catId);
        } else if (chosen == openInFmAction) {
          QString dirPath = categoryDirPath(catId);
          QDesktopServices::openUrl(QUrl::fromLocalFile(
              QDir(project_path_).absoluteFilePath(dirPath)));
        }
      } else {
        auto* openInFmAction =
            menu.addAction(QStringLiteral("在文件系统中打开"));
        if (menu.exec(tree_view_->viewport()->mapToGlobal(pos)) ==
            openInFmAction) {
          QString dirPath = categoryDirPath(catId);
          if (dirPath.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(project_path_));
          } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                QDir(project_path_).absoluteFilePath(dirPath)));
          }
        }
      }
    }
  } else if (nodeType == QStringLiteral("file")) {
    // 文件节点右键
    auto* openAction = menu.addAction(QStringLiteral("打开"));
    menu.addSeparator();
    auto* renameAction = menu.addAction(QStringLiteral("重命名"));
    auto* deleteAction =
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                       QStringLiteral("删除"));
    deleteAction->setData(QVariant::fromValue(true));  // 标记为危险操作
    menu.addSeparator();
    auto* copyPathAction = menu.addAction(QStringLiteral("复制路径"));
    auto* copyRelAction = menu.addAction(QStringLiteral("复制相对路径"));
    menu.addSeparator();
    auto* openInFmAction = menu.addAction(QStringLiteral("在文件系统中打开"));
    auto* openTextAction = menu.addAction(QStringLiteral("用文本编辑器打开"));

    // .erun 文件追加「设为当前运行配置」（写 .etproj settings.runConfigFile）
    QString relPath = item->data(RelativePathRole).toString();
    QAction* setAsRunAction = nullptr;
    if (relPath.endsWith(QStringLiteral(".erun"))) {
      menu.addSeparator();
      setAsRunAction = menu.addAction(QStringLiteral("设为当前运行配置"));
    }

    QAction* chosen = menu.exec(tree_view_->viewport()->mapToGlobal(pos));

    if (chosen == openAction) {
      emit fileOpenRequested(absolutePath(relPath));
    } else if (setAsRunAction && chosen == setAsRunAction) {
      etest::core::project::ProjectManager::instance().setSetting(
          QStringLiteral("runConfigFile"), relPath);
    } else if (chosen == renameAction) {
      if (relPath.isEmpty()) {
        return;
      }
      rename_old_path_ = absolutePath(relPath);
      tree_view_->edit(index);
    } else if (chosen == deleteAction) {
      deleteSelectedFile();
    } else if (chosen == copyPathAction) {
      copyFilePath();
    } else if (chosen == copyRelAction) {
      copyRelativePath();
    } else if (chosen == openInFmAction) {
      openInFileManager();
    } else if (chosen == openTextAction) {
      openWithTextEditor();
    }
  }
}

void ProjectStructureWidget::onItemDoubleClicked(const QModelIndex& index) {
  LOG_INFO("PROJECT_UI", "文件树双击打开");
  QStandardItem* item = model_->itemFromIndex(index);
  if (!item)
    return;
  QString nodeType = item->data(NodeTypeRole).toString();
  if (nodeType == QStringLiteral("hardware_device")) {
    QString deviceType = item->data(Qt::UserRole + 1).toString();
    QString pluginId = item->data(Qt::UserRole + 2).toString();
    emit hardwareDeviceNavigateRequested(deviceType, pluginId);
    return;
  }
  if (nodeType != QStringLiteral("file"))
    return;

  QString relPath = item->data(RelativePathRole).toString();
  emit fileOpenRequested(absolutePath(relPath));
}

void ProjectStructureWidget::onItemChanged(QStandardItem* item) {
  if (!item || rename_old_path_.isEmpty())
    return;
  if (item->data(NodeTypeRole).toString() != QStringLiteral("file"))
    return;

  // 取出用户编辑后的文件名
  QString newFileName = item->text().trimmed();
  if (newFileName.isEmpty()) {
    // 用户清空了文件名,取消编辑
    QSignalBlocker blocker(model_);
    item->setText(QFileInfo(rename_old_path_).fileName());
    rename_old_path_.clear();
    return;
  }

  QFileInfo oldFi(rename_old_path_);
  QString oldFileName = oldFi.fileName();
  if (newFileName == oldFileName) {
    // 没有变化,直接重置
    rename_old_path_.clear();
    return;
  }

  // 构造新路径:目录不变,文件名更新
  QString newPath = oldFi.absolutePath() + QStringLiteral("/") + newFileName;

  // 检查源文件存在且目标不存在
  QFile src(rename_old_path_);
  if (!src.exists()) {
    rename_old_path_.clear();
    return;
  }
  if (QFile::exists(newPath)) {
    // 目标已存在,还原
    QSignalBlocker blocker(model_);
    item->setText(oldFileName);
    QMessageBox::warning(this, QStringLiteral("重命名失败"),
                         QStringLiteral("目标文件已存在: %1").arg(newFileName));
    rename_old_path_.clear();
    return;
  }

  if (!src.rename(newPath)) {
    QSignalBlocker blocker(model_);
    item->setText(oldFileName);
    QMessageBox::warning(this, QStringLiteral("重命名失败"),
                         QStringLiteral("无法重命名文件: %1").arg(newFileName));
    rename_old_path_.clear();
    return;
  }

  // 更新 item 的 RelativePathRole
  QDir projectDir(project_path_);
  QString newRelPath = projectDir.relativeFilePath(newPath);
  {
    QSignalBlocker blocker(model_);
    item->setData(newRelPath, RelativePathRole);
  }

  emit fileRenamed(rename_old_path_, newPath);
  rename_old_path_.clear();
}

// ── 文件操作 ──

void ProjectStructureWidget::createNewFile(const QString& categoryId,
                                           const QString& extension,
                                           const QString& baseName) {
  QString targetDir;
  if (!categoryId.isEmpty()) {
    targetDir = categoryDirPath(categoryId);
  }
  if (targetDir.isEmpty()) {
    targetDir = project_path_;
  }

  QString fullDir = QDir(project_path_).absoluteFilePath(targetDir);
  QDir().mkpath(fullDir);

  QString fileName;
  QString fullPath;
  bool created = false;
  // 向导创建的文件名已由信息页确定，跳过结尾的行内重命名（避免文件名与
  // .etprog 内部 suite.name 不一致）
  bool skipRename = false;

  if (extension == QStringLiteral("etprog")) {
    // 测试程序文件走完整向导（模板/信息/用例与步骤/完成），名称由向导决定
    TestProgramWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted) {
      return;
    }
    TestProgramData suite = wizard.resultProgram();
    fileName = suite.name + QStringLiteral(".etprog");
    fullPath = QDir(fullDir).absoluteFilePath(fileName);
    // 兜底守卫（向导信息页已预检，此处二次校验）
    if (QFile::exists(fullPath)) {
      QMessageBox::warning(this, QStringLiteral("新建失败"),
                           QStringLiteral("文件已存在：%1").arg(fullPath));
      return;
    }
    created = saveTestProgram(fullPath, suite);
    skipRename = true;
    LOG_INFO("PROJECT_UI", "向导新建测试程序 [name={}] [version={}] [author={}]",
             suite.name.toStdString(), suite.version.toStdString(),
             suite.author.toStdString());
  } else if (extension == QStringLiteral("eprotox")) {
    // 协议文件走完整向导（模板/帧属性/字段/完成），名称由向导决定
    ProtocolFileWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted) {
      return;
    }
    icd::Frame frame = wizard.resultFrame();
    const int frameId = frame.id();
    fileName = QString::fromStdString(std::string(frame.name())) +
               QStringLiteral(".eprotox");
    fullPath = QDir(fullDir).absoluteFilePath(fileName);
    // 兜底守卫（向导信息页已预检，此处二次校验）
    if (QFile::exists(fullPath)) {
      QMessageBox::warning(this, QStringLiteral("新建失败"),
                           QStringLiteral("文件已存在：%1").arg(fullPath));
      return;
    }
    icd::Repository repo;
    repo.add_frame(std::make_unique<icd::Frame>(std::move(frame)));
    auto result = icd::format::serialize_xml_repository(
        etest::core::utils::toFsPath(fullPath), repo);
    if (!result) {
      QMessageBox::warning(
          this, QStringLiteral("新建失败"),
          QStringLiteral("写入协议文件失败：%1").arg(fullPath));
      return;
    }
    created = true;
    skipRename = true;
    LOG_INFO("PROJECT_UI", "向导新建协议文件 [name={}] [frameId={}]",
             fileName.toStdString(), frameId);
  } else if (extension == QStringLiteral("etopo")) {
    // 拓扑文件走完整向导（模板/设备&UUT/连线/完成），名称由向导决定
    TopologyFileWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted) {
      return;
    }
    const QString name = wizard.topologyName();
    fileName = name + QStringLiteral(".etopo");
    fullPath = QDir(fullDir).absoluteFilePath(fileName);
    // 兜底守卫（向导第 1 页已校验名称，此处二次校验）
    if (QFile::exists(fullPath)) {
      QMessageBox::warning(this, QStringLiteral("新建失败"),
                           QStringLiteral("文件已存在：%1").arg(fullPath));
      return;
    }
    etest::topology::TopologyDocument* doc = wizard.resultDocument();
    const QJsonObject root =
        etest::topology::TopologyJsonSerializer::serialize(*doc);
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
      QMessageBox::warning(this, QStringLiteral("新建失败"),
                           QStringLiteral("写入拓扑文件失败：%1").arg(fullPath));
      return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    created = true;
    skipRename = true;
    LOG_INFO("PROJECT_UI", "向导新建拓扑文件 [name={}]",
             fileName.toStdString());
  } else {
    fileName =
        newFileBaseName(baseName, fullDir) +
        (extension.isEmpty() ? QString() : QStringLiteral(".") + extension);
    fullPath = QDir(fullDir).absoluteFilePath(fileName);

    QFile file(fullPath);
    created = file.open(QIODevice::WriteOnly);
    if (created) {
      file.close();
    }
  }
  if (!created) {
    return;
  }

  emit fileCreated(fullPath);

  // 短暂抑制该目录的 watcher 刷新,避免 200ms 后的 refreshCategory 重建 model
  // 破坏刚打开的 inline editor(导致焦点丢失、editor 关闭)
  suppressed_watch_paths_.insert(fullDir);
  QTimer::singleShot(500, this, [this, fullDir]() {
    suppressed_watch_paths_.remove(fullDir);
  });

  // 同步把新文件 item 添加到 model 对应分类下(避免依赖 QFileSystemWatcher
  // 防抖刷新)
  QString effectiveCatId = categoryId;
  if (effectiveCatId.isEmpty()) {
    effectiveCatId = QStringLiteral("other");
  }
  QString relPath = targetDir + fileName;

  QStandardItem* catItem = nullptr;
  if (root_item_) {
    for (int i = 0; i < root_item_->rowCount(); ++i) {
      auto* child = root_item_->child(i);
      if (child->data(CategoryIdRole).toString() == effectiveCatId) {
        catItem = child;
        break;
      }
    }
  }

  QStandardItem* newItem = nullptr;
  if (catItem) {
    newItem = createFileItem(fileName, relPath);
    catItem->appendRow(newItem);
    // 刷新分类标题中的文件计数
    int fileCount = catItem->rowCount();
    QString baseName = catItem->data(Qt::DisplayRole).toString();
    int parenIdx = baseName.indexOf(QStringLiteral(" ("));
    if (parenIdx > 0) {
      baseName = baseName.left(parenIdx);
    }
    catItem->setText(baseName + QStringLiteral(" (") +
                     QString::number(fileCount) + QStringLiteral(")"));
  }

  // 选中并进入重命名模式（向导创建的文件名已定，跳过行内重命名）
  if (newItem) {
    QModelIndex newIndex = newItem->index();
    tree_view_->setCurrentIndex(newIndex);
    tree_view_->scrollTo(newIndex);
    if (!skipRename) {
      rename_old_path_ = fullPath;
      tree_view_->edit(newIndex);
    }
  }
}

void ProjectStructureWidget::deleteSelectedFile() {
  QModelIndex index = tree_view_->currentIndex();
  if (!index.isValid())
    return;

  QStandardItem* item = model_->itemFromIndex(index);
  if (!item)
    return;
  if (item->data(NodeTypeRole).toString() != QStringLiteral("file"))
    return;

  QString relPath = item->data(RelativePathRole).toString();
  QString absPath = absolutePath(relPath);

  QFileInfo fi(absPath);
  QString msg = QStringLiteral("确定要删除 \"%1\" 吗？").arg(fi.fileName());
  auto ret = QMessageBox::question(this, QStringLiteral("确认删除"), msg,
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
  if (ret != QMessageBox::Yes)
    return;

  if (QFile::remove(absPath)) {
    emit fileDeleted(absPath);
  }
}

void ProjectStructureWidget::copyFilePath() {
  QModelIndex index = tree_view_->currentIndex();
  if (!index.isValid())
    return;
  QStandardItem* item = model_->itemFromIndex(index);
  if (!item)
    return;

  QString relPath = item->data(RelativePathRole).toString();
  if (relPath.isEmpty())
    return;

  QApplication::clipboard()->setText(absolutePath(relPath));
}

void ProjectStructureWidget::copyRelativePath() {
  QModelIndex index = tree_view_->currentIndex();
  if (!index.isValid())
    return;
  QStandardItem* item = model_->itemFromIndex(index);
  if (!item)
    return;

  QString relPath = item->data(RelativePathRole).toString();
  if (relPath.isEmpty())
    return;

  QApplication::clipboard()->setText(relPath);
}

void ProjectStructureWidget::openInFileManager() {
  QModelIndex index = tree_view_->currentIndex();
  if (!index.isValid())
    return;
  QStandardItem* item = model_->itemFromIndex(index);
  if (!item)
    return;

  QString relPath = item->data(RelativePathRole).toString();
  if (relPath.isEmpty())
    return;

  QString absPath = absolutePath(relPath);
  QFileInfo fi(absPath);
  QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

void ProjectStructureWidget::openWithTextEditor() {
  QModelIndex index = tree_view_->currentIndex();
  if (!index.isValid())
    return;
  QStandardItem* item = model_->itemFromIndex(index);
  if (!item)
    return;

  QString relPath = item->data(RelativePathRole).toString();
  if (relPath.isEmpty())
    return;

  emit fileOpenAsTextRequested(absolutePath(relPath));
}

// ── 工具方法 ──

QStandardItem* ProjectStructureWidget::createCategoryItem(
    const CategoryInfo& info,
    int fileCount) {
  QString displayText = info.displayName + QStringLiteral(" (") +
                        QString::number(fileCount) + QStringLiteral(")");
  auto* item = new QStandardItem(displayText);
  item->setData(QStringLiteral("category"), NodeTypeRole);
  item->setData(info.id, CategoryIdRole);
  item->setEditable(false);
  item->setIcon(AppIconProvider::instance().icon(info.iconName));
  return item;
}

void ProjectStructureWidget::populateReportCategory(
    QStandardItem* catItem,
    const QFileInfoList& entries,
    const QString& dirPath) {
  QFileInfoList sorted = entries;
  std::sort(sorted.begin(), sorted.end(),
            [](const QFileInfo& a, const QFileInfo& b) {
              return a.lastModified() > b.lastModified();
            });

  QSet<QString> seenPrograms;
  QRegularExpression re(QStringLiteral("^(.+)_\\d{8}_\\d{6}\\.etlog$"));

  for (const auto& fi : sorted) {
    QString relPath = dirPath + fi.fileName();
    auto* item = createFileItem(fi.fileName(), relPath);

    bool isLatest = false;
    if (fi.suffix().toLower() == "etlog") {
      QString programName;
      QRegularExpressionMatch m = re.match(fi.fileName());
      if (m.hasMatch()) {
        programName = m.captured(1);
      } else {
        programName = fi.completeBaseName();
      }
      if (!seenPrograms.contains(programName)) {
        seenPrograms.insert(programName);
        isLatest = true;
      }
    }

    item->setData(isLatest, IsLatestRole);
    catItem->appendRow(item);
  }
}

void ProjectStructureWidget::populateBackupCategory(
    QStandardItem* catItem,
    const QFileInfoList& entries,
    const QString& dirPath) {
  QFileInfoList sorted = entries;
  std::sort(sorted.begin(), sorted.end(),
            [](const QFileInfo& a, const QFileInfo& b) {
              return a.lastModified() > b.lastModified();
            });

  for (int i = 0; i < sorted.size(); ++i) {
    const auto& fi = sorted[i];
    QString relPath = dirPath + fi.fileName();
    auto* item = createFileItem(fi.fileName(), relPath);
    item->setData(i == 0, IsLatestRole);
    catItem->appendRow(item);
  }
}

QStandardItem* ProjectStructureWidget::createFileItem(
    const QString& fileName,
    const QString& relativePath) {
  auto* item = new QStandardItem(fileName);
  item->setData(QStringLiteral("file"), NodeTypeRole);
  item->setData(relativePath, RelativePathRole);
  item->setEditable(true);

  QFileInfo fi(fileName);
  QString suffix = fi.suffix().toLower();
  QString iconName;
  if (suffix == QStringLiteral("eproto") ||
      suffix == QStringLiteral("eprotox")) {
    iconName = QStringLiteral("file_eproto");
  } else if (suffix == QStringLiteral("etopo")) {
    iconName = QStringLiteral("file_etopo");
    if (fileName == QStringLiteral("topology.etopo")) {
      item->setData(true, IsEffectiveTopologyRole);
      item->setEditable(false);  // 固定文件名，禁止重命名
    }
  } else if (suffix == QStringLiteral("emock")) {
    iconName = QStringLiteral("file_json");
    if (fileName == QStringLiteral("MockResponses.emock")) {
      item->setData(true, IsMockConfigRole);
    }
  } else if (suffix == QStringLiteral("json")) {
    iconName = QStringLiteral("file_json");
  } else if (suffix == QStringLiteral("lua")) {
    iconName = QStringLiteral("file_lua");
  } else if (suffix == QStringLiteral("xml")) {
    iconName = QStringLiteral("file_xml");
  } else if (suffix == QStringLiteral("yaml") ||
             suffix == QStringLiteral("yml")) {
    iconName = QStringLiteral("file_yaml");
  } else if (suffix == QStringLiteral("cpp") ||
             suffix == QStringLiteral("cxx") ||
             suffix == QStringLiteral("cc")) {
    iconName = QStringLiteral("file_cpp");
  } else if (suffix == QStringLiteral("cmake")) {
    iconName = QStringLiteral("file_cmake");
  } else if (suffix == QStringLiteral("md")) {
    iconName = QStringLiteral("file_markdown");
  } else if (suffix == QStringLiteral("py")) {
    iconName = QStringLiteral("file_python");
  } else if (suffix == QStringLiteral("js")) {
    iconName = QStringLiteral("file_js");
  } else {
    iconName = QStringLiteral("file_generic");
  }
  item->setIcon(AppIconProvider::instance().icon(iconName));

  if (fi.completeBaseName().toLower() == QStringLiteral("icdconfig") &&
      (suffix == QStringLiteral("xml") || suffix == QStringLiteral("json"))) {
    item->setData(true, IsIcdConfigRole);
    item->setEditable(false);
  }

  return item;
}

QString ProjectStructureWidget::absolutePath(
    const QString& relativePath) const {
  return QDir(project_path_).absoluteFilePath(relativePath);
}

QString ProjectStructureWidget::categoryDirPath(
    const QString& categoryId) const {
  for (const auto& cat : defaultCategories()) {
    if (cat.id == categoryId) {
      return cat.dirPath;
    }
  }
  return QString();
}

void ProjectStructureWidget::clearAllEtlogFiles(const QString& categoryId) {
  QString dirPath = categoryDirPath(categoryId);
  if (dirPath.isEmpty())
    return;

  QDir dir(QDir(project_path_).absoluteFilePath(dirPath));
  if (!dir.exists())
    return;

  QStringList etlogFiles = dir.entryList(
      QStringList() << QStringLiteral("*.etlog"), QDir::Files, QDir::Time);

  if (etlogFiles.isEmpty())
    return;

  QString msg =
      QStringLiteral("确定要删除 %1 个 .etlog 文件吗？\n\n此操作不可撤销。")
          .arg(etlogFiles.size());
  auto ret = QMessageBox::question(this, QStringLiteral("确认清除"), msg,
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
  if (ret != QMessageBox::Yes)
    return;

  int deletedCount = 0;
  for (const QString& fileName : etlogFiles) {
    if (dir.remove(fileName)) {
      deletedCount++;
    }
  }

  if (deletedCount > 0) {
    refreshCategory(dir.absolutePath());
    refreshOtherCategory();
    emit fileDeleted(dir.absolutePath());
    LOG_INFO("PROJECT_UI", "已删除 {} 个 .etlog 文件", deletedCount);
  }
}

void ProjectStructureWidget::refreshOtherCategory() {
  if (project_path_.isEmpty() || !root_item_)
    return;

  // 找到 otherItem
  QStandardItem* otherItem = nullptr;
  for (int i = 0; i < root_item_->rowCount(); ++i) {
    auto* child = root_item_->child(i);
    if (child->data(CategoryIdRole).toString() == QLatin1String("other")) {
      otherItem = child;
      break;
    }
  }
  if (!otherItem)
    return;

  otherItem->removeRows(0, otherItem->rowCount());

  // 重新扫描非标准目录的文件
  QDir projectDir(project_path_);
  QStringList skipPrefixes;
  for (const auto& cat : defaultCategories()) {
    QString absDir = QDir(project_path_).absoluteFilePath(cat.dirPath);
    skipPrefixes.append(absDir.toLower() + QStringLiteral("/"));
  }
  skipPrefixes.append(
      projectDir.absoluteFilePath(QStringLiteral("hardware/")).toLower() +
      QStringLiteral("/"));

  int otherCount = 0;
  QDirIterator it(project_path_, QDir::Files | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    QString absPath = it.filePath();
    bool inStandardDir = false;
    for (const auto& prefix : skipPrefixes) {
      if (absPath.toLower().startsWith(prefix)) {
        inStandardDir = true;
        break;
      }
    }
    if (inStandardDir)
      continue;

    QFileInfo fi = it.fileInfo();
    QString relPath = projectDir.relativeFilePath(fi.absoluteFilePath());
    otherItem->appendRow(createFileItem(fi.fileName(), relPath));
    ++otherCount;
  }

  QString otherText = QStringLiteral("其他文件 (") +
                      QString::number(otherCount) + QStringLiteral(")");
  if (otherCount == 0) {
    otherItem->setForeground(QColor(0xbb, 0xbb, 0xbb));
  } else {
    otherItem->setForeground(QBrush());
  }
  otherItem->setText(otherText);

  emit fileListChanged();
}

// ── 硬件节点 ──

void ProjectStructureWidget::refreshHardwareDevices() {
  if (!root_item_ || project_path_.isEmpty())
    return;

  // Find the hardware node (last child — the computed node)
  QStandardItem* hwItem = nullptr;
  for (int i = 0; i < root_item_->rowCount(); ++i) {
    auto* child = root_item_->child(i);
    if (child->data(CategoryIdRole).toString() == QLatin1String("hardware")) {
      hwItem = child;
      break;
    }
  }
  if (!hwItem)
    return;

  hwItem->removeRows(0, hwItem->rowCount());

  // Parse all .etopo files in the topology/ directory
  QDir topoDir(
      QDir(project_path_).absoluteFilePath(QStringLiteral("topology")));
  QSet<QString> seenNames;  // Dedup by device name

  if (topoDir.exists()) {
    for (const QFileInfo& fi :
         topoDir.entryInfoList({QStringLiteral("*.etopo")}, QDir::Files)) {
      QFile file(fi.absoluteFilePath());
      if (!file.open(QIODevice::ReadOnly))
        continue;
      QJsonParseError err;
      QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll(), &err);
      file.close();
      if (err.error != QJsonParseError::NoError)
        continue;

      QJsonArray devices = jdoc.object()[QStringLiteral("devices")].toArray();
      for (const auto& dval : devices) {
        QJsonObject dobj = dval.toObject();
        QString name = dobj[QStringLiteral("name")].toString();
        if (name.isEmpty() || seenNames.contains(name))
          continue;
        seenNames.insert(name);

        QString deviceType = dobj[QStringLiteral("deviceType")].toString();
        QString pluginId = dobj[QStringLiteral("pluginId")].toString();

        auto* deviceItem = new QStandardItem(deviceType);
        deviceItem->setData(QStringLiteral("hardware_device"), NodeTypeRole);
        deviceItem->setData(deviceType, Qt::UserRole + 1);  // deviceType
        deviceItem->setData(pluginId, Qt::UserRole + 2);    // pluginId
        deviceItem->setEditable(false);

        // Match via PluginManager
        auto& pm = etest::core::plugin::PluginManager::instance();
        etest::core::plugin::IPlugin* plugin = pm.plugin(pluginId);
        QString suffix;
        QColor color;
        if (plugin) {
          auto* device =
              pm.pluginAs<etest::core::plugin::IDevicePlugin>(pluginId);
          if (device && device->deviceStatus() ==
                            etest::core::plugin::DeviceStatus::Online) {
            suffix = QStringLiteral("  [在线]");
            color = QColor(0x4C, 0xAF, 0x50);  // 绿色
          } else {
            suffix = QStringLiteral("  [在线]");
            color = QColor(0xFF, 0x98, 0x00);  // 橙色—已匹配但状态异常
          }
        } else if (!pluginId.isEmpty()) {
          suffix = QStringLiteral("  [未加载]");
          color = QColor(0x99, 0x99, 0x99);  // 灰色
        } else {
          suffix = QStringLiteral("  [未加载]");
          color = QColor(0x99, 0x99, 0x99);  // 灰色—旧文件无pluginId
        }

        deviceItem->setText(name + suffix);
        deviceItem->setForeground(color);

        QString tip =
            QStringLiteral("设备: %1\n类型: %2").arg(name, deviceType);
        if (!pluginId.isEmpty())
          tip += QStringLiteral("\n插件: %1").arg(pluginId);
        deviceItem->setToolTip(tip);

        hwItem->appendRow(deviceItem);
      }
    }
  }

  // Update hardware count
  int count = hwItem->rowCount();
  hwItem->setText(QStringLiteral("硬件 (%1)").arg(count));
}

void ProjectStructureWidget::connectHardwareRefresh() {
  // Topology directory changes → refresh hardware
  connect(this, &ProjectStructureWidget::directoryContentChanged, this,
          [this](const QString& dirPath) {
            if (dirPath.endsWith(QStringLiteral("/topology")) ||
                dirPath.endsWith(QStringLiteral("\\topology"))) {
              refreshHardwareDevices();
            }
          });
  // Plugin load/unload → refresh hardware
  auto& pm = etest::core::plugin::PluginManager::instance();
  connect(&pm, &etest::core::plugin::PluginManager::pluginLoaded, this,
          &ProjectStructureWidget::refreshHardwareDevices);
  connect(&pm, &etest::core::plugin::PluginManager::pluginUnloaded, this,
          &ProjectStructureWidget::refreshHardwareDevices);
}

// ── 静态辅助函数 ──

static QString newFileBaseName(const QString& base, const QString& dir) {
  // 自动递增：如果 "新建协议.eproto" 已存在，则尝试 "新建协议 1.eproto"
  QString candidate = base;
  int counter = 0;
  QDir d(dir);
  while (d.exists(candidate)) {
    ++counter;
    candidate = base + QStringLiteral(" ") + QString::number(counter);
  }
  return candidate;
}

// ── 占位页：刷新最近项目列表 ──

void ProjectStructureWidget::refreshRecentProjects() {
  if (!recent_projects_model_)
    return;
  recent_projects_model_->clear();

  auto& cfg = etest::core::config::ConfigManager::instance();
  QStringList recentList =
      cfg.get<QStringList>(etest::core::config::CONFIG_RECENT_PROJECT_LIST);
  QVariantMap timestamps = cfg.get<QVariantMap>(
      etest::core::config::CONFIG_RECENT_PROJECT_TIMESTAMPS);

  for (const QString& path : recentList) {
    QFileInfo fi(path);
    QString displayName = fi.completeBaseName();
    if (displayName.isEmpty())
      continue;

    QString timeStr;
    if (timestamps.contains(path)) {
      QDateTime dt = timestamps[path].toDateTime();
      timeStr = dt.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    }

    auto* item = new QStandardItem(displayName);
    item->setEditable(false);
    item->setIcon(etest::core_ui::AppIconProvider::instance().icon(
        QStringLiteral("folder")));
    item->setData(path, FilePathRole);
    item->setData(fi.absolutePath(), DirPathRole);
    item->setData(timeStr, TimeStrRole);
    QString tooltip = path;
    if (!timeStr.isEmpty())
      tooltip += QStringLiteral("\n") + timeStr;
    item->setToolTip(tooltip);
    recent_projects_model_->appendRow(item);
  }

  // 无最近项目时整节隐藏（D9）
  if (recent_projects_section_) {
    recent_projects_section_->setVisible(!recentList.isEmpty());
  }
}

// ── 已打开文件列表 ──

void ProjectStructureWidget::setOpenFiles(const QStringList& paths) {
  clearOpenFiles();
  for (const QString& path : paths) {
    addOpenFileItem(path);
  }
  updateOpenFilesCount();
}

void ProjectStructureWidget::onFileOpened(const QString& path) {
  removeOpenFileItem(path);
  addOpenFileItem(path);
  updateOpenFilesCount();
}

void ProjectStructureWidget::onFileClosed(const QString& path) {
  removeOpenFileItem(path);
  updateOpenFilesCount();
}

void ProjectStructureWidget::addOpenFileItem(const QString& path) {
  QFileInfo fi(path);
  QString displayName = fi.fileName();
  QString dirPath =
      project_path_.isEmpty()
          ? fi.absolutePath()
          : QDir(project_path_).relativeFilePath(fi.absolutePath());

  auto* item = new QStandardItem(displayName);
  item->setEditable(false);
  item->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("file_generic")));
  item->setData(path, FilePathRole);
  item->setData(dirPath, DirPathRole);
  item->setToolTip(path);
  open_files_model_->appendRow(item);
  updateOpenFilesCount();
}

void ProjectStructureWidget::removeOpenFileItem(const QString& path) {
  for (int i = 0; i < open_files_model_->rowCount(); ++i) {
    if (open_files_model_->item(i)->data(FilePathRole).toString() == path) {
      open_files_model_->removeRow(i);
      break;
    }
  }
  updateOpenFilesCount();
}

void ProjectStructureWidget::clearOpenFiles() {
  open_files_model_->clear();
  updateOpenFilesCount();
}

void ProjectStructureWidget::updateOpenFilesCount() {
  int count = open_files_model_->rowCount();
  open_files_header_label_->setText(QStringLiteral("已打开 (%1)").arg(count));
}

// ── 最近文件 ──

void ProjectStructureWidget::refreshRecentFiles() {
  if (!recent_files_model_)
    return;
  recent_files_model_->clear();

  auto& cfg = etest::core::config::ConfigManager::instance();
  QStringList files = cfg.get<QStringList>(
      QString::fromLatin1(etest::core::config::CONFIG_RECENT_FILE_LIST));

  for (const QString& path : files) {
    QFileInfo fi(path);
    if (!fi.exists())
      continue;

    QString dirPath = fi.absolutePath();
    auto* item = new QStandardItem(fi.fileName());
    item->setEditable(false);
    item->setIcon(etest::core_ui::AppIconProvider::instance().icon(
        QStringLiteral("file_generic")));
    item->setData(path, FilePathRole);
    item->setData(dirPath, DirPathRole);
    item->setToolTip(path);
    recent_files_model_->appendRow(item);
  }

  // 无最近文件时整节隐藏（D9）
  if (recent_files_section_) {
    recent_files_section_->setVisible(!files.isEmpty());
  }
}

void ProjectStructureWidget::removeRecentFileFromConfig(const QString& path) {
  auto& cfg = etest::core::config::ConfigManager::instance();
  QStringList files = cfg.get<QStringList>(
      QString::fromLatin1(etest::core::config::CONFIG_RECENT_FILE_LIST));
  if (files.removeAll(path) > 0) {
    cfg.set(QString::fromLatin1(etest::core::config::CONFIG_RECENT_FILE_LIST),
            files);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// QAB 搜索支持
// ══════════════════════════════════════════════════════════════════════════════

QStringList ProjectStructureWidget::allFileNames() const {
  QStringList result;
  if (!root_item_ || project_path_.isEmpty()) {
    return result;
  }
  std::function<void(QStandardItem*)> collect;
  collect = [&](QStandardItem* item) {
    if (!item) {
      return;
    }
    if (item->data(NodeTypeRole).toString() == QStringLiteral("file")) {
      result << item->text();
    }
    for (int i = 0; i < item->rowCount(); ++i) {
      collect(item->child(i));
    }
  };
  collect(root_item_);
  result.removeDuplicates();
  return result;
}

bool ProjectStructureWidget::locateFile(const QString& fileName) {
  if (!root_item_ || project_path_.isEmpty()) {
    return false;
  }
  QModelIndex startIndex = model_->index(0, 0);
  QModelIndexList matches =
      model_->match(startIndex, Qt::DisplayRole, fileName, -1,
                    Qt::MatchExactly | Qt::MatchRecursive);
  for (const auto& idx : matches) {
    if (idx.data(NodeTypeRole).toString() == QStringLiteral("file")) {
      tree_view_->expand(idx.parent());
      tree_view_->setCurrentIndex(idx);
      tree_view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
      return true;
    }
  }
  return false;
}

bool ProjectStructureWidget::locateFileByPath(const QString& relativePath) {
  if (!root_item_ || project_path_.isEmpty()) {
    LOG_DEBUG("PROJECT_UI", "locateFileByPath: 项目树未就绪 root={} proj={}",
              root_item_ ? 1 : 0, project_path_.toStdString());
    return false;
  }
  QModelIndex start_index = model_->index(0, 0);
  QModelIndexList matches =
      model_->match(start_index, RelativePathRole, relativePath, -1,
                    Qt::MatchExactly | Qt::MatchRecursive);
  LOG_DEBUG("PROJECT_UI", "locateFileByPath: 查找 rel={} 命中数={}",
            relativePath.toStdString(), matches.size());
  for (const auto& idx : matches) {
    if (idx.data(NodeTypeRole).toString() == QStringLiteral("file")) {
      tree_view_->expand(idx.parent());
      tree_view_->setCurrentIndex(idx);
      tree_view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
      return true;
    }
  }
  return false;
}

void ProjectStructureWidget::clearTreeSelection() {
  tree_view_->clearSelection();
}

bool ProjectStructureWidget::isSyncDocEnabled() const {
  return tree_delegate_->isSyncDocEnabled();
}

}  // namespace etest::app
