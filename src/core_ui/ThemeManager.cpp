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

ThemeManager& ThemeManager::instance() {
  static ThemeManager inst;
  return inst;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
  auto& cfg = ConfigManager::instance();
  QString theme =
      cfg.get<QString>(CONFIG_APPEARANCE_THEME,
                       QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));

  loadQss(theme);
  current_theme_ = theme;
  applyEditorTheme();

  connect(&cfg, &ConfigManager::configChanged, this,
          &ThemeManager::onConfigChanged);
}

ThemeManager::~ThemeManager() = default;

bool ThemeManager::isDarkTheme() const {
  return is_dark_;
}

QString ThemeManager::currentTheme() const {
  return current_theme_;
}

// -- 语义色板 --

static bool isChineseRed(const QString& theme) {
  return theme == QStringLiteral("chinese_red");
}

QColor ThemeManager::windowBackground() const {
  if (isChineseRed(current_theme_)) return QColor("#1A1A1A");
  if (is_dark_) return QColor("#1E1E1E");
  return QColor(0xF0, 0xF0, 0xF0);
}

QColor ThemeManager::panelBackground() const {
  if (isChineseRed(current_theme_)) return QColor("#241F20");
  if (is_dark_) return QColor("#252526");
  return QColor(0xF0, 0xF0, 0xF0);
}

QColor ThemeManager::toolbarBackground() const {
  if (isChineseRed(current_theme_)) return QColor("#33292B");
  if (is_dark_) return QColor("#3C3C3C");
  return QColor(0xF0, 0xF0, 0xF0);
}

QColor ThemeManager::hoverBackground() const {
  if (isChineseRed(current_theme_)) return QColor("#4A3437");
  if (is_dark_) return QColor("#505050");
  return QColor(0xE0, 0xE0, 0xE0);
}

QColor ThemeManager::selectionBackground() const {
  if (isChineseRed(current_theme_)) return QColor("#5D1A1A");
  if (is_dark_) return QColor("#094771");
  return QColor(0xCC, 0xE4, 0xF7);
}

QColor ThemeManager::borderColor() const {
  if (isChineseRed(current_theme_)) return QColor("#4A3A3C");
  if (is_dark_) return QColor("#454545");
  return QColor(0xCC, 0xCC, 0xCC);
}

QColor ThemeManager::textColor() const {
  if (isChineseRed(current_theme_)) return QColor("#D4C5C5");
  if (is_dark_) return QColor("#CCCCCC");
  return QColor(0x33, 0x33, 0x33);
}

QColor ThemeManager::secondaryTextColor() const {
  if (isChineseRed(current_theme_)) return QColor("#9A8585");
  if (is_dark_) return QColor("#858585");
  return QColor(0x88, 0x88, 0x88);
}

QColor ThemeManager::disabledTextColor() const {
  if (isChineseRed(current_theme_)) return QColor("#5A4A4A");
  if (is_dark_) return QColor("#5A5A5A");
  return QColor(0xAA, 0xAA, 0xAA);
}

QColor ThemeManager::accentColor() const {
  if (isChineseRed(current_theme_)) return QColor("#C62828");
  return QColor("#007ACC");
}

QColor ThemeManager::statusBarBackground() const {
  if (isChineseRed(current_theme_)) return QColor("#B71C1C");
  if (is_dark_) return QColor("#007ACC");
  return QColor(0xF0, 0xF0, 0xF0);
}

QColor ThemeManager::clockFaceBackground() const {
  if (isChineseRed(current_theme_)) return QColor("#241F20");
  if (is_dark_) return QColor("#252526");
  return QColor(0xF6, 0xF6, 0xF6);
}

QColor ThemeManager::clockHandColor() const {
  if (isChineseRed(current_theme_)) return QColor("#D4C5C5");
  if (is_dark_) return QColor("#CCCCCC");
  return QColor(0x33, 0x33, 0x33);
}

QColor ThemeManager::clockSecondaryColor() const {
  if (isChineseRed(current_theme_)) return QColor("#9A8585");
  if (is_dark_) return QColor("#858585");
  return QColor(0x55, 0x55, 0x55);
}

QColor ThemeManager::clockAccentColor() const {
  if (isChineseRed(current_theme_)) return QColor("#D4AF37");
  return QColor(0xFF, 0x66, 0x00);
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

  // ads_dark.qss 同时设到 qApp（覆盖浮动窗口等顶级 QADS 窗口）
  // 和 CDockManager（覆盖 QADS 内置 widget 级 default.css）
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
  // Parse hex color manually (no QColor dependency)
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
  auto& cfg = ConfigManager::instance();
  if (isChineseRed(current_theme_)) {
    // ChineseRed 配色：基于 Dark+，关键字/标签用中国红，函数用金色
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_PAPER),
            QStringLiteral("#1A1A1A"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_TEXT),
            QStringLiteral("#D4C5C5"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_CARET_LINE),
            QStringLiteral("#2A2425"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_CARET),
            QStringLiteral("#D4C5C5"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_SELECTION_BG),
            QStringLiteral("#4A1A1A"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_SELECTION_FG),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_MARGIN_BG),
            QStringLiteral("#241F20"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_LINE_NUMBER),
            QStringLiteral("#9A8585"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_INDENT_GUIDE),
            QStringLiteral("#4A3A3C"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_LIGHT_BG),
            QStringLiteral("#4A1A1A"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_LIGHT_FG),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_BAD_BG),
            QStringLiteral("#8B0000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_BAD_FG),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_FOLD_MARGIN),
            QStringLiteral("#9A8585"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_KEYWORD),
            QStringLiteral("#C62828"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_COMMENT),
            QStringLiteral("#6A9955"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_STRING),
            QStringLiteral("#CE9178"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_NUMBER),
            QStringLiteral("#B5CEA8"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_FUNCTION),
            QStringLiteral("#D4AF37"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_TAG),
            QStringLiteral("#C62828"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_PREPROCESSOR),
            QStringLiteral("#9B9B9B"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS),
            QStringLiteral("#4EC9B0"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ),
            QStringLiteral("#D7BA7D"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_PROPERTY),
            QStringLiteral("#D4AF37"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_OPERATOR),
            QStringLiteral("#D4C5C5"));
  } else if (is_dark_) {
    // VS Code Dark+ 配色
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_PAPER),
            QStringLiteral("#1E1E1E"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_TEXT),
            QStringLiteral("#CCCCCC"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_CARET_LINE),
            QStringLiteral("#2E2E2E"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_CARET),
            QStringLiteral("#CCCCCC"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_SELECTION_BG),
            QStringLiteral("#264F78"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_SELECTION_FG),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_MARGIN_BG),
            QStringLiteral("#252525"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_LINE_NUMBER),
            QStringLiteral("#858585"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_INDENT_GUIDE),
            QStringLiteral("#434343"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_LIGHT_BG),
            QStringLiteral("#264F78"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_LIGHT_FG),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_BAD_BG),
            QStringLiteral("#8B0000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_BAD_FG),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_FOLD_MARGIN),
            QStringLiteral("#858585"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_KEYWORD),
            QStringLiteral("#569CD6"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_COMMENT),
            QStringLiteral("#6A9955"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_STRING),
            QStringLiteral("#CE9178"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_NUMBER),
            QStringLiteral("#B5CEA8"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_FUNCTION),
            QStringLiteral("#DCDCAA"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_TAG),
            QStringLiteral("#569CD6"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_PREPROCESSOR),
            QStringLiteral("#9B9B9B"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS),
            QStringLiteral("#4EC9B0"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ),
            QStringLiteral("#D7BA7D"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_PROPERTY),
            QStringLiteral("#DCDCAA"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_OPERATOR),
            QStringLiteral("#CCCCCC"));
  } else {
    // VS Code Light+ 配色
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_PAPER),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_TEXT),
            QStringLiteral("#000000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_CARET_LINE),
            QStringLiteral("#E8F0FE"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_CARET),
            QStringLiteral("#000000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_SELECTION_BG),
            QStringLiteral("#ADD6FF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_SELECTION_FG),
            QStringLiteral("#000000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_MARGIN_BG),
            QStringLiteral("#F3F3F3"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_LINE_NUMBER),
            QStringLiteral("#888888"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_INDENT_GUIDE),
            QStringLiteral("#D3D3D3"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_LIGHT_BG),
            QStringLiteral("#ADD6FF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_LIGHT_FG),
            QStringLiteral("#000000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_BAD_BG),
            QStringLiteral("#E57373"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_BRACE_BAD_FG),
            QStringLiteral("#FFFFFF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_THEME_FOLD_MARGIN),
            QStringLiteral("#888888"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_KEYWORD),
            QStringLiteral("#0000FF"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_COMMENT),
            QStringLiteral("#008000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_STRING),
            QStringLiteral("#A31515"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_NUMBER),
            QStringLiteral("#098658"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_FUNCTION),
            QStringLiteral("#795E26"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_TAG),
            QStringLiteral("#800000"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_PREPROCESSOR),
            QStringLiteral("#888888"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_GLOBAL_CLASS),
            QStringLiteral("#267F99"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_ESCAPE_SEQ),
            QStringLiteral("#E57373"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_PROPERTY),
            QStringLiteral("#795E26"));
    cfg.set(QString::fromLatin1(CONFIG_EDITOR_SYNTAX_OPERATOR),
            QStringLiteral("#000000"));
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
