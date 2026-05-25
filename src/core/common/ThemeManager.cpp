#include "ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include "config/ConfigManager.h"
#include "config/ConfigDefs.h"

namespace etest::app {

using namespace etest::core::config;

ThemeManager& ThemeManager::instance() {
  static ThemeManager inst;
  return inst;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
  auto& cfg = ConfigManager::instance();
  QString theme = cfg.get<QString>(
      CONFIG_APPEARANCE_THEME,
      QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));

  loadQss(theme);
  current_theme_ = theme;

  connect(&cfg, &ConfigManager::configChanged, this, &ThemeManager::onConfigChanged);
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
  if (themeId == current_theme_) return;

  current_theme_ = themeId;

  loadQss(themeId);

  ConfigManager::instance().set(
      QString::fromLatin1(CONFIG_APPEARANCE_THEME), themeId);

  emit themeChanged(is_dark_);
}

void ThemeManager::loadQss(const QString& themeId) {
  QFile styleFile(
      QStringLiteral(":/resources/styles/%1.qss").arg(themeId));
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
}

bool ThemeManager::detectDarkFromQss(const QString& qss) const {
  static const QRegularExpression rx(
      QStringLiteral("background-color\\s*:\\s*(#[0-9a-fA-F]{3,8})"));
  auto match = rx.match(qss);
  if (!match.hasMatch()) return false;

  QColor bg(match.captured(1).trimmed());
  if (!bg.isValid()) return false;

  double luma = 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF();
  return luma < 0.4;
}

void ThemeManager::onConfigChanged(const QString& key) {
  if (key == QString::fromLatin1(CONFIG_APPEARANCE_THEME)) {
    QString theme = ConfigManager::instance().get<QString>(
        CONFIG_APPEARANCE_THEME,
        QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));
    setTheme(theme);
  }
}

}  // namespace etest::app
