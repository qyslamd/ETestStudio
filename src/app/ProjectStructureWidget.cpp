#include "ProjectStructureWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>


#include "AppIconProvider.h"
#include "ConfigManager.h"
#include "config/ConfigDefs.h"
#include "widgets/RecentProjectCard.h"

namespace etest::app {

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
  scroll_content->setObjectName(QStringLiteral("PhScrollContent"));
  auto* sc_layout = new QVBoxLayout(scroll_content);
  sc_layout->setContentsMargins(12, 8, 12, 8);
  sc_layout->setSpacing(16);

  // ── 卡片 1：快速操作 ──
  auto* card1 = new QFrame(scroll_content);
  card1->setObjectName(QStringLiteral("PhCard"));
  auto* c1_layout = new QVBoxLayout(card1);
  c1_layout->setContentsMargins(20, 24, 20, 24);
  c1_layout->setSpacing(12);

  auto* ph_icon = new QLabel(card1);
  ph_icon->setPixmap(AppIconProvider::instance()
                         .icon(QStringLiteral("project"))
                         .pixmap(48, 48));
  ph_icon->setAlignment(Qt::AlignCenter);
  c1_layout->addWidget(ph_icon);

  auto* ph_title = new QLabel(QStringLiteral("没有打开的项目"), card1);
  ph_title->setObjectName(QStringLiteral("PhTitle"));
  ph_title->setAlignment(Qt::AlignCenter);
  c1_layout->addWidget(ph_title);

  auto* ph_desc =
      new QLabel(QStringLiteral("创建或打开一个项目来管理测试资产\n"
                                "您也可以直接使用编辑器创建和编辑文件"),
                 card1);
  ph_desc->setObjectName(QStringLiteral("PhDesc"));
  ph_desc->setAlignment(Qt::AlignCenter);
  ph_desc->setWordWrap(true);
  c1_layout->addWidget(ph_desc);

  // 快捷操作 — 2×2 grid，复用 defaultCategories
  auto* btn_grid = new QGridLayout();
  btn_grid->setSpacing(8);

  QList<CategoryInfo> quickCats;
  for (const auto& cat : defaultCategories()) {
    if (!cat.newFileExt.isEmpty()) {
      quickCats.append(cat);
      if (quickCats.size() >= 4)
        break;
    }
  }
  static const int kRows[] = {0, 0, 1, 1};
  static const int kCols[] = {0, 1, 0, 1};
  for (int i = 0; i < quickCats.size(); ++i) {
    const auto& cat = quickCats[i];
    auto* btn = new QPushButton(cat.displayName, card1);
    btn->setObjectName(QStringLiteral("PhQuickBtn"));
    btn->setFixedHeight(28);
    btn->setProperty("catId", cat.id);
    btn->setProperty("ext", cat.newFileExt);
    btn->setProperty("baseName", cat.newFileLabel);
    btn_grid->addWidget(btn, kRows[i], kCols[i]);
  }

  auto* btn_row = new QHBoxLayout();
  btn_row->addStretch();
  btn_row->addLayout(btn_grid);
  btn_row->addStretch();
  c1_layout->addLayout(btn_row);

  sc_layout->addWidget(card1);

  // ── 卡片 2：项目管理 ──
  auto* card2 = new QFrame(scroll_content);
  card2->setObjectName(QStringLiteral("PhCard"));
  auto* c2_layout = new QVBoxLayout(card2);
  c2_layout->setContentsMargins(20, 20, 20, 20);
  c2_layout->setSpacing(12);

  auto* proj_section_label =
      new QLabel(QStringLiteral("项目管理"), card2);
  proj_section_label->setObjectName(QStringLiteral("PhSectionLabel"));
  c2_layout->addWidget(proj_section_label);

  new_proj_btn_ = new QPushButton(QStringLiteral("  新建项目"), card2);
  new_proj_btn_->setObjectName(QStringLiteral("PhProjectBtn"));
  new_proj_btn_->setFixedHeight(32);
  new_proj_btn_->setCursor(Qt::PointingHandCursor);

  open_proj_btn_ =
      new QPushButton(QStringLiteral("  打开项目"), card2);
  open_proj_btn_->setObjectName(QStringLiteral("PhProjectBtn"));
  open_proj_btn_->setFixedHeight(32);
  open_proj_btn_->setCursor(Qt::PointingHandCursor);

  auto* proj_btn_layout = new QHBoxLayout();
  proj_btn_layout->setSpacing(8);
  proj_btn_layout->addStretch();
  proj_btn_layout->addWidget(new_proj_btn_);
  proj_btn_layout->addWidget(open_proj_btn_);
  proj_btn_layout->addStretch();
  c2_layout->addLayout(proj_btn_layout);

  sc_layout->addWidget(card2);

  // ── 卡片 3：最近浏览 ──
  auto* card3 = new QFrame(scroll_content);
  card3->setObjectName(QStringLiteral("PhCard"));
  auto* c3_layout = new QVBoxLayout(card3);
  c3_layout->setContentsMargins(20, 20, 20, 20);
  c3_layout->setSpacing(12);

  auto* recent_section_label =
      new QLabel(QStringLiteral("最近项目"), card3);
  recent_section_label->setObjectName(QStringLiteral("PhSectionLabel"));
  c3_layout->addWidget(recent_section_label);

  recent_container_ = new QWidget(card3);
  recent_container_->setObjectName(QStringLiteral("PhRecentContainer"));
  c3_layout->addWidget(recent_container_);

  // ── 卡片内部隔线 ──
  auto* sep3 = new QFrame(card3);
  sep3->setObjectName(QStringLiteral("PhSeparator"));
  sep3->setFrameShape(QFrame::HLine);
  c3_layout->addWidget(sep3);

  auto* recent_file_label =
      new QLabel(QStringLiteral("最近文件"), card3);
  recent_file_label->setObjectName(QStringLiteral("PhSectionLabel"));
  c3_layout->addWidget(recent_file_label);

  recent_files_view_ = new QListView();
  recent_files_view_->setFrameShape(QFrame::NoFrame);
  recent_files_view_->setMouseTracking(true);
  recent_files_model_ = new QStandardItemModel(this);
  recent_files_view_->setModel(recent_files_model_);
  auto* rf_delegate = new OpenFileDelegate(this);
  rf_delegate->setCloseButtonVisible(false);
  recent_files_view_->setItemDelegate(rf_delegate);
  recent_files_view_->setFixedHeight(200);
  c3_layout->addWidget(recent_files_view_);

  sc_layout->addWidget(card3);

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

  open_files_header_btn_ = new QToolButton(open_files_widget_);
  open_files_header_btn_->setObjectName(QStringLiteral("PhOpenFilesHeaderBtn"));
  open_files_header_btn_->setCheckable(false);
  open_files_header_btn_->setToolButtonStyle(Qt::ToolButtonTextOnly);
  open_files_header_btn_->setText(QStringLiteral("已打开 (0)"));
  of_layout->addWidget(open_files_header_btn_);

  open_files_view_ = new QListView();
  open_files_view_->setFrameShape(QFrame::NoFrame);
  open_files_view_->setMouseTracking(true);
  open_files_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  open_files_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  open_files_model_ = new QStandardItemModel(this);
  open_files_view_->setModel(open_files_model_);
  open_file_delegate_ = new OpenFileDelegate(this);
  open_files_view_->setItemDelegate(open_file_delegate_);
  of_layout->addWidget(open_files_view_);

  tree_view_ = new QTreeView();
  tree_view_->setHeaderHidden(true);
  tree_view_->setAnimated(true);
  tree_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->setDragDropMode(QAbstractItemView::NoDragDrop);
  tree_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  tree_view_->setIndentation(16);
  tree_view_->setIconSize(QSize(16, 16));

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
  // 快捷按钮
  auto quickBtns = page_default_->findChildren<QPushButton*>(
      QStringLiteral("PhQuickBtn"));
  for (auto* btn : quickBtns) {
    QString catId = btn->property("catId").toString();
    QString ext = btn->property("ext").toString();
    QString baseName = btn->property("baseName").toString();
    connect(btn, &QPushButton::clicked, this, [this, catId, ext, baseName]() {
      if (project_path_.isEmpty()) {
        createStandaloneFile(ext, baseName);
      } else {
        createNewFile(catId, ext, baseName);
      }
    });
  }

  // 项目管理按钮
  connect(new_proj_btn_, &QPushButton::clicked, this,
          &ProjectStructureWidget::newProjectRequested);
  connect(open_proj_btn_, &QPushButton::clicked, this,
          &ProjectStructureWidget::openProjectRequested);

  // 文件监视器
  connect(file_watcher_, &QFileSystemWatcher::directoryChanged, this,
          &ProjectStructureWidget::onDirectoryChanged);

  // 防抖定时器
  connect(debounce_timer_, &QTimer::timeout, this, [this]() {
    for (const auto& path : debounce_timer_queued_paths_) {
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

  // ── 已打开文件列表 ──
  connect(open_files_view_, &QListView::clicked, this,
          [this](const QModelIndex& index) {
            QString path = index.data(FilePathRole).toString();
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
  connect(open_file_delegate_, &OpenFileDelegate::closeRequested, this,
          &ProjectStructureWidget::openFileCloseRequested);

  // ── 最近文件列表 ──
  if (recent_files_view_) {
    connect(recent_files_view_, &QListView::clicked, this,
            [this](const QModelIndex& index) {
              QString path = index.data(FilePathRole).toString();
              if (!path.isEmpty())
                emit recentFileOpenRequested(path);
            });
    connect(recent_files_view_, &QListView::customContextMenuRequested, this,
            [this](const QPoint& pos) {
              QModelIndex index = recent_files_view_->indexAt(pos);
              if (!index.isValid()) return;
              QString path = index.data(FilePathRole).toString();
              if (path.isEmpty()) return;

              QMenu menu(recent_files_view_);
              menu.setObjectName(QStringLiteral("PhRecentContextMenu"));
              auto* removeAction = menu.addAction(QStringLiteral("从最近文件中移除"));
              if (menu.exec(recent_files_view_->viewport()->mapToGlobal(pos)) ==
                  removeAction) {
                removeRecentFileFromConfig(path);
                refreshRecentFiles();
              }
            });
  }

  // 最近项目/文件变更时刷新
  connect(&etest::core::config::ConfigManager::instance(),
          &etest::core::config::ConfigManager::configChanged, this,
          [this](const QString& key) {
            if (key == QLatin1String(
                           etest::core::config::CONFIG_RECENT_PROJECT_LIST)) {
              refreshRecentProjects();
            } else if (key == QLatin1String(
                               etest::core::config::CONFIG_RECENT_FILE_LIST)) {
              refreshRecentFiles();
            }
          });
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
       QStringLiteral("eproto"), QStringLiteral("新建协议文件")},
      {QStringLiteral("topology"), QStringLiteral("拓扑"),
       QStringLiteral("topology/"), QStringLiteral("topo_tap"),
       QStringLiteral("etopo"), QStringLiteral("新建拓扑文件")},
      {QStringLiteral("hardware"), QStringLiteral("硬件"),
       QStringLiteral("hardware/"), QStringLiteral("hardware"), QString(),
       QString()},
      {QStringLiteral("testprog"), QStringLiteral("用例"),
       QStringLiteral("cases/"), QStringLiteral("testprogram"),
       QStringLiteral("tcase"), QStringLiteral("新建测试用例")},
      {QStringLiteral("script"), QStringLiteral("脚本"),
       QStringLiteral("scripts/"), QStringLiteral("file_lua"),
       QStringLiteral("lua"), QStringLiteral("新建脚本")},
      {QStringLiteral("report"), QStringLiteral("报告"),
       QStringLiteral("reports/"), QStringLiteral("file_generic"), QString(),
       QString()},
      {QStringLiteral("config"), QStringLiteral("配置"),
       QStringLiteral("config/"), QStringLiteral("file_json"),
       QStringLiteral("json"), QStringLiteral("新建配置文件")},
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

      for (const auto& fi : entries) {
        QString relPath = cat.dirPath + fi.fileName();
        catItem->appendRow(createFileItem(fi.fileName(), relPath));
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

  // 展开根节点
  tree_view_->expand(root_item_->index());

  // 开始监视
  if (!watchedDirs.isEmpty()) {
    file_watcher_->addPaths(watchedDirs);
  }
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
  for (const auto& fi : entries) {
    QString relPath =
        QDir(project_path_).relativeFilePath(fi.absoluteFilePath());
    catItem->appendRow(createFileItem(fi.fileName(), relPath));
    ++fileCount;
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
}

void ProjectStructureWidget::onDirectoryChanged(const QString& path) {
  debounce_timer_queued_paths_.insert(path);
  debounce_timer_->start();
}

// ── 槽函数 ──

void ProjectStructureWidget::onCustomContextMenu(const QPoint& pos) {
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
      // 不支持新建的分类（硬件、报告、备份、"其他文件"）
      auto* openInFmAction = menu.addAction(QStringLiteral("在文件系统中打开"));
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

    QAction* chosen = menu.exec(tree_view_->viewport()->mapToGlobal(pos));

    if (chosen == openAction) {
      QString relPath = item->data(RelativePathRole).toString();
      emit fileOpenRequested(absolutePath(relPath));
    } else if (chosen == renameAction) {
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
  QStandardItem* item = model_->itemFromIndex(index);
  if (!item)
    return;
  QString nodeType = item->data(NodeTypeRole).toString();
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

  QString newRelPath = item->data(RelativePathRole).toString();
  QDir projectDir(project_path_);
  QString newPath = projectDir.absoluteFilePath(newRelPath);

  // 检查是否真的发生了重命名（不是编辑器失焦等误触）
  QFileInfo fi(newPath);
  QString newFileName = fi.fileName();

  // 更新相对路径
  QString parentRelPath;
  QStandardItem* parent = item->parent();
  if (parent && parent != model_->invisibleRootItem()) {
    QString catId = parent->data(CategoryIdRole).toString();
    for (const auto& cat : defaultCategories()) {
      if (cat.id == catId) {
        parentRelPath = cat.dirPath;
        break;
      }
    }
  }
  if (!parentRelPath.isEmpty()) {
    item->setData(parentRelPath + newFileName, RelativePathRole);
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

  QString fileName =
      newFileBaseName(baseName, fullDir) +
      (extension.isEmpty() ? QString() : QStringLiteral(".") + extension);
  QString fullPath = QDir(fullDir).absoluteFilePath(fileName);

  QFile file(fullPath);
  if (!file.open(QIODevice::WriteOnly))
    return;
  file.close();

  emit fileCreated(fullPath);

  // 领域文件自动打开
  if (!extension.isEmpty() && extension != QStringLiteral("json") &&
      extension != QStringLiteral("lua")) {
    emit fileOpenRequested(fullPath);
  } else {
    // 非领域文件进入重命名模式
    QModelIndexList matches =
        model_->match(model_->index(0, 0), RelativePathRole,
                      QVariant::fromValue(targetDir + fileName), -1,
                      Qt::MatchExactly | Qt::MatchRecursive);
    if (!matches.isEmpty()) {
      tree_view_->edit(matches.first());
    }
  }
}

void ProjectStructureWidget::createStandaloneFile(const QString& extension,
                                                  const QString& baseName) {
  auto& cfg = etest::core::config::ConfigManager::instance();
  QString defaultDir = cfg.get<QString>(
      etest::core::config::CONFIG_DEFAULT_FILE_SAVE_PATH,
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  QString defaultName = baseName + QStringLiteral(".") + extension;
  QString filePath = QFileDialog::getSaveFileName(
      this, baseName, defaultDir,
      QStringLiteral("%1 (*.%2)").arg(baseName).arg(extension));
  if (filePath.isEmpty())
    return;

  QFileInfo fi(filePath);
  cfg.set(etest::core::config::CONFIG_DEFAULT_FILE_SAVE_PATH,
          fi.absolutePath());

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly))
    return;
  file.close();

  emit fileCreated(filePath);
  emit fileOpenRequested(filePath);
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

QStandardItem* ProjectStructureWidget::createFileItem(
    const QString& fileName,
    const QString& relativePath) {
  auto* item = new QStandardItem(fileName);
  item->setData(QStringLiteral("file"), NodeTypeRole);
  item->setData(relativePath, RelativePathRole);
  item->setEditable(false);

  QFileInfo fi(fileName);
  QString suffix = fi.suffix().toLower();
  QString iconName;
  if (suffix == QStringLiteral("eproto")) {
    iconName = QStringLiteral("file_eproto");
  } else if (suffix == QStringLiteral("etopo")) {
    iconName = QStringLiteral("file_etopo");
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
  if (!recent_container_)
    return;

  // 清空旧条目
  QLayout* recent_layout = recent_container_->layout();
  if (recent_layout) {
    QLayoutItem* child;
    while ((child = recent_layout->takeAt(0)) != nullptr) {
      if (child->widget()) {
        child->widget()->deleteLater();
      }
      delete child;
    }
  } else {
    recent_layout = new QVBoxLayout(recent_container_);
    recent_layout->setContentsMargins(0, 0, 0, 0);
    recent_layout->setSpacing(6);
  }

  auto& cfg = etest::core::config::ConfigManager::instance();
  QStringList recentList =
      cfg.get<QStringList>(etest::core::config::CONFIG_RECENT_PROJECT_LIST);
  QVariantMap timestamps = cfg.get<QVariantMap>(
      etest::core::config::CONFIG_RECENT_PROJECT_TIMESTAMPS);

  if (recentList.isEmpty()) {
    auto* empty_label =
        new QLabel(QStringLiteral("暂无最近项目"), recent_container_);
    empty_label->setObjectName(QStringLiteral("PhRecentEmpty"));
    empty_label->setAlignment(Qt::AlignCenter);
    recent_layout->addWidget(empty_label);
    return;
  }

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

    auto* card = new RecentProjectCard(path, displayName, fi.absolutePath(),
                                       timeStr, recent_container_);
    card->setObjectName(QStringLiteral("PhRecentCard"));
    card->setAttribute(Qt::WA_StyledBackground);
    connect(card, &RecentProjectCard::openRequested, this,
            &ProjectStructureWidget::projectOpenRequested);
    connect(card, &RecentProjectCard::removeRequested, this,
            [this](const QString& p) {
              auto& cfg = etest::core::config::ConfigManager::instance();
              QStringList recentList = cfg.get<QStringList>(
                  etest::core::config::CONFIG_RECENT_PROJECT_LIST);
              recentList.removeAll(p);
              cfg.set(etest::core::config::CONFIG_RECENT_PROJECT_LIST,
                      recentList);
              refreshRecentProjects();
            });

    recent_layout->addWidget(card);
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
  open_files_header_btn_->setText(QStringLiteral("已打开 (%1)").arg(count));
}

// ── 最近文件 ──

void ProjectStructureWidget::refreshRecentFiles() {
  if (!recent_files_model_) return;
  recent_files_model_->clear();

  auto& cfg = etest::core::config::ConfigManager::instance();
  QStringList files = cfg.get<QStringList>(
      QString::fromLatin1(etest::core::config::CONFIG_RECENT_FILE_LIST));

  for (const QString& path : files) {
    QFileInfo fi(path);
    if (!fi.exists()) continue;

    QString dirPath = fi.absolutePath();
    auto* item = new QStandardItem(fi.fileName());
    item->setEditable(false);
    item->setData(path, FilePathRole);
    item->setData(dirPath, DirPathRole);
    item->setToolTip(path);
    recent_files_model_->appendRow(item);
  }

  // Auto-hide the section when there are no recent files
  if (recent_files_view_) {
    recent_files_view_->setVisible(!files.isEmpty());
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

}  // namespace etest::app
