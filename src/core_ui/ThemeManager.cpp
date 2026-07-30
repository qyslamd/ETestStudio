#include "ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

namespace etest::core_ui {

using namespace etest::core::config;

// ── 内置 palette 工厂 ──

static ThemePalette makeDarkPalette() {
  ThemePalette p;
  p.themeId = QStringLiteral("vscode");
  p.isDark = true;
  p.ribbonBaseTheme = 5;  // Dark2
  p.windowBackground = QColor("#1E1E1E");
  p.panelBackground = QColor("#252526");
  p.toolbarBackground = QColor("#3C3C3C");
  p.hoverBackground = QColor("#505050");
  p.selectionBackground = QColor("#094771");
  p.borderColor = QColor("#454545");
  p.textColor = QColor("#CCCCCC");
  p.secondaryTextColor = QColor("#858585");
  p.disabledTextColor = QColor("#5A5A5A");
  p.accentColor = QColor("#007ACC");
  p.statusBarBackground = QColor("#007ACC");
  p.clockFaceBackground = QColor("#252526");
  p.clockHandColor = QColor("#CCCCCC");
  p.clockSecondaryColor = QColor("#858585");
  p.clockAccentColor = QColor(0xFF, 0x66, 0x00);
  // Editor: VS Code Dark+
  p.editorColors = {
      {CONFIG_EDITOR_THEME_PAPER, "#1E1E1E"},
      {CONFIG_EDITOR_THEME_TEXT, "#CCCCCC"},
      {CONFIG_EDITOR_THEME_CARET_LINE, "#2E2E2E"},
      {CONFIG_EDITOR_THEME_CARET, "#CCCCCC"},
      {CONFIG_EDITOR_THEME_SELECTION_BG, "#264F78"},
      {CONFIG_EDITOR_THEME_SELECTION_FG, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_MARGIN_BG, "#252525"},
      {CONFIG_EDITOR_THEME_LINE_NUMBER, "#858585"},
      {CONFIG_EDITOR_THEME_INDENT_GUIDE, "#434343"},
      {CONFIG_EDITOR_THEME_BRACE_LIGHT_BG, "#264F78"},
      {CONFIG_EDITOR_THEME_BRACE_LIGHT_FG, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_BRACE_BAD_BG, "#8B0000"},
      {CONFIG_EDITOR_THEME_BRACE_BAD_FG, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_FOLD_MARGIN, "#858585"},
      {CONFIG_EDITOR_SYNTAX_KEYWORD, "#569CD6"},
      {CONFIG_EDITOR_SYNTAX_COMMENT, "#6A9955"},
      {CONFIG_EDITOR_SYNTAX_STRING, "#CE9178"},
      {CONFIG_EDITOR_SYNTAX_NUMBER, "#B5CEA8"},
      {CONFIG_EDITOR_SYNTAX_FUNCTION, "#DCDCAA"},
      {CONFIG_EDITOR_SYNTAX_TAG, "#569CD6"},
      {CONFIG_EDITOR_SYNTAX_PREPROCESSOR, "#9B9B9B"},
      {CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS, "#4EC9B0"},
      {CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ, "#D7BA7D"},
      {CONFIG_EDITOR_SYNTAX_PROPERTY, "#DCDCAA"},
      {CONFIG_EDITOR_SYNTAX_OPERATOR, "#CCCCCC"},
  };
  return p;
}

static ThemePalette makeLightPalette() {
  ThemePalette p;
  p.themeId = QStringLiteral("default");
  p.isDark = false;
  p.ribbonBaseTheme = 2;  // Office2021Blue
  p.windowBackground = QColor(0xF0, 0xF0, 0xF0);
  p.panelBackground = QColor(0xF0, 0xF0, 0xF0);
  p.toolbarBackground = QColor(0xF0, 0xF0, 0xF0);
  p.hoverBackground = QColor(0xE0, 0xE0, 0xE0);
  p.selectionBackground = QColor(0xCC, 0xE4, 0xF7);
  p.borderColor = QColor(0xCC, 0xCC, 0xCC);
  p.textColor = QColor(0x33, 0x33, 0x33);
  p.secondaryTextColor = QColor(0x88, 0x88, 0x88);
  p.disabledTextColor = QColor(0xAA, 0xAA, 0xAA);
  p.accentColor = QColor("#007ACC");
  p.statusBarBackground = QColor(0xF0, 0xF0, 0xF0);
  p.clockFaceBackground = QColor(0xF6, 0xF6, 0xF6);
  p.clockHandColor = QColor(0x33, 0x33, 0x33);
  p.clockSecondaryColor = QColor(0x55, 0x55, 0x55);
  p.clockAccentColor = QColor(0xFF, 0x66, 0x00);
  // Editor: VS Code Light+
  p.editorColors = {
      {CONFIG_EDITOR_THEME_PAPER, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_TEXT, "#000000"},
      {CONFIG_EDITOR_THEME_CARET_LINE, "#E8F0FE"},
      {CONFIG_EDITOR_THEME_CARET, "#000000"},
      {CONFIG_EDITOR_THEME_SELECTION_BG, "#ADD6FF"},
      {CONFIG_EDITOR_THEME_SELECTION_FG, "#000000"},
      {CONFIG_EDITOR_THEME_MARGIN_BG, "#F3F3F3"},
      {CONFIG_EDITOR_THEME_LINE_NUMBER, "#888888"},
      {CONFIG_EDITOR_THEME_INDENT_GUIDE, "#D3D3D3"},
      {CONFIG_EDITOR_THEME_BRACE_LIGHT_BG, "#ADD6FF"},
      {CONFIG_EDITOR_THEME_BRACE_LIGHT_FG, "#000000"},
      {CONFIG_EDITOR_THEME_BRACE_BAD_BG, "#E57373"},
      {CONFIG_EDITOR_THEME_BRACE_BAD_FG, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_FOLD_MARGIN, "#888888"},
      {CONFIG_EDITOR_SYNTAX_KEYWORD, "#0000FF"},
      {CONFIG_EDITOR_SYNTAX_COMMENT, "#008000"},
      {CONFIG_EDITOR_SYNTAX_STRING, "#A31515"},
      {CONFIG_EDITOR_SYNTAX_NUMBER, "#098658"},
      {CONFIG_EDITOR_SYNTAX_FUNCTION, "#795E26"},
      {CONFIG_EDITOR_SYNTAX_TAG, "#800000"},
      {CONFIG_EDITOR_SYNTAX_PREPROCESSOR, "#888888"},
      {CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS, "#267F99"},
      {CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ, "#E57373"},
      {CONFIG_EDITOR_SYNTAX_PROPERTY, "#795E26"},
      {CONFIG_EDITOR_SYNTAX_OPERATOR, "#000000"},
  };
  return p;
}

static ThemePalette makeChineseRedPalette() {
  ThemePalette p;
  p.themeId = QStringLiteral("chinese_red");
  p.isDark = true;
  p.ribbonBaseTheme = 5;  // Dark2
  p.windowBackground = QColor("#1A1A1A");
  p.panelBackground = QColor("#241F20");
  p.toolbarBackground = QColor("#33292B");
  p.hoverBackground = QColor("#4A3437");
  p.selectionBackground = QColor("#5D1A1A");
  p.borderColor = QColor("#4A3A3C");
  p.textColor = QColor("#D4C5C5");
  p.secondaryTextColor = QColor("#9A8585");
  p.disabledTextColor = QColor("#5A4A4A");
  p.accentColor = QColor("#C62828");
  p.statusBarBackground = QColor("#B71C1C");
  p.clockFaceBackground = QColor("#241F20");
  p.clockHandColor = QColor("#D4C5C5");
  p.clockSecondaryColor = QColor("#9A8585");
  p.clockAccentColor = QColor("#D4AF37");
  // Editor: 基于 Dark+，关键字/标签用中国红，函数用金色
  p.editorColors = {
      {CONFIG_EDITOR_THEME_PAPER, "#1A1A1A"},
      {CONFIG_EDITOR_THEME_TEXT, "#D4C5C5"},
      {CONFIG_EDITOR_THEME_CARET_LINE, "#2A2425"},
      {CONFIG_EDITOR_THEME_CARET, "#D4C5C5"},
      {CONFIG_EDITOR_THEME_SELECTION_BG, "#4A1A1A"},
      {CONFIG_EDITOR_THEME_SELECTION_FG, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_MARGIN_BG, "#241F20"},
      {CONFIG_EDITOR_THEME_LINE_NUMBER, "#9A8585"},
      {CONFIG_EDITOR_THEME_INDENT_GUIDE, "#4A3A3C"},
      {CONFIG_EDITOR_THEME_BRACE_LIGHT_BG, "#4A1A1A"},
      {CONFIG_EDITOR_THEME_BRACE_LIGHT_FG, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_BRACE_BAD_BG, "#8B0000"},
      {CONFIG_EDITOR_THEME_BRACE_BAD_FG, "#FFFFFF"},
      {CONFIG_EDITOR_THEME_FOLD_MARGIN, "#9A8585"},
      {CONFIG_EDITOR_SYNTAX_KEYWORD, "#C62828"},
      {CONFIG_EDITOR_SYNTAX_COMMENT, "#6A9955"},
      {CONFIG_EDITOR_SYNTAX_STRING, "#CE9178"},
      {CONFIG_EDITOR_SYNTAX_NUMBER, "#B5CEA8"},
      {CONFIG_EDITOR_SYNTAX_FUNCTION, "#D4AF37"},
      {CONFIG_EDITOR_SYNTAX_TAG, "#C62828"},
      {CONFIG_EDITOR_SYNTAX_PREPROCESSOR, "#9B9B9B"},
      {CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS, "#4EC9B0"},
      {CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ, "#D7BA7D"},
      {CONFIG_EDITOR_SYNTAX_PROPERTY, "#D4AF37"},
      {CONFIG_EDITOR_SYNTAX_OPERATOR, "#D4C5C5"},
  };
  return p;
}

// ── ThemeManager ──

ThemeManager& ThemeManager::instance() {
  static ThemeManager inst;
  return inst;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
  registerBuiltinPalettes();

  auto& cfg = ConfigManager::instance();
  QString theme =
      cfg.get<QString>(CONFIG_APPEARANCE_THEME,
                       QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));

  loadQss(theme);
  current_theme_ = theme;
  auto it = palettes_.find(theme);
  palette_ = (it != palettes_.end()) ? &it.value() : nullptr;
  is_dark_ = palette_ ? palette_->isDark : true;
  applyEditorTheme();

  connect(&cfg, &ConfigManager::configChanged, this,
          &ThemeManager::onConfigChanged);
}

ThemeManager::~ThemeManager() = default;

void ThemeManager::registerBuiltinPalettes() {
  palettes_[QStringLiteral("default")] = makeLightPalette();
  palettes_[QStringLiteral("vscode")] = makeDarkPalette();
  palettes_[QStringLiteral("chinese_red")] = makeChineseRedPalette();
}

bool ThemeManager::isDarkTheme() const {
  return is_dark_;
}

QString ThemeManager::currentTheme() const {
  return current_theme_;
}

int ThemeManager::ribbonBaseTheme() const {
  return palette_ ? palette_->ribbonBaseTheme : 5;
}

// -- 语义色板（一行委托）--

QColor ThemeManager::windowBackground() const {
  return palette_ ? palette_->windowBackground : QColor();
}
QColor ThemeManager::panelBackground() const {
  return palette_ ? palette_->panelBackground : QColor();
}
QColor ThemeManager::toolbarBackground() const {
  return palette_ ? palette_->toolbarBackground : QColor();
}
QColor ThemeManager::hoverBackground() const {
  return palette_ ? palette_->hoverBackground : QColor();
}
QColor ThemeManager::selectionBackground() const {
  return palette_ ? palette_->selectionBackground : QColor();
}
QColor ThemeManager::borderColor() const {
  return palette_ ? palette_->borderColor : QColor();
}
QColor ThemeManager::textColor() const {
  return palette_ ? palette_->textColor : QColor();
}
QColor ThemeManager::secondaryTextColor() const {
  return palette_ ? palette_->secondaryTextColor : QColor();
}
QColor ThemeManager::disabledTextColor() const {
  return palette_ ? palette_->disabledTextColor : QColor();
}
QColor ThemeManager::accentColor() const {
  return palette_ ? palette_->accentColor : QColor();
}
QColor ThemeManager::statusBarBackground() const {
  return palette_ ? palette_->statusBarBackground : QColor();
}
QColor ThemeManager::clockFaceBackground() const {
  return palette_ ? palette_->clockFaceBackground : QColor();
}
QColor ThemeManager::clockHandColor() const {
  return palette_ ? palette_->clockHandColor : QColor();
}
QColor ThemeManager::clockSecondaryColor() const {
  return palette_ ? palette_->clockSecondaryColor : QColor();
}
QColor ThemeManager::clockAccentColor() const {
  return palette_ ? palette_->clockAccentColor : QColor();
}

QStringList ThemeManager::availableThemes() const {
  QSet<QString> themes;
  QDir dir(QStringLiteral(":/resources/styles"));
  if (dir.exists()) {
    const auto entries = dir.entryList({QStringLiteral("*.qss")}, QDir::Files);
    for (const auto& fn : entries) {
      if (!fn.startsWith(QStringLiteral("ads_"))) {
        themes.insert(QFileInfo(fn).completeBaseName());
      }
    }
  }
  if (themes.isEmpty()) {
    themes.insert(QStringLiteral("default"));
    themes.insert(QStringLiteral("vscode"));
  }
  return themes.values();
}

void ThemeManager::setTheme(const QString& themeId) {
  if (themeId == current_theme_)
    return;

  current_theme_ = themeId;
  auto it = palettes_.find(themeId);
  palette_ = (it != palettes_.end()) ? &it.value() : nullptr;
  is_dark_ = palette_ ? palette_->isDark : true;

  loadQss(themeId);
  applyEditorTheme();

  ConfigManager::instance().set(QString::fromLatin1(CONFIG_APPEARANCE_THEME),
                                themeId);

  emit themeChanged(is_dark_);
}

void ThemeManager::loadQss(const QString& themeId) {
  QFile styleFile(QStringLiteral(":/resources/styles/%1.qss").arg(themeId));
  QString qss;
  if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qss = QString::fromUtf8(styleFile.readAll());
    styleFile.close();
  }

  is_dark_ = detectDarkFromQss(qss);

  if (is_dark_) {
    QFile adsFile(QStringLiteral(":/resources/styles/ads_dark.qss"));
    if (adsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qss += QStringLiteral("\n") + QString::fromUtf8(adsFile.readAll());
      adsFile.close();
    }
  }

  qApp->setStyleSheet(qss);
  emit themeChanged(is_dark_);
}

bool ThemeManager::detectDarkFromQss(const QString& qss) const {
  static const QRegularExpression rx(
      QStringLiteral("background-color\\s*:\\s*(#[0-9a-fA-F]{3,8})"));
  auto match = rx.match(qss);
  if (!match.hasMatch()) return false;

  QString hex = match.captured(1).trimmed();
  if (hex.startsWith('#')) hex = hex.mid(1);
  if (hex.length() < 6) return false;
  bool ok1, ok2, ok3;
  int r = hex.mid(0, 2).toInt(&ok1, 16);
  int g = hex.mid(2, 2).toInt(&ok2, 16);
  int b = hex.mid(4, 2).toInt(&ok3, 16);
  if (!ok1 || !ok2 || !ok3) return false;
  double luma = 0.2126 * r / 255.0 + 0.7152 * g / 255.0 + 0.0722 * b / 255.0;
  return luma < 0.4;
}

void ThemeManager::applyEditorTheme() {
  if (!palette_) return;
  auto& cfg = ConfigManager::instance();
  for (auto it = palette_->editorColors.constBegin();
       it != palette_->editorColors.constEnd(); ++it) {
    cfg.set(it.key(), it.value());
  }
}

void ThemeManager::onConfigChanged(const QString& key) {
  if (key == QString::fromLatin1(CONFIG_APPEARANCE_THEME)) {
    QString theme = ConfigManager::instance().get<QString>(
        CONFIG_APPEARANCE_THEME,
        QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));
    setTheme(theme);
  }
}

}  // namespace etest::core_ui
