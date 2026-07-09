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
  applyEditorTheme();
  current_theme_ = theme;

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
  if (is_dark_) {
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
