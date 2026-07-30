#include "dialogs/SettingsDialog.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "backup/BackupManager.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

namespace etest::app {

using namespace core::config;

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void SettingsDialog::initUi() {
  setWindowTitle(QStringLiteral("设置"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  resize(800, 500);

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // === Content area: navigation + pages ===
  auto* contentLayout = new QHBoxLayout();
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  // === Left: category list ===
  list_ = new QListWidget(this);
  list_->setFixedWidth(180);
  list_->setIconSize(QSize(16, 16));
  list_->setFocusPolicy(Qt::NoFocus);
  list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QListWidgetItem* itemEditor =
      new QListWidgetItem(QStringLiteral("编辑器"), list_);
  QListWidgetItem* itemTerminal =
      new QListWidgetItem(QStringLiteral("终端"), list_);
  QListWidgetItem* itemAppearance =
      new QListWidgetItem(QStringLiteral("外观"), list_);
  QListWidgetItem* itemProject =
      new QListWidgetItem(QStringLiteral("项目"), list_);
  QListWidgetItem* itemBackup =
      new QListWidgetItem(QStringLiteral("备份"), list_);

  itemEditor->setData(Qt::UserRole, 0);
  itemTerminal->setData(Qt::UserRole, 1);
  itemAppearance->setData(Qt::UserRole, 2);
  itemProject->setData(Qt::UserRole, 3);
  itemBackup->setData(Qt::UserRole, 4);

  contentLayout->addWidget(list_);

  // === Right: stacked pages ===
  pages_ = new QStackedWidget(this);
  pages_->addWidget(createEditorPage());      // index 0
  pages_->addWidget(createTerminalPage());    // index 1
  pages_->addWidget(createAppearancePage());  // index 2
  pages_->addWidget(createProjectPage());     // index 3
  pages_->addWidget(createBackupPage());      // index 4

  contentLayout->addWidget(pages_, 1);
  mainLayout->addLayout(contentLayout, 1);

  // === Bottom: button bar ===
  auto* buttonBar = new QWidget(this);
  buttonBar->setObjectName("SettingsButtonBar");
  auto* buttonLayout = new QHBoxLayout(buttonBar);
  buttonLayout->setContentsMargins(12, 8, 12, 8);
  buttonLayout->addStretch();

  btn_ok_ = new QPushButton(QStringLiteral("确定"), buttonBar);
  btn_cancel_ = new QPushButton(QStringLiteral("取消"), buttonBar);
  btn_apply_ = new QPushButton(QStringLiteral("应用"), buttonBar);

  btn_ok_->setFixedWidth(80);
  btn_cancel_->setFixedWidth(80);
  btn_apply_->setFixedWidth(80);

  buttonLayout->addWidget(btn_ok_);
  buttonLayout->addWidget(btn_cancel_);
  buttonLayout->addWidget(btn_apply_);

  mainLayout->addWidget(buttonBar);

  // Select first item by default
  list_->setCurrentRow(0);
  pages_->setCurrentIndex(0);
}

void SettingsDialog::initSignals() {
  connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0)
      pages_->setCurrentIndex(row);
  });

  connect(&ConfigManager::instance(), &ConfigManager::configChanged, this,
          &SettingsDialog::onConfigChanged);

  connect(btn_ok_, &QPushButton::clicked, this, &QDialog::accept);
  connect(btn_cancel_, &QPushButton::clicked, this, &QDialog::reject);
  connect(btn_apply_, &QPushButton::clicked, this,
          []() { ConfigManager::instance().sync(); });
}

// =========================================================================
// Page creation
// =========================================================================

QWidget* SettingsDialog::createEditorPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

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

  layout->addStretch();
  return page;
}

QWidget* SettingsDialog::createTerminalPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

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

  layout->addStretch();
  return page;
}

QWidget* SettingsDialog::createAppearancePage() {
  // Outer page wraps a QScrollArea because this page has the most content
  auto* page = new QWidget(this);
  auto* pageLayout = new QVBoxLayout(page);
  pageLayout->setContentsMargins(0, 0, 0, 0);

  auto* scrollArea = new QScrollArea(page);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setFrameShape(QFrame::NoFrame);

  auto* scrollContent = new QWidget();
  scrollArea->setWidget(scrollContent);
  auto* layout = new QVBoxLayout(scrollContent);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  // --- 外观 card ---
  auto* cardAppearance =
      createSettingsCard(scrollContent, QStringLiteral("外观"));

  // 主题选择（使用 userData 存储配置值）
  {
    auto* rightLayout = addSettingRow(cardAppearance, QStringLiteral("主题"),
                                      QStringLiteral("应用程序界面色彩主题"));
    auto* combo = new QComboBox();
    combo->addItem(QStringLiteral("默认主题"), QStringLiteral("default"));
    combo->addItem(QStringLiteral("VS Code 暗黑"), QStringLiteral("vscode"));
    combo->addItem(QStringLiteral("中国红"), QStringLiteral("chinese_red"));
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
  auto* cardBg =
      createSettingsCard(scrollContent, QStringLiteral("欢迎页背景"));

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
  auto* cardSaver = createSettingsCard(scrollContent, QStringLiteral("屏保"));

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

  layout->addStretch();
  pageLayout->addWidget(scrollArea);
  return page;
}

QWidget* SettingsDialog::createProjectPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

  auto* card = createSettingsCard(page, QStringLiteral("项目"));
  addCheckBoxRow(card, QStringLiteral("自动打开所属项目"),
                 QStringLiteral("打开文件时自动打开其所属项目"),
                 CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT,
                 CONFIG_RECENT_FILE_AUTO_OPEN_PROJECT_DEFAULT);
  addCheckBoxRow(card, QStringLiteral("提示初始化 Git 仓库"),
                 QStringLiteral("打开未初始化 Git 的项目时弹窗询问是否初始化"),
                 CONFIG_PROJECT_GIT_PROMPT_INIT,
                 CONFIG_PROJECT_DEFAULT_GIT_PROMPT_INIT);

  layout->addStretch();
  return page;
}

QWidget* SettingsDialog::createBackupPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(12);

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

  layout->addStretch();
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

QCheckBox* SettingsDialog::addCheckBoxRow(QWidget* parent,
                                          const QString& title,
                                          const QString& description,
                                          const QString& configKey,
                                          bool defaultVal) {
  auto* rightLayout = addSettingRow(parent, title, description);

  auto* cb = new QCheckBox();

  bool val = ConfigManager::instance().get<bool>(configKey, defaultVal);
  cb->setChecked(val);

  rightLayout->addWidget(cb);

  check_map_.insert(configKey, cb);
  checkBoxToConfig(configKey, cb);

  return cb;
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

void SettingsDialog::checkBoxToConfig(const QString& key, QCheckBox* cb) {
  connect(cb, &QCheckBox::toggled, this, [key](bool checked) {
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
      key == QString::fromLatin1(CONFIG_TUXSAVER_MODE)) {
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
