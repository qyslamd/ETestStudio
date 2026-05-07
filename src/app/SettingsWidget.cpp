#include "SettingsWidget.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

namespace etest {
namespace app {

using namespace core::config;

SettingsWidget::SettingsWidget(QWidget* parent) : QDialog(parent) {
  initUi();
  initSignals();
}

void SettingsWidget::initUi() {
  setWindowTitle(QStringLiteral("设置"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  resize(700, 500);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // === Left: category tree ===
  tree_ = new QTreeWidget(this);
  tree_->setFixedWidth(180);
  tree_->setHeaderHidden(true);
  tree_->setIndentation(0);
  tree_->setRootIsDecorated(false);

  // Category items (order matches pages_ index)
  auto* itemGeneral = new QTreeWidgetItem(tree_, {QStringLiteral("常用")});
  auto* itemEditor = new QTreeWidgetItem(tree_, {QStringLiteral("编辑器")});
  auto* itemTerminal = new QTreeWidgetItem(tree_, {QStringLiteral("终端")});
  auto* itemAppearance = new QTreeWidgetItem(tree_, {QStringLiteral("外观")});

  // Assign indices for selection tracking
  itemGeneral->setData(0, Qt::UserRole, 0);
  itemEditor->setData(0, Qt::UserRole, 1);
  itemTerminal->setData(0, Qt::UserRole, 2);
  itemAppearance->setData(0, Qt::UserRole, 3);

  layout->addWidget(tree_);

  // === Right: scroll area with pages ===
  pages_ = new QStackedWidget(this);

  pages_->addWidget(createGeneralPage());    // index 0
  pages_->addWidget(createEditorPage());     // index 1
  pages_->addWidget(createTerminalPage());   // index 2
  pages_->addWidget(createAppearancePage()); // index 3

  scroll_area_ = new QScrollArea(this);
  scroll_area_->setWidgetResizable(true);
  scroll_area_->setWidget(pages_);
  scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  // Force dark background on viewport + content area
  QPalette darkPal;
  darkPal.setColor(QPalette::Window, QColor("#1E1E1E"));
  auto* vp = scroll_area_->viewport();
  vp->setAutoFillBackground(true);
  vp->setPalette(darkPal);
  pages_->setAutoFillBackground(true);
  pages_->setPalette(darkPal);

  layout->addWidget(scroll_area_, 1);  // stretch factor 1

  // Select first item by default
  tree_->setCurrentItem(itemGeneral);
  pages_->setCurrentIndex(0);
}

void SettingsWidget::initSignals() {
  // Tree item selection → switch page
  connect(tree_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/) {
            if (!current) return;
            int idx = current->data(0, Qt::UserRole).toInt();
            pages_->setCurrentIndex(idx);
          });

  // Config changes from external sources → update controls
  connect(&ConfigManager::instance(), &ConfigManager::configChanged, this,
          &SettingsWidget::onConfigChanged);
}

// =========================================================================
// Page creation
// =========================================================================

QWidget* SettingsWidget::createGeneralPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(8);

  // --- 编辑器 section ---
  layout->addWidget(createSectionHeader(QStringLiteral("编辑器")));
  addSpinBoxRow(page, QStringLiteral("字体大小"), CONFIG_EDITOR_FONT_SIZE, 8, 72,
                1, CONFIG_EDITOR_DEFAULT_FONT_SIZE);
  addCheckBoxRow(page, QStringLiteral("显示行号"), CONFIG_EDITOR_SHOW_LINE_NUMBER,
                 CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER);
  addCheckBoxRow(page, QStringLiteral("自动缩进"), CONFIG_EDITOR_AUTO_INDENT,
                 CONFIG_EDITOR_DEFAULT_AUTO_INDENT);
  addSpinBoxRow(page, QStringLiteral("Tab 宽度"), CONFIG_EDITOR_TAB_WIDTH, 2, 8,
                1, CONFIG_EDITOR_DEFAULT_TAB_WIDTH);
  addCheckBoxRow(page, QStringLiteral("空格替代 Tab"),
                 CONFIG_EDITOR_SPACES_FOR_TAB,
                 CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB);
  layout->addSpacing(12);

  // --- 终端 section ---
  layout->addWidget(createSectionHeader(QStringLiteral("终端")));
  addComboBoxRow(
      page, QStringLiteral("Shell"), CONFIG_TERMINAL_SHELL,
      {QStringLiteral("cmd.exe"), QStringLiteral("powershell.exe"),
       QStringLiteral("bash.exe")},
      QString::fromLatin1(CONFIG_TERMINAL_DEFAULT_SHELL));
  addSpinBoxRow(page, QStringLiteral("字体大小"), CONFIG_TERMINAL_FONT_SIZE, 8, 24,
                1, CONFIG_TERMINAL_DEFAULT_FONT_SIZE);
  addSpinBoxRow(page, QStringLiteral("滚动缓冲行数"), CONFIG_TERMINAL_SCROLLBACK,
                100, 100000, 1000, CONFIG_TERMINAL_DEFAULT_SCROLLBACK);
  layout->addSpacing(12);

  // --- 外观 section ---
  layout->addWidget(createSectionHeader(QStringLiteral("外观")));
  addCheckBoxRow(page, QStringLiteral("工具栏可见"), CONFIG_TOOLBAR_VISIBLE,
                 CONFIG_TOOLBAR_DEFAULT_VISIBLE);
  addButtonRow(page, QStringLiteral("窗口布局"),
               QStringLiteral("恢复默认"));
  layout->addStretch();

  return page;
}

QWidget* SettingsWidget::createEditorPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(8);

  addSpinBoxRow(page, QStringLiteral("字体大小"), CONFIG_EDITOR_FONT_SIZE, 8, 72,
                1, CONFIG_EDITOR_DEFAULT_FONT_SIZE);
  addCheckBoxRow(page, QStringLiteral("显示行号"), CONFIG_EDITOR_SHOW_LINE_NUMBER,
                 CONFIG_EDITOR_DEFAULT_SHOW_LINE_NUMBER);
  addCheckBoxRow(page, QStringLiteral("自动缩进"), CONFIG_EDITOR_AUTO_INDENT,
                 CONFIG_EDITOR_DEFAULT_AUTO_INDENT);
  addSpinBoxRow(page, QStringLiteral("Tab 宽度"), CONFIG_EDITOR_TAB_WIDTH, 2, 8,
                1, CONFIG_EDITOR_DEFAULT_TAB_WIDTH);
  addCheckBoxRow(page, QStringLiteral("空格替代 Tab"),
                 CONFIG_EDITOR_SPACES_FOR_TAB,
                 CONFIG_EDITOR_DEFAULT_SPACES_FOR_TAB);
  layout->addStretch();
  return page;
}

QWidget* SettingsWidget::createTerminalPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(8);

  addComboBoxRow(
      page, QStringLiteral("Shell"), CONFIG_TERMINAL_SHELL,
      {QStringLiteral("cmd.exe"), QStringLiteral("powershell.exe"),
       QStringLiteral("bash.exe")},
      QString::fromLatin1(CONFIG_TERMINAL_DEFAULT_SHELL));
  addSpinBoxRow(page, QStringLiteral("字体大小"), CONFIG_TERMINAL_FONT_SIZE, 8, 24,
                1, CONFIG_TERMINAL_DEFAULT_FONT_SIZE);
  addSpinBoxRow(page, QStringLiteral("滚动缓冲行数"), CONFIG_TERMINAL_SCROLLBACK,
                100, 100000, 1000, CONFIG_TERMINAL_DEFAULT_SCROLLBACK);
  layout->addStretch();
  return page;
}

QWidget* SettingsWidget::createAppearancePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(20, 16, 20, 16);
  layout->setSpacing(8);

  addCheckBoxRow(page, QStringLiteral("工具栏可见"), CONFIG_TOOLBAR_VISIBLE,
                 CONFIG_TOOLBAR_DEFAULT_VISIBLE);
  addButtonRow(page, QStringLiteral("窗口布局"),
               QStringLiteral("恢复默认"));
  layout->addStretch();
  return page;
}

// =========================================================================
// Section header
// =========================================================================

QWidget* SettingsWidget::createSectionHeader(const QString& title) {
  auto* label = new QLabel(title, this);
  label->setObjectName("SettingsSectionHeader");
  return label;
}

// =========================================================================
// Form row creators — each knows its ConfigKey and default value
// =========================================================================

QSpinBox* SettingsWidget::addSpinBoxRow(QWidget* parent,
                                        const QString& label,
                                        const QString& configKey,
                                        int min, int max, int step,
                                        int defaultVal) {
  auto* row = new QWidget(parent);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 2, 0, 2);

  auto* lbl = new QLabel(label, row);
  lbl->setFixedWidth(120);
  lbl->setStyleSheet("color: #CCCCCC;");

  auto* spin = new QSpinBox(row);
  spin->setRange(min, max);
  spin->setSingleStep(step);
  spin->setFixedWidth(100);

  // Read current value from config (or use default)
  int val = ConfigManager::instance().get<int>(configKey, defaultVal);
  spin->setValue(val);

  rowLayout->addWidget(lbl);
  rowLayout->addWidget(spin);
  rowLayout->addStretch();
  parent->layout()->addWidget(row);

  // Store for bidirectional sync
  spin_map_.insert(configKey, spin);
  spinBoxToConfig(configKey, spin);

  return spin;
}

QCheckBox* SettingsWidget::addCheckBoxRow(QWidget* parent,
                                          const QString& label,
                                          const QString& configKey,
                                          bool defaultVal) {
  auto* row = new QWidget(parent);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 2, 0, 2);

  auto* lbl = new QLabel(label, row);
  lbl->setFixedWidth(120);
  lbl->setStyleSheet("color: #CCCCCC;");

  auto* cb = new QCheckBox(row);
  cb->setStyleSheet("color: #CCCCCC;");

  bool val = ConfigManager::instance().get<bool>(configKey, defaultVal);
  cb->setChecked(val);

  rowLayout->addWidget(lbl);
  rowLayout->addWidget(cb);
  rowLayout->addStretch();
  parent->layout()->addWidget(row);

  check_map_.insert(configKey, cb);
  checkBoxToConfig(configKey, cb);

  return cb;
}

QComboBox* SettingsWidget::addComboBoxRow(QWidget* parent,
                                          const QString& label,
                                          const QString& configKey,
                                          const QStringList& items,
                                          const QString& defaultVal) {
  auto* row = new QWidget(parent);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 2, 0, 2);

  auto* lbl = new QLabel(label, row);
  lbl->setFixedWidth(120);
  lbl->setStyleSheet("color: #CCCCCC;");

  auto* combo = new QComboBox(row);
  combo->addItems(items);
  combo->setFixedWidth(160);

  QString val =
      ConfigManager::instance().get<QString>(configKey, defaultVal);
  int idx = combo->findText(val);
  if (idx >= 0) combo->setCurrentIndex(idx);

  rowLayout->addWidget(lbl);
  rowLayout->addWidget(combo);
  rowLayout->addStretch();
  parent->layout()->addWidget(row);

  combo_map_.insert(configKey, combo);
  comboBoxToConfig(configKey, combo);

  return combo;
}

QPushButton* SettingsWidget::addButtonRow(QWidget* parent,
                                          const QString& label,
                                          const QString& text) {
  auto* row = new QWidget(parent);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 2, 0, 2);

  auto* lbl = new QLabel(label, row);
  lbl->setFixedWidth(120);
  lbl->setStyleSheet("color: #CCCCCC;");

  auto* btn = new QPushButton(text, row);
  btn->setFixedWidth(100);

  rowLayout->addWidget(lbl);
  rowLayout->addWidget(btn);
  rowLayout->addStretch();
  parent->layout()->addWidget(row);

  connect(btn, &QPushButton::clicked, this, [this]() {
    ConfigManager::instance().resetAllToDefault();
  });

  return btn;
}

// =========================================================================
// Bidirectional binding: control → config and config → control
// =========================================================================

void SettingsWidget::spinBoxToConfig(const QString& key, QSpinBox* spin) {
  connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [key](int val) { ConfigManager::instance().set<int>(key, val); });
}

void SettingsWidget::checkBoxToConfig(const QString& key, QCheckBox* cb) {
  connect(cb, &QCheckBox::toggled, this,
          [key](bool checked) { ConfigManager::instance().set<bool>(key, checked); });
}

void SettingsWidget::comboBoxToConfig(const QString& key, QComboBox* combo) {
  connect(combo, &QComboBox::currentTextChanged, this,
          [key](const QString& text) {
            ConfigManager::instance().set<QString>(key, text);
          });
}

void SettingsWidget::onConfigChanged(const QString& key) {
  // Suppress re-entrant updates: config changes triggered by our own controls
  // would otherwise cause infinite loops
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
      if (idx >= 0) combo->setCurrentIndex(idx);
      combo->blockSignals(false);
    }
  }
}

}  // namespace app
}  // namespace etest
