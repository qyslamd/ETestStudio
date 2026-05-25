#include "ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include "core/common/ThemeState.h"
#include "core/config/ConfigManager.h"
#include "core/config/ConfigDefs.h"

namespace etest::app {

using namespace etest::core::config;

ThemeManager& ThemeManager::instance() {
  static ThemeManager inst;
  return inst;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
  // 从配置读取主题
  auto& cfg = ConfigManager::instance();
  QString theme = cfg.get<QString>(
      CONFIG_APPEARANCE_THEME,
      QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));

  // 加载 QSS 并检测暗亮
  loadQss(theme);
  current_theme_ = theme;

  // 同步遗留状态
  syncLegacyState();

  // 监听外部配置变更
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
  // 从 images 目录读取所有 .qss 文件
  QDir dir(QStringLiteral(":/resources/styles"));
  if (dir.exists()) {
    const auto entries = dir.entryList({QStringLiteral("*.qss")}, QDir::Files);
    for (const auto& fn : entries) {
      // 过滤 ads_ 前缀（ADS 补丁样式，非独立主题）
      if (!fn.startsWith(QStringLiteral("ads_"))) {
        themes.insert(QFileInfo(fn).completeBaseName());
      }
    }
  }
  // 兜底：确保至少包含已知主题
  if (themes.isEmpty()) {
    themes.insert(QStringLiteral("default"));
    themes.insert(QStringLiteral("vscode"));
  }
  return themes.values();
}

void ThemeManager::setTheme(const QString& themeId) {
  // guard：防止 re-entry（configChanged → setTheme 环）
  if (themeId == current_theme_) return;

  current_theme_ = themeId;

  // 加载 QSS 并检测暗亮
  loadQss(themeId);

  // 同步遗留状态
  syncLegacyState();

  // 持久化配置
  ConfigManager::instance().set(
      QString::fromLatin1(CONFIG_APPEARANCE_THEME), themeId);

  // 通知所有 widget
  emit themeChanged(is_dark_);
}

void ThemeManager::loadQss(const QString& themeId) {
  // 加载主样式表
  QFile styleFile(
      QStringLiteral(":/resources/styles/%1.qss").arg(themeId));
  QString qss;
  if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qss = QString::fromUtf8(styleFile.readAll());
    styleFile.close();
  }

  // 检测暗亮（在应用 ADS 补丁前检测，主 QSS 的背景色决定主题类型）
  is_dark_ = detectDarkFromQss(qss);

  // 暗色主题时加载 ADS 暗色覆盖
  if (is_dark_) {
    QFile adsFile(QStringLiteral(":/resources/styles/ads_dark.qss"));
    if (adsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qss += QStringLiteral("\n") + QString::fromUtf8(adsFile.readAll());
      adsFile.close();
    }
  }

  // 应用到全局
  qApp->setStyleSheet(qss);
}

bool ThemeManager::detectDarkFromQss(const QString& qss) const {
  // 尝试提取 background-color 的十六进制值
  static const QRegularExpression rx(
      QStringLiteral("background-color\\s*:\\s*(#[0-9a-fA-F]{3,8})"));
  auto match = rx.match(qss);
  if (!match.hasMatch()) return false;  // 无背景色定义，默认亮色

  QColor bg(match.captured(1).trimmed());
  if (!bg.isValid()) return false;

  // 计算相对亮度 (ITU-R BT.709)
  double luma = 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF();
  return luma < 0.4;
}

void ThemeManager::syncLegacyState() {
  core::common::setDarkTheme(is_dark_);
}

void ThemeManager::onConfigChanged(const QString& key) {
  if (key == QString::fromLatin1(CONFIG_APPEARANCE_THEME)) {
    // 从配置重新读取主题
    QString theme = ConfigManager::instance().get<QString>(
        CONFIG_APPEARANCE_THEME,
        QString::fromLatin1(CONFIG_APPEARANCE_DEFAULT_THEME));
    setTheme(theme);
  }
}

}  // namespace etest::app
