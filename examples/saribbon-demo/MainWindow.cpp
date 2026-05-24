#include "MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonPanel.h"
#include "SARibbonQuickAccessBar.h"

//=============================================================================
// ActivityBar
//=============================================================================

ActivityBar::ActivityBar(QWidget* parent) : QWidget(parent) {
  setFixedWidth(48);
  setObjectName("sidebarActivityBar");

  auto* lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  // clang-format off
    const QStringList tips{
        QStringLiteral("资源管理器"),   // 资源管理器
        QStringLiteral("搜索"),                       // 搜索
        QStringLiteral("源代码管理"),     // 源代码管理
        QStringLiteral("调试"),                       // 调试
        QStringLiteral("扩展"),                       // 扩展
        QStringLiteral("硬件"),                       // 硬件
        QStringLiteral("协议"),                       // 协议
        QStringLiteral("用例"),                       // 用例
    };
  // clang-format on

  // Standard pixmaps for each activity button
  const QStyle::StandardPixmap spix[]{
      QStyle::SP_ComputerIcon,            // 资源管理器
      QStyle::SP_FileDialogContentsView,  // 搜索
      QStyle::SP_DriveNetIcon,            // 源代码管理
      QStyle::SP_MessageBoxQuestion,      // 调试
      QStyle::SP_FileDialogListView,      // 扩展
      QStyle::SP_DriveHDIcon,             // 硬件
      QStyle::SP_FileDialogInfoView,      // 协议
      QStyle::SP_FileDialogDetailedView,  // 用例
  };

  for (int i = 0; i < 8; ++i) {
    auto* btn = new QToolButton(this);
    btn->setToolTip(tips[i]);
    btn->setCheckable(true);
    btn->setFixedSize(48, 40);
    btn->setAutoRaise(true);
    btn->setIconSize(QSize(24, 24));
    btn->setIcon(style()->standardIcon(spix[i]));

    lay->addWidget(btn);
    buttons_.append(btn);

    connect(btn, &QToolButton::clicked, this, [this, i]() {
      setActiveIndex(i);
      emit pageClicked(i);
    });
  }

  lay->addStretch();

  // Settings button at bottom
  auto* settings_btn = new QToolButton(this);
  settings_btn->setToolTip(QStringLiteral("设置"));  // 设置
  settings_btn->setFixedSize(48, 40);
  settings_btn->setAutoRaise(true);
  settings_btn->setIconSize(QSize(24, 24));
  settings_btn->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  lay->addWidget(settings_btn);

  connect(settings_btn, &QToolButton::clicked, this,
          &ActivityBar::settingsTriggered);

  if (!buttons_.isEmpty())
    buttons_[0]->setChecked(true);
}

void ActivityBar::setActiveIndex(int index) {
  if (index < 0 || index >= buttons_.size())
    return;
  active_index_ = index;
  updateButtonStates();
}

void ActivityBar::updateButtonStates() {
  for (int i = 0; i < buttons_.size(); ++i)
    buttons_[i]->setChecked(i == active_index_);
}

//=============================================================================
// MainWindow
//=============================================================================

MainWindow::MainWindow(QWidget* parent) : SARibbonMainWindow(parent) {
  setupRibbon();
  setupCentralLayout();
  setupStatusBar();

  resize(1400, 900);

  setWindowIcon(QIcon(":/app_icon.ico"));
}

//-----------------------------------------------------------------------------
// Ribbon
//-----------------------------------------------------------------------------

void MainWindow::setupRibbon() {
  auto* ribbon = ribbonBar();

  // ---- QuickAccessBar ----
  auto* qab = ribbon->quickAccessBar();
  act_new_project_ = new QAction(style()->standardIcon(QStyle::SP_FileIcon),
                                 QStringLiteral("新建项目"), this);  // 新建项目
  act_open_project_ =
      new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
                  QStringLiteral("打开项目"), this);  // 打开项目
  act_save_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                          QStringLiteral("保存"), this);  // 保存
  act_undo_ = new QAction(style()->standardIcon(QStyle::SP_ArrowBack),
                          QStringLiteral("撤销"), this);  // 撤销
  act_redo_ = new QAction(style()->standardIcon(QStyle::SP_ArrowForward),
                          QStringLiteral("重做"), this);  // 重做

  qab->addAction(act_new_project_);
  qab->addAction(act_open_project_);
  qab->addAction(act_save_);
  qab->addSeparator();
  qab->addAction(act_undo_);
  qab->addAction(act_redo_);

  // ---- Application Button ----
  ribbon->applicationButton()->setIcon(QIcon(":/app_icon.ico"));
  ribbon->applicationButton()->setText(QStringLiteral("文件"));  // 文件

  // ============================================================
  //  主页
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("主页"));  // 主页

    // 文件 Panel
    auto* panel_file = cat->addPanel(QStringLiteral("文件"));  // 文件
    panel_file->addLargeAction(act_new_project_);
    panel_file->addLargeAction(act_open_project_);
    panel_file->addLargeAction(act_save_);
    act_save_as_ = new QAction(QStringLiteral("另存为..."), this);  // 另存为...
    panel_file->addLargeAction(act_save_as_);

    // 编辑 Panel
    auto* panel_edit = cat->addPanel(QStringLiteral("编辑"));  // 编辑
    panel_edit->addLargeAction(act_undo_);
    panel_edit->addLargeAction(act_redo_);
    panel_edit->addSeparator();
    act_cut_ = new QAction(style()->standardIcon(QStyle::SP_FileLinkIcon),
                           QStringLiteral("切剪"), this);  // 剪切
    act_copy_ = new QAction(style()->standardIcon(QStyle::SP_FileIcon),
                            QStringLiteral("复制"), this);   // 复制
    act_paste_ = new QAction(QStringLiteral("粘贴"), this);  // 粘贴
    panel_edit->addSmallAction(act_cut_);
    panel_edit->addSmallAction(act_copy_);
    panel_edit->addSmallAction(act_paste_);
  }

  // ============================================================
  //  视图
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("视图"));  // 视图

    auto* panel_panels = cat->addPanel(QStringLiteral("面板"));  // 面板
    act_welcome_ = new QAction(QStringLiteral("欢迎页"), this);  // 欢迎页
    act_toggle_output_ =
        new QAction(QStringLiteral("输出面板"), this);  // 输出面板
    act_toggle_output_->setCheckable(true);
    act_toggle_output_->setChecked(true);
    act_toggle_aux_ =
        new QAction(QStringLiteral("辅助侧边栏"), this);  // 辅助侧边栏
    act_toggle_aux_->setCheckable(true);

    panel_panels->addLargeAction(act_welcome_);
    panel_panels->addLargeAction(act_toggle_output_);
    panel_panels->addLargeAction(act_toggle_aux_);

    connect(act_toggle_output_, &QAction::triggered, this,
            &MainWindow::toggleBottomPanel);
    connect(act_toggle_aux_, &QAction::triggered, this,
            &MainWindow::toggleAuxSidebar);
  }

  // ============================================================
  //  帮助
  // ============================================================
  {
    auto* cat = ribbon->addCategoryPage(QStringLiteral("帮助"));  // 帮助
    auto* panel_about = cat->addPanel(QStringLiteral("关于"));    // 关于
    act_about_ = new QAction(QStringLiteral("关于 SARibbon Demo"), this);
    panel_about->addLargeAction(act_about_);

    connect(act_about_, &QAction::triggered, this, [this]() {
      QMessageBox::about(this, QStringLiteral("关于"),
                         QStringLiteral("SARibbon Demo — ETest UI 布局展示"
                                        "\n\n仅用于界面效果预览"));
    });
  }

  // Ribbon style & collapse button
  ribbon->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);
  ribbon->showMinimumModeButton(true);
  ribbon->setTabDoubleClickToMinimumMode(true);
}

//-----------------------------------------------------------------------------
// Central layout
//-----------------------------------------------------------------------------

void MainWindow::setupCentralLayout() {
  auto* container = new QWidget(this);
  auto* container_lay = new QHBoxLayout(container);
  container_lay->setContentsMargins(0, 0, 0, 0);
  container_lay->setSpacing(0);

  // ---- Activity Bar (left) ----
  activity_bar_ = new ActivityBar(container);
  container_lay->addWidget(activity_bar_);

  // ---- Horizontal splitter ----
  h_splitter_ = new QSplitter(Qt::Horizontal, container);

  // ============================================================
  //  Sidebar (h_splitter [0])
  // ============================================================
  {
    auto* sidebar = new QWidget(h_splitter_);
    auto* sb_lay = new QVBoxLayout(sidebar);
    sb_lay->setContentsMargins(0, 0, 0, 0);
    sb_lay->setSpacing(0);

    // Title bar
    auto* title_bar = new QWidget(sidebar);
    title_bar->setObjectName(QStringLiteral("sidebarTitleBar"));
    title_bar->setFixedHeight(35);
    auto* title_lay = new QHBoxLayout(title_bar);
    title_lay->setContentsMargins(10, 0, 0, 0);
    sidebar_title_ = new QLabel(QStringLiteral("资源管理器"), title_bar);
    title_lay->addWidget(sidebar_title_);
    sb_lay->addWidget(title_bar);

    // Stacked pages
    // clang-format off
        const QStringList pageNames{
            QStringLiteral("资源管理器"),   // 资源管理器
            QStringLiteral("搜索"),                       // 搜索
            QStringLiteral("源代码管理"),     // 源代码管理
            QStringLiteral("调试"),                       // 调试
            QStringLiteral("扩展"),                       // 扩展
            QStringLiteral("硬件"),                       // 硬件
            QStringLiteral("协议"),                       // 协议
            QStringLiteral("用例"),                       // 用例
        };
    // clang-format on

    sidebar_stack_ = new QStackedWidget(sidebar);
    for (const auto& name : pageNames) {
      auto* page = new QLabel(name, sidebar_stack_);
      page->setAlignment(Qt::AlignCenter);
      page->setStyleSheet(QStringLiteral("color: #666; font-size: 18px;"));
      sidebar_stack_->addWidget(page);
    }
    sb_lay->addWidget(sidebar_stack_);

    sidebar_content_panel_ = sidebar;
    h_splitter_->addWidget(sidebar);
    h_splitter_->setStretchFactor(0, 0);
  }

  // ============================================================
  //  Vertical splitter (h_splitter [1]) : editor | bottom
  // ============================================================
  v_splitter_ = new QSplitter(Qt::Vertical, h_splitter_);

  // Central editor placeholder
  central_editor_ = new QTextEdit(v_splitter_);
  central_editor_->setReadOnly(true);
  central_editor_->setHtml(QStringLiteral(
      "<h2 style='text-align:center; color:#555; margin-top:100px;'>"
      "欢迎使用 SARibbon Demo</h2>"  // 欢迎使用...
      "<p style='text-align:center; color:#888;'>"
      "这是一个基于 SARibbon 的 "   // 这是一个基于...
      "ETest UI 布展示示例</p>"));  // ETest UI 布局展示示例
  v_splitter_->addWidget(central_editor_);

  // Bottom container
  {
    bottom_container_ = new QWidget(v_splitter_);
    auto* bot_lay = new QVBoxLayout(bottom_container_);
    bot_lay->setContentsMargins(0, 0, 0, 0);
    bot_lay->setSpacing(0);

    bottom_tab_ = new QTabWidget(bottom_container_);
    bottom_tab_->setDocumentMode(true);
    bottom_tab_->setMovable(true);

    auto* out = new QTextEdit(bottom_tab_);
    out->setReadOnly(true);
    out->setPlainText(QStringLiteral(
        "[输出面板] 这里显示日志输出信息"));  // [输出面板] 这里显示日志输出信息
    bottom_tab_->addTab(out, QStringLiteral("输出"));  // 输出

    auto* prob = new QTextEdit(bottom_tab_);
    prob->setReadOnly(true);
    prob->setPlainText(QStringLiteral(
        "[问题面板] 这里显示错误和警告"));  // [问题面板] 这里显示错误和警告
    bottom_tab_->addTab(prob, QStringLiteral("问题"));  // 问题

    auto* term = new QTextEdit(bottom_tab_);
    term->setReadOnly(true);
    term->setPlainText(QStringLiteral(
        "[终端面板] 这里是终端模拟区域"));  // [终端面板] 这里是终端模拟区域
    bottom_tab_->addTab(term, QStringLiteral("终端"));  // 终端

    bot_lay->addWidget(bottom_tab_);
    v_splitter_->addWidget(bottom_container_);
  }

  v_splitter_->setStretchFactor(0, 1);
  v_splitter_->setStretchFactor(1, 0);
  v_splitter_->setSizes({600, 200});

  h_splitter_->addWidget(v_splitter_);
  h_splitter_->setStretchFactor(1, 1);

  // ---- Aux sidebar (hidden) ----
  aux_sidebar_widget_ = new QWidget(h_splitter_);
  auto* aux_lay = new QVBoxLayout(aux_sidebar_widget_);
  auto* aux_lbl = new QLabel(QStringLiteral("辅助侧边栏"),
                             aux_sidebar_widget_);  // 辅助侧边栏
  aux_lbl->setAlignment(Qt::AlignCenter);
  aux_lbl->setStyleSheet(QStringLiteral("color: #666;"));
  aux_lay->addWidget(aux_lbl);
  aux_sidebar_widget_->hide();
  h_splitter_->addWidget(aux_sidebar_widget_);
  h_splitter_->setStretchFactor(2, 0);

  h_splitter_->setSizes({sidebar_width_, 800, 0});

  container_lay->addWidget(h_splitter_);
  setCentralWidget(container);

  // ---- Signals ----
  connect(activity_bar_, &ActivityBar::pageClicked, this,
          &MainWindow::onActivityBarClicked);
}

//-----------------------------------------------------------------------------
// Status bar
//-----------------------------------------------------------------------------

void MainWindow::setupStatusBar() {
  auto* sb = statusBar();

  // clang-format off
    statusBar()->addWidget(new QLabel(QStringLiteral("就绪"), this));          // 就绪
    statusBar()->addWidget(new QLabel(QStringLiteral("无打开项目"), this));  // 无打开项目
    statusBar()->addWidget(new QLabel(QStringLiteral("0 错误, 0 警告"), this));  // 0 错误, 0 警告
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("纯文本"), this));     // 纯文本
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("CRLF"), this));
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("UTF-8"), this));
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("行 1, 列 1"), this));  // 行 1, 列 1
  // clang-format on

  Q_UNUSED(sb);
}

//-----------------------------------------------------------------------------
// Slots
//-----------------------------------------------------------------------------

void MainWindow::onActivityBarClicked(int index) {
  bool same = (index == active_activity_index_);
  bool visible = sidebar_content_panel_->isVisible();

  if (same && visible) {
    // Toggle off
    sidebar_content_panel_->hide();
  } else {
    sidebar_stack_->setCurrentIndex(index);
    auto* lbl = qobject_cast<QLabel*>(sidebar_stack_->currentWidget());
    if (lbl)
      sidebar_title_->setText(lbl->text());
    if (!visible)
      sidebar_content_panel_->show();
    active_activity_index_ = index;
  }
}

void MainWindow::toggleBottomPanel() {
  bool visible = bottom_container_->isVisible();
  if (visible) {
    QList<int> sz = v_splitter_->sizes();
    if (sz.size() > 1)
      bottom_height_ = sz[1];
    bottom_container_->hide();
  } else {
    bottom_container_->show();
    v_splitter_->setSizes({600, bottom_height_});
  }
  act_toggle_output_->setChecked(!visible);
}

void MainWindow::toggleAuxSidebar() {
  bool visible = !aux_sidebar_widget_->isVisible();
  aux_sidebar_widget_->setVisible(visible);
  act_toggle_aux_->setChecked(visible);
}
