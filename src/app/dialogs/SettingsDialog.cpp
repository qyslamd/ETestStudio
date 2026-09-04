#include "dialogs/SettingsDialog.h"

#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "ThemeManager.h"
#include "backup/BackupManager.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "core_ui/AppIconProvider.h"
#include "utils/switch_button.h"
#include "version.h"


namespace etest::app {

using namespace core::config;

SettingsDialog::SettingsDialog(QWidget* parent) : OverlayDialog(parent) {
  round_radius_ = 16;  // 遮罩圆角
  initUi();
  initSignals();
}

void SettingsDialog::initUi() {
  setWindowTitle(QStringLiteral("设置"));

  // OverlayDialog 内容卡片（固定尺寸，居中显示于遮罩）
  auto* content = new QWidget;
  content->setObjectName(QStringLiteral("SettingsContent"));
  content->setFixedSize(800, 600);
  auto* mainLayout = new QVBoxLayout(content);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // in-dialog 标题栏（无原生标题，替代之）
  auto* titleBar = new QHBoxLayout();
  titleBar->setContentsMargins(20, 14, 12, 10);
  titleBar->setSpacing(12);
  title_icon_ = new QLabel(content);
  title_icon_->setObjectName(QStringLiteral("SettingsAppIcon"));
  title_icon_->setFixedSize(34, 34);
  title_icon_->setAlignment(Qt::AlignCenter);
  title_icon_->setPixmap(etest::core_ui::AppIconProvider::instance()
                             .icon(QStringLiteral("welcome"))
                             .pixmap(18, 18));
  auto* titleGroup = new QVBoxLayout();
  titleGroup->setSpacing(0);
  auto* titleLabel = new QLabel(QStringLiteral("设置"), content);
  titleLabel->setObjectName(QStringLiteral("SettingsTitleText"));
  auto* subtitleLabel =
      new QLabel(QStringLiteral("ETest Studio · 自动化测试系统"), content);
  subtitleLabel->setObjectName(QStringLiteral("SettingsTitleSub"));
  titleGroup->addWidget(titleLabel);
  titleGroup->addWidget(subtitleLabel);
  titleBar->addWidget(title_icon_);
  titleBar->addLayout(titleGroup);
  titleBar->addStretch();
  auto* closeBtn = new QToolButton(content);
  closeBtn->setObjectName(QStringLiteral("SettingsCloseBtn"));
  closeBtn->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("close")));
  closeBtn->setIconSize(QSize(16, 16));
  closeBtn->setCursor(Qt::PointingHandCursor);
  connect(closeBtn, &QToolButton::clicked, this, [this](bool) { reject(); });
  titleBar->addWidget(closeBtn);
  mainLayout->addLayout(titleBar);

  // === Content area: navigation + pages ===
  auto* contentLayout = new QHBoxLayout();
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  // === Left: category list ===
  list_ = new QListWidget(content);
  list_->setFixedWidth(180);
  list_->setIconSize(QSize(16, 16));
  list_->setFocusPolicy(Qt::NoFocus);
  list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto makeLabelItem = [this](const QString& text) {
    auto* item = new QListWidgetItem(text, list_);
    item->setFlags(Qt::NoItemFlags);
    item->setData(Qt::UserRole, -1);
    item->setForeground(
        etest::core_ui::ThemeManager::instance().secondaryTextColor());
    QFont f = list_->font();
    f.setBold(true);
    f.setPointSizeF(qMax<qreal>(9.0, f.pointSizeF() - 1.5));
    item->setFont(f);
    return item;
  };
  makeLabelItem(QStringLiteral("设置"));

  QListWidgetItem* itemGeneral =
      new QListWidgetItem(QStringLiteral("通用"), list_);
  itemGeneral->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("settings")));
  QListWidgetItem* itemEditor =
      new QListWidgetItem(QStringLiteral("编辑器"), list_);
  itemEditor->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("file_cpp")));
  QListWidgetItem* itemTerminal =
      new QListWidgetItem(QStringLiteral("终端"), list_);
  itemTerminal->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("tab_terminal")));
  QListWidgetItem* itemAppearance =
      new QListWidgetItem(QStringLiteral("外观"), list_);
  itemAppearance->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("palette")));
  QListWidgetItem* itemProject =
      new QListWidgetItem(QStringLiteral("项目"), list_);
  itemProject->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("folder")));
  QListWidgetItem* itemBackup =
      new QListWidgetItem(QStringLiteral("备份"), list_);
  itemBackup->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("file_save")));

  itemGeneral->setData(Qt::UserRole, 0);
  itemGeneral->setData(Qt::UserRole + 1, QStringLiteral("settings"));
  itemEditor->setData(Qt::UserRole, 1);
  itemEditor->setData(Qt::UserRole + 1, QStringLiteral("file_cpp"));
  itemTerminal->setData(Qt::UserRole, 2);
  itemTerminal->setData(Qt::UserRole + 1, QStringLiteral("tab_terminal"));
  itemAppearance->setData(Qt::UserRole, 3);
  itemAppearance->setData(Qt::UserRole + 1, QStringLiteral("palette"));
  itemProject->setData(Qt::UserRole, 4);
  itemProject->setData(Qt::UserRole + 1, QStringLiteral("folder"));
  itemBackup->setData(Qt::UserRole, 5);
  itemBackup->setData(Qt::UserRole + 1, QStringLiteral("file_save"));

  makeLabelItem(QStringLiteral("关于"));
  QListWidgetItem* itemAbout =
      new QListWidgetItem(QStringLiteral("关于"), list_);
  itemAbout->setIcon(etest::core_ui::AppIconProvider::instance().icon(
      QStringLiteral("ribbon_about")));
  itemAbout->setData(Qt::UserRole, 6);
  itemAbout->setData(Qt::UserRole + 1, QStringLiteral("ribbon_about"));

  contentLayout->addWidget(list_);

  // === Right: all categories stacked in one scroll area ===
  scroll_area_ = new QScrollArea(content);
  scroll_area_->setWidgetResizable(true);
  scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll_area_->setFrameShape(QFrame::NoFrame);

  auto* scrollContent = new QWidget();
  scroll_area_->setWidget(scrollContent);
  auto* scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 0, 0);
  scrollLayout->setSpacing(0);

  page_widgets_ = {createGeneralPage(),  createEditorPage(),
                   createTerminalPage(), createAppearancePage(),
                   createProjectPage(),  createBackupPage(),
                   createAboutPage()};
  for (QWidget* page : page_widgets_) {
    scrollLayout->addWidget(page);
  }

  contentLayout->addWidget(scroll_area_, 1);
  mainLayout->addLayout(contentLayout, 1);

  // === Bottom: button bar ===
  auto* buttonBar = new QWidget(content);
  buttonBar->setObjectName("SettingsButtonBar");
  auto* buttonLayout = new QHBoxLayout(buttonBar);
  buttonLayout->setContentsMargins(12, 8, 12, 8);
  buttonLayout->addStretch();

  btn_close_ = new QPushButton(QStringLiteral("关闭"), buttonBar);
  btn_close_->setFixedWidth(80);

  buttonLayout->addWidget(btn_close_);

  mainLayout->addWidget(buttonBar);

  setWidget(content);

  // 默认选中「通用」（跳过开头的分组标签行），触发滚动定位到首分类
  for (int i = 0; i < list_->count(); ++i) {
    if (list_->item(i)->data(Qt::UserRole).toInt() == 0) {
      list_->setCurrentItem(list_->item(i));
      break;
    }
  }
}

void SettingsDialog::initSignals() {
  connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
    QListWidgetItem* item = (row >= 0) ? list_->item(row) : nullptr;
    if (!item) {
      return;
    }
    const int page = item->data(Qt::UserRole).toInt();
    if (page >= 0 && page < page_widgets_.size()) {
      // 左导航仅定位：把该分类顶部滚到可视区起点
      const int y =
          page_widgets_[page]->mapTo(scroll_area_->widget(), QPoint(0, 0)).y();
      scroll_area_->verticalScrollBar()->setValue(y);
    }
  });

  connect(&ConfigManager::instance(), &ConfigManager::configChanged, this,
          &SettingsDialog::onConfigChanged);

  connect(btn_close_, &QPushButton::clicked, this, &QDialog::reject);

  // 主题切换后刷新导航图标与 Toggle on
  // 色（对话框复用，构造时烧入的值需跟随主题）
  connect(
      &etest::core_ui::ThemeManager::instance(),
      &etest::core_ui::ThemeManager::themeChanged, this, [this](bool) {
        for (int i = 0; i < list_->count(); ++i) {
          QListWidgetItem* item = list_->item(i);
          const QString iconName = item->data(Qt::UserRole + 1).toString();
          if (!iconName.isEmpty()) {
            item->setIcon(
                etest::core_ui::AppIconProvider::instance().icon(iconName));
          }
          // 分组标签前景随主题刷新
          if (item->data(Qt::UserRole).toInt() < 0) {
            item->setForeground(
                etest::core_ui::ThemeManager::instance().secondaryTextColor());
          }
        }
        if (title_icon_) {
          title_icon_->setPixmap(etest::core_ui::AppIconProvider::instance()
                                     .icon(QStringLiteral("welcome"))
                                     .pixmap(18, 18));
        }
        const QColor accent =
            etest::core_ui::ThemeManager::instance().accentColor();
        for (QAbstractButton* btn : check_map_) {
          if (auto* sw = qobject_cast<SwitchButton*>(btn)) {
            sw->setOnBackground(accent);
          }
        }
      });
}

// =========================================================================
// Page creation
// =========================================================================

QWidget* SettingsDialog::createGeneralPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  // --- 日志 card ---
  addPageHeader(layout, QStringLiteral("通用"),
                QStringLiteral("日志与默认保存目录"));
  auto* cardLog = createSettingsCard(page, QStringLiteral("日志"));

  // 日志级别（int 值存 userData，仿主题下拉写法）
  {
    auto* rightLayout =
        addSettingRow(cardLog, QStringLiteral("日志级别"),
                      QStringLiteral("记录到日志文件的最低级别"));
    auto* combo = new QComboBox();
    combo->addItem(QStringLiteral("调试"), 0);
    combo->addItem(QStringLiteral("信息"), 1);
    combo->addItem(QStringLiteral("警告"), 2);
    combo->addItem(QStringLiteral("错误"), 3);
    combo->addItem(QStringLiteral("致命"), 4);
    combo->setFixedWidth(160);

    int val = ConfigManager::instance().get<int>(CONFIG_LOG_LEVEL,
                                                 CONFIG_LOG_DEFAULT_LEVEL);
    int idx = combo->findData(val);
    if (idx >= 0)
      combo->setCurrentIndex(idx);

    rightLayout->addWidget(combo);

    combo_map_.insert(QString::fromLatin1(CONFIG_LOG_LEVEL), combo);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [combo]() {
              ConfigManager::instance().set<int>(CONFIG_LOG_LEVEL,
                                                 combo->currentData().toInt());
            });
  }

  // 单文件大小上限（配置存字节，UI 以 MB 编辑，需手动转换）
  {
    auto* rightLayout =
        addSettingRow(cardLog, QStringLiteral("单文件大小上限"),
                      QStringLiteral("单个日志文件的最大体积（MB）"));
    auto* spin = new QSpinBox();
    spin->setRange(1, 512);
    spin->setSingleStep(1);
    spin->setFixedWidth(100);

    int mb = ConfigManager::instance().get<int>(
                 CONFIG_LOG_MAX_FILE_SIZE, CONFIG_LOG_DEFAULT_MAX_FILE_SIZE) /
             1024 / 1024;
    spin->setValue(mb);

    rightLayout->addWidget(spin);

    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int val) {
              ConfigManager::instance().set<int>(CONFIG_LOG_MAX_FILE_SIZE,
                                                 val * 1024 * 1024);
            });
  }

  addSpinBoxRow(cardLog, QStringLiteral("保留文件份数"),
                QStringLiteral("循环写满后保留的日志文件个数上限"),
                CONFIG_LOG_MAX_FILE_COUNT, 1, 100, 1,
                CONFIG_LOG_DEFAULT_MAX_FILE_COUNT);
  addSpinBoxRow(cardLog, QStringLiteral("保留天数"),
                QStringLiteral("超过该天数的日志文件将被清理"),
                CONFIG_LOG_KEEP_DAYS, 1, 365, 1, CONFIG_LOG_DEFAULT_KEEP_DAYS);

  // --- 默认保存目录 card ---
  auto* cardPath = createSettingsCard(page, QStringLiteral("默认保存目录"));

  {
    auto* rightLayout =
        addSettingRow(cardPath, QStringLiteral("保存目录"),
                      QStringLiteral("保存文件时默认定位到的目录"));
    auto* pathEdit = new QLineEdit();
    pathEdit->setReadOnly(true);
    pathEdit->setPlaceholderText(QStringLiteral("未设置"));
    pathEdit->setFixedWidth(260);
    pathEdit->setText(
        ConfigManager::instance().get<QString>(CONFIG_DEFAULT_FILE_SAVE_PATH));

    auto* browseBtn = new QPushButton(QStringLiteral("选择目录..."));
    auto* clearBtn = new QPushButton(QStringLiteral("清除"));

    rightLayout->addWidget(pathEdit);
    rightLayout->addWidget(browseBtn);
    rightLayout->addWidget(clearBtn);

    connect(browseBtn, &QPushButton::clicked, this, [pathEdit]() {
      QString dir = QFileDialog::getExistingDirectory(
          nullptr, QStringLiteral("选择默认保存目录"));
      if (!dir.isEmpty()) {
        pathEdit->setText(dir);
        ConfigManager::instance().set(CONFIG_DEFAULT_FILE_SAVE_PATH, dir);
      }
    });
    connect(clearBtn, &QPushButton::clicked, this, [pathEdit]() {
      pathEdit->clear();
      ConfigManager::instance().set(CONFIG_DEFAULT_FILE_SAVE_PATH, QString());
    });
  }

  return page;
}

QWidget* SettingsDialog::createEditorPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  addPageHeader(layout, QStringLiteral("编辑器"),
                QStringLiteral("代码编辑与排版偏好"));
  auto* card = createSettingsCard(page, QStringLiteral("编辑器"));
  addSpinBoxRow(card, QStringLiteral("字体大小"),
                QStringLiteral("编辑器文字大小（像素）"),
                CONFIG_EDITOR_FONT_SIZE, 8, 72, 1,
                CONFIG_EDITOR_DEFAULT_FONT_SIZE);
  addCheckBoxRow(
      card, QStringLiteral("显示行号"), QStringLiteral("在编辑器左侧显示行号"),
      CONFIG_EDITOR_SHOW_LINE_NUMBER, CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER);
  addCheckBoxRow(card, QStringLiteral("自动缩进"),
                 QStringLiteral("换行时自动对齐缩进"),
                 CONFIG_EDITOR_AUTO_INDENT, CONFIG_EDITOR_DEFAULT_AUTO_INDENT);
  addSpinBoxRow(card, QStringLiteral("Tab 宽度"),
                QStringLiteral("制表符占据的空格数"), CONFIG_EDITOR_TAB_WIDTH,
                2, 8, 1, CONFIG_EDITOR_DEFAULT_TAB_WIDTH);
  addCheckBoxRow(card, QStringLiteral("空格替代 Tab"),
                 QStringLiteral("按 Tab 键插入空格而非制表符"),
                 CONFIG_EDITOR_SPACES_FOR_TAB,
                 CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB);

  return page;
}

QWidget* SettingsDialog::createTerminalPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  addPageHeader(layout, QStringLiteral("终端"),
                QStringLiteral("命令行解释器与显示"));
  auto* card = createSettingsCard(page, QStringLiteral("终端"));
  addComboBoxRow(card, QStringLiteral("Shell"),
                 QStringLiteral("终端使用的命令行解释器"),
                 CONFIG_TERMINAL_SHELL,
                 {QStringLiteral("cmd.exe"), QStringLiteral("powershell.exe"),
                  QStringLiteral("bash.exe")},
                 QString::fromLatin1(CONFIG_TERMINAL_DEFAULT_SHELL));
  addSpinBoxRow(
      card, QStringLiteral("字体大小"), QStringLiteral("终端文字大小（像素）"),
      CONFIG_TERMINAL_FONT_SIZE, 8, 24, 1, CONFIG_TERMINAL_DEFAULT_FONT_SIZE);
  addSpinBoxRow(card, QStringLiteral("滚动缓冲行数"),
                QStringLiteral("终端可回滚查看的历史行数"),
                CONFIG_TERMINAL_SCROLLBACK, 100, 100000, 1000,
                CONFIG_TERMINAL_DEFAULT_SCROLLBACK);

  return page;
}

QWidget* SettingsDialog::createAppearancePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  // --- 外观 card ---
  addPageHeader(layout, QStringLiteral("外观"),
                QStringLiteral("主题、欢迎页背景与屏保"));
  auto* cardAppearance = createSettingsCard(page, QStringLiteral("外观"));

  // 主题选择（使用 userData 存储配置值）
  {
    auto* rightLayout = addSettingRow(cardAppearance, QStringLiteral("主题"),
                                      QStringLiteral("应用程序界面色彩主题"));
    auto* combo = new QComboBox();
    for (const auto& themeId :
         etest::core_ui::ThemeManager::instance().availableThemes()) {
      combo->addItem(
          etest::core_ui::ThemeManager::instance().themeDisplayName(themeId),
          themeId);
    }
    combo->setFixedWidth(160);

    QString val = ConfigManager::instance().get<QString>(
        CONFIG_APPEARANCE_THEME,
        QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));
    int idx = combo->findData(val);
    if (idx >= 0)
      combo->setCurrentIndex(idx);

    rightLayout->addWidget(combo);

    combo_map_.insert(QString::fromLatin1(CONFIG_APPEARANCE_THEME), combo);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [combo]() {
              ConfigManager::instance().set<QString>(
                  CONFIG_APPEARANCE_THEME, combo->currentData().toString());
            });
  }

  addCheckBoxRow(cardAppearance, QStringLiteral("工具栏可见"),
                 QStringLiteral("显示或隐藏顶部工具栏"), CONFIG_TOOLBAR_VISIBLE,
                 CONFIG_TOOLBAR_DEFAULT_VISIBLE);
  addButtonRow(cardAppearance, QStringLiteral("窗口布局"),
               QStringLiteral("将停靠面板布局恢复到默认状态"),
               QStringLiteral("恢复默认"),
               []() { ConfigManager::instance().resetAllToDefault(); });

  // --- 欢迎页背景 card ---
  auto* cardBg = createSettingsCard(page, QStringLiteral("欢迎页背景"));

  {
    auto* hint = new QLabel(
        QStringLiteral("设置了图片目录则优先从目录中随机选图"), cardBg);
    hint->setObjectName("SettingsHint");
    cardBg->layout()->addWidget(hint);
  }

  // 背景图片目录
  {
    auto* rightLayout =
        addSettingRow(cardBg, QStringLiteral("图片目录"),
                      QStringLiteral("从指定目录中随机选择背景图片"));
    auto* dirEdit = new QLineEdit();
    dirEdit->setReadOnly(true);
    dirEdit->setPlaceholderText(QStringLiteral("未设置（随机选图）"));
    dirEdit->setFixedWidth(260);
    QString curDir =
        ConfigManager::instance().get<QString>(CONFIG_WELCOME_BG_DIR);
    dirEdit->setText(curDir);

    auto* browseDirBtn = new QPushButton(QStringLiteral("选择目录..."));
    auto* clearDirBtn = new QPushButton(QStringLiteral("清除"));

    rightLayout->addWidget(dirEdit);
    rightLayout->addWidget(browseDirBtn);
    rightLayout->addWidget(clearDirBtn);

    connect(browseDirBtn, &QPushButton::clicked, this, [dirEdit]() {
      QString dir = QFileDialog::getExistingDirectory(
          nullptr, QStringLiteral("选择背景图片目录"));
      if (!dir.isEmpty()) {
        dirEdit->setText(dir);
        ConfigManager::instance().set(CONFIG_WELCOME_BG_DIR, dir);
      }
    });
    connect(clearDirBtn, &QPushButton::clicked, this, [dirEdit]() {
      dirEdit->clear();
      ConfigManager::instance().set(CONFIG_WELCOME_BG_DIR, QString());
    });
  }

  // 固定背景图片
  {
    auto* rightLayout =
        addSettingRow(cardBg, QStringLiteral("固定图片"),
                      QStringLiteral("使用单张固定图片作为背景"));
    auto* pathEdit = new QLineEdit();
    pathEdit->setReadOnly(true);
    pathEdit->setPlaceholderText(QStringLiteral("未设置"));
    pathEdit->setFixedWidth(260);
    QString curPath =
        ConfigManager::instance().get<QString>(CONFIG_WELCOME_BG_IMAGE);
    pathEdit->setText(curPath);

    auto* browseBtn = new QPushButton(QStringLiteral("浏览..."));
    auto* clearBtn = new QPushButton(QStringLiteral("清除"));

    rightLayout->addWidget(pathEdit);
    rightLayout->addWidget(browseBtn);
    rightLayout->addWidget(clearBtn);

    connect(browseBtn, &QPushButton::clicked, this, [pathEdit]() {
      QString file = QFileDialog::getOpenFileName(
          nullptr, QStringLiteral("选择背景图片"), {},
          QStringLiteral(
              "图片文件 (*.png *.jpg *.jpeg *.jfif *.bmp *.gif *.svg)"));
      if (!file.isEmpty()) {
        pathEdit->setText(file);
        ConfigManager::instance().set(CONFIG_WELCOME_BG_IMAGE, file);
      }
    });
    connect(clearBtn, &QPushButton::clicked, this, [pathEdit]() {
      pathEdit->clear();
      ConfigManager::instance().set(CONFIG_WELCOME_BG_IMAGE, QString());
    });
  }

  // 填充方式
  {
    auto* rightLayout =
        addSettingRow(cardBg, QStringLiteral("填充方式"),
                      QStringLiteral("背景图片的缩放和排列方式"));
    auto* combo = new QComboBox();
    combo->addItem(QStringLiteral("居中"), 0);
    combo->addItem(QStringLiteral("平铺"), 1);
    combo->addItem(QStringLiteral("拉伸"), 2);
    combo->setFixedWidth(160);

    int curMode = ConfigManager::instance().get<int>(CONFIG_WELCOME_BG_MODE, 0);
    combo->setCurrentIndex(combo->findData(curMode));

    rightLayout->addWidget(combo);

    combo_map_.insert(QString::fromLatin1(CONFIG_WELCOME_BG_MODE), combo);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [combo]() {
              ConfigManager::instance().set<int>(CONFIG_WELCOME_BG_MODE,
                                                 combo->currentData().toInt());
            });
  }

  // --- 屏保 card ---
  auto* cardSaver = createSettingsCard(page, QStringLiteral("屏保"));

  addCheckBoxRow(cardSaver, QStringLiteral("空闲时显示屏保"),
                 QStringLiteral("一段时间无操作后自动显示屏保动画"),
                 CONFIG_TUXSAVER_ENABLED, CONFIG_TUXSAVER_DEFAULT_ENABLED);
  addSpinBoxRow(cardSaver, QStringLiteral("触发时间(秒)"),
                QStringLiteral("无操作后启动屏保的等待时间"),
                CONFIG_TUXSAVER_IDLE_TIMEOUT, 1, 60, 1,
                CONFIG_TUXSAVER_DEFAULT_TIMEOUT);

  // 屏保模式（使用 userData 存储配置值）
  {
    auto* rightLayout = addSettingRow(cardSaver, QStringLiteral("屏保模式"),
                                      QStringLiteral("屏保动画的风格"));
    auto* combo = new QComboBox();
    combo->addItem(QStringLiteral("小企鹅"), QStringLiteral("tux"));
    combo->addItem(QStringLiteral("哲思·片刻"), QStringLiteral("wisdom"));
    combo->setFixedWidth(160);

    QString val = ConfigManager::instance().get<QString>(
        CONFIG_TUXSAVER_MODE,
        QString::fromLatin1(CONFIG_TUXSAVER_DEFAULT_MODE));
    int idx = combo->findData(val);
    if (idx >= 0)
      combo->setCurrentIndex(idx);

    rightLayout->addWidget(combo);

    combo_map_.insert(QString::fromLatin1(CONFIG_TUXSAVER_MODE), combo);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [combo]() {
              ConfigManager::instance().set<QString>(
                  CONFIG_TUXSAVER_MODE, combo->currentData().toString());
            });
  }

  return page;
}

QWidget* SettingsDialog::createProjectPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  addPageHeader(layout, QStringLiteral("项目"),
                QStringLiteral("项目打开与 Git 行为"));
  auto* card = createSettingsCard(page, QStringLiteral("项目"));
  addCheckBoxRow(card, QStringLiteral("自动打开所属项目"),
                 QStringLiteral("打开文件时自动打开其所属项目"),
                 CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT,
                 CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT_DEFAULT);
  addCheckBoxRow(card, QStringLiteral("提示初始化 Git 仓库"),
                 QStringLiteral("打开未初始化 Git 的项目时弹窗询问是否初始化"),
                 CONFIG_PROJECT_GIT_PROMPT_INIT,
                 CONFIG_PROJECT_DEFAULT_GIT_PROMPT_INIT);

  return page;
}

QWidget* SettingsDialog::createBackupPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  addPageHeader(layout, QStringLiteral("备份"),
                QStringLiteral("自动备份与手动操作"));
  auto* cardAuto = createSettingsCard(page, QStringLiteral("自动备份"));
  addCheckBoxRow(cardAuto, QStringLiteral("启用自动备份"),
                 QStringLiteral("按设定间隔自动备份项目文件"),
                 CONFIG_BACKUP_ENABLED, CONFIG_BACKUP_DEFAULT_ENABLED);
  addSpinBoxRow(cardAuto, QStringLiteral("备份间隔(分钟)"),
                QStringLiteral("两次自动备份之间的时间间隔"),
                CONFIG_BACKUP_INTERVAL_MIN, 1, 60, 1,
                CONFIG_BACKUP_DEFAULT_INTERVAL_MIN);
  addSpinBoxRow(cardAuto, QStringLiteral("最大备份数"),
                QStringLiteral("保留的备份份数上限，超出后自动删除最旧的"),
                CONFIG_BACKUP_MAX_COUNT, 1, 50, 1,
                CONFIG_BACKUP_DEFAULT_MAX_COUNT);

  auto* cardManual = createSettingsCard(page, QStringLiteral("手动操作"));
  auto* manualBtn = addButtonRow(cardManual, QStringLiteral("立即备份"),
                                 QStringLiteral("立即创建一次项目备份"),
                                 QStringLiteral("备份"), nullptr);
  connect(manualBtn, &QPushButton::clicked, this, []() {
    etest::core::backup::BackupManager::instance().manualBackup();
  });

  return page;
}

void SettingsDialog::addPageHeader(QVBoxLayout* layout,
                                   const QString& title,
                                   const QString& subtitle) {
  auto* titleLabel = new QLabel(title, layout->parentWidget());
  titleLabel->setObjectName(QStringLiteral("SettingsPageTitle"));
  auto* subLabel = new QLabel(subtitle, layout->parentWidget());
  subLabel->setObjectName(QStringLiteral("SettingsPageSub"));
  layout->addWidget(titleLabel);
  layout->addWidget(subLabel);
  layout->addSpacing(10);
}

QWidget* SettingsDialog::createAboutPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);
  addPageHeader(layout, QStringLiteral("关于"), QStringLiteral("版本信息"));

  auto* card = new QWidget(page);
  card->setObjectName(QStringLiteral("SettingsAboutCard"));
  auto* cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(24, 28, 24, 28);
  cardLayout->setSpacing(6);
  cardLayout->setAlignment(Qt::AlignCenter);

  auto* icon = new QLabel(card);
  icon->setObjectName(QStringLiteral("SettingsAboutIcon"));
  icon->setFixedSize(56, 56);
  icon->setAlignment(Qt::AlignCenter);
  icon->setPixmap(etest::core_ui::AppIconProvider::instance()
                      .icon(QStringLiteral("welcome"))
                      .pixmap(26, 26));
  cardLayout->addWidget(icon, 0, Qt::AlignCenter);

  auto* nameLabel = new QLabel(QStringLiteral("ETest Studio"), card);
  nameLabel->setObjectName(QStringLiteral("SettingsAboutName"));
  nameLabel->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(nameLabel);

  auto* versionLabel = new QLabel(
      QStringLiteral("版本 %1 · 自动化测试系统").arg(PROJECT_VERSION), card);
  versionLabel->setObjectName(QStringLiteral("SettingsAboutVersion"));
  versionLabel->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(versionLabel);

  auto* copyright = new QLabel(QStringLiteral("Copyright © 2026 ETest"), card);
  copyright->setObjectName(QStringLiteral("SettingsAboutCopyright"));
  copyright->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(copyright);

  layout->addWidget(card);
  return page;
}

// =========================================================================
// Settings card container
// =========================================================================

QWidget* SettingsDialog::createSettingsCard(QWidget* parent,
                                            const QString& title) {
  auto* card = new QWidget(parent);
  card->setObjectName("SettingsCard");
  auto* cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(12, 8, 12, 8);
  cardLayout->setSpacing(0);

  auto* titleLabel = new QLabel(title, card);
  titleLabel->setObjectName("SettingsCardTitle");
  cardLayout->addWidget(titleLabel);

  parent->layout()->addWidget(card);
  return card;
}

// =========================================================================
// VS Code style setting row
// =========================================================================

QHBoxLayout* SettingsDialog::addSettingRow(QWidget* parent,
                                           const QString& title,
                                           const QString& description) {
  auto* row = new QWidget(parent);
  row->setObjectName("SettingsRow");
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 8, 0, 8);

  auto* leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(2);
  leftLayout->setContentsMargins(0, 0, 0, 0);

  auto* titleLabel = new QLabel(title, row);
  titleLabel->setObjectName("SettingsRowTitle");
  leftLayout->addWidget(titleLabel);

  if (!description.isEmpty()) {
    auto* descLabel = new QLabel(description, row);
    descLabel->setObjectName("SettingsRowDesc");
    descLabel->setWordWrap(true);
    leftLayout->addWidget(descLabel);
  }

  rowLayout->addLayout(leftLayout, 1);

  auto* rightLayout = new QHBoxLayout();
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->addLayout(rightLayout, 0);

  parent->layout()->addWidget(row);
  return rightLayout;
}

// =========================================================================
// Form row creators — each knows its ConfigKey and default value
// =========================================================================

QSpinBox* SettingsDialog::addSpinBoxRow(QWidget* parent,
                                        const QString& title,
                                        const QString& description,
                                        const QString& configKey,
                                        int min,
                                        int max,
                                        int step,
                                        int defaultVal) {
  auto* rightLayout = addSettingRow(parent, title, description);

  auto* spin = new QSpinBox();
  spin->setRange(min, max);
  spin->setSingleStep(step);
  spin->setFixedWidth(100);

  int val = ConfigManager::instance().get<int>(configKey, defaultVal);
  spin->setValue(val);

  rightLayout->addWidget(spin);

  spin_map_.insert(configKey, spin);
  spinBoxToConfig(configKey, spin);

  return spin;
}

QAbstractButton* SettingsDialog::addCheckBoxRow(QWidget* parent,
                                                const QString& title,
                                                const QString& description,
                                                const QString& configKey,
                                                bool defaultVal) {
  auto* rightLayout = addSettingRow(parent, title, description);

  // Fluent Toggle（SwitchButton 自绘滑块），on=主题 accent、off=中性灰
  auto* sw = new SwitchButton();
  sw->setOnBackground(etest::core_ui::ThemeManager::instance().accentColor());
  sw->setOffBackground(QColor(0xD0, 0xD0, 0xDD));

  bool val = ConfigManager::instance().get<bool>(configKey, defaultVal);
  sw->setChecked(val);

  rightLayout->addWidget(sw);

  check_map_.insert(configKey, sw);
  checkBoxToConfig(configKey, sw);

  return sw;
}

QComboBox* SettingsDialog::addComboBoxRow(QWidget* parent,
                                          const QString& title,
                                          const QString& description,
                                          const QString& configKey,
                                          const QStringList& items,
                                          const QString& defaultVal) {
  auto* rightLayout = addSettingRow(parent, title, description);

  auto* combo = new QComboBox();
  combo->addItems(items);
  combo->setFixedWidth(160);

  QString val = ConfigManager::instance().get<QString>(configKey, defaultVal);
  int idx = combo->findText(val);
  if (idx >= 0)
    combo->setCurrentIndex(idx);

  rightLayout->addWidget(combo);

  combo_map_.insert(configKey, combo);
  comboBoxToConfig(configKey, combo);

  return combo;
}

QPushButton* SettingsDialog::addButtonRow(QWidget* parent,
                                          const QString& title,
                                          const QString& description,
                                          const QString& text,
                                          std::function<void()> callback) {
  auto* rightLayout = addSettingRow(parent, title, description);

  auto* btn = new QPushButton(text);
  btn->setFixedWidth(100);

  rightLayout->addWidget(btn);

  if (callback) {
    connect(btn, &QPushButton::clicked, this, callback);
  }

  return btn;
}

// =========================================================================
// Bidirectional binding: control → config and config → control
// =========================================================================

void SettingsDialog::spinBoxToConfig(const QString& key, QSpinBox* spin) {
  connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [key](int val) { ConfigManager::instance().set<int>(key, val); });
}

void SettingsDialog::checkBoxToConfig(const QString& key, QAbstractButton* cb) {
  connect(cb, &QAbstractButton::toggled, this, [key](bool checked) {
    ConfigManager::instance().set<bool>(key, checked);
  });
}

void SettingsDialog::comboBoxToConfig(const QString& key, QComboBox* combo) {
  connect(combo, &QComboBox::currentTextChanged, this,
          [key](const QString& text) {
            ConfigManager::instance().set<QString>(key, text);
          });
}

void SettingsDialog::onConfigChanged(const QString& key) {
  if (key == QString::fromLatin1(CONFIG_APPEARANCE_THEME) ||
      key == QString::fromLatin1(CONFIG_WELCOME_BG_MODE) ||
      key == QString::fromLatin1(CONFIG_TUXSAVER_MODE) ||
      key == QString::fromLatin1(CONFIG_LOG_LEVEL)) {
    if (combo_map_.contains(key)) {
      auto* combo = combo_map_[key];
      QString val = ConfigManager::instance().get<QString>(key);
      if (combo->currentData().toString() != val) {
        combo->blockSignals(true);
        int idx = combo->findData(val);
        if (idx >= 0)
          combo->setCurrentIndex(idx);
        combo->blockSignals(false);
      }
    }
    return;
  }

  if (spin_map_.contains(key)) {
    auto* spin = spin_map_[key];
    int val = ConfigManager::instance().get<int>(key);
    if (spin->value() != val) {
      spin->blockSignals(true);
      spin->setValue(val);
      spin->blockSignals(false);
    }
  } else if (check_map_.contains(key)) {
    auto* cb = check_map_[key];
    bool val = ConfigManager::instance().get<bool>(key);
    if (cb->isChecked() != val) {
      cb->blockSignals(true);
      cb->setChecked(val);
      cb->blockSignals(false);
    }
  } else if (combo_map_.contains(key)) {
    auto* combo = combo_map_[key];
    QString val = ConfigManager::instance().get<QString>(key);
    if (combo->currentText() != val) {
      combo->blockSignals(true);
      int idx = combo->findText(val);
      if (idx >= 0)
        combo->setCurrentIndex(idx);
      combo->blockSignals(false);
    }
  }
}

}  // namespace etest::app
