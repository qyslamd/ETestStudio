#include "ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <nlohmann/json.hpp>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

namespace etest::core_ui {

using namespace etest::core::config;
using json = nlohmann::json;

static QColor parseColor(const json& j) {
  if (j.is_string()) {
    std::string s = j.get<std::string>();
    return QColor(QString::fromStdString(s));
  }
  return QColor();
}

// ── JSON 加载 ──

bool ThemeManager::loadPaletteFromJson(const QString& path,
                                       ThemePalette& out) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QByteArray data = f.readAll();
  f.close();

  try {
    json j = json::parse(data.toStdString());

    out.themeId = QString::fromStdString(j.value("themeId", std::string()));
    out.isDark = j.value("isDark", false);
    out.ribbonBaseTheme = j.value("ribbonBaseTheme", 5);

    auto& colors = j["colors"];
    if (!colors.is_null()) {
      auto setColor = [&](const char* key, QColor& target) {
        auto it = colors.find(key);
        if (it != colors.end()) target = parseColor(*it);
      };
      setColor("windowBackground", out.windowBackground);
      setColor("panelBackground", out.panelBackground);
      setColor("toolbarBackground", out.toolbarBackground);
      setColor("hoverBackground", out.hoverBackground);
      setColor("selectionBackground", out.selectionBackground);
      setColor("tabSelectedBackground", out.tabSelectedBackground);
      setColor("borderColor", out.borderColor);
      setColor("textColor", out.textColor);
      setColor("secondaryTextColor", out.secondaryTextColor);
      setColor("disabledTextColor", out.disabledTextColor);
      setColor("accentColor", out.accentColor);
      setColor("statusBarBackground", out.statusBarBackground);
      setColor("clockFaceBackground", out.clockFaceBackground);
      setColor("clockHandColor", out.clockHandColor);
      setColor("clockSecondaryColor", out.clockSecondaryColor);
      setColor("clockAccentColor", out.clockAccentColor);
    }

    auto& editor = j["editorColors"];
    if (editor.is_object()) {
      for (auto it = editor.begin(); it != editor.end(); ++it) {
        if (it.value().is_string()) {
          std::string val = it.value().get<std::string>();
          out.editorColors[QString::fromStdString(it.key())] =
              QString::fromStdString(val);
        }
      }
    }

    return true;
  } catch (const json::exception& e) {
    qWarning("Theme JSON parse error: %s", e.what());
    return false;
  }
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
  // 已删主题回退 default：在 loadQss/current_theme_ 赋值前归一化，
  // 避免 current_theme_ 残留无效 ID 导致 onThemeChanged 加载 ads_<id>.qss 失败
  if (!palettes_.contains(theme)) {
    theme = QStringLiteral("default");
  }

  loadQss(theme);
  current_theme_ = theme;
  auto it = palettes_.find(theme);
  if (it == palettes_.end()) {
    it = palettes_.find(QStringLiteral("default"));
  }
  palette_ = (it != palettes_.end()) ? &it.value() : nullptr;
  is_dark_ = palette_ ? palette_->isDark : true;
  applyEditorTheme();

  connect(&cfg, &ConfigManager::configChanged, this,
          &ThemeManager::onConfigChanged);
}

ThemeManager::~ThemeManager() = default;

void ThemeManager::registerBuiltinPalettes() {
  QDir dir(QStringLiteral(":/resources/themes"));
  if (dir.exists()) {
    const auto entries =
        dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    for (const auto& fi : entries) {
      ThemePalette p;
      if (loadPaletteFromJson(fi.absoluteFilePath(), p)) {
        palettes_[p.themeId] = p;
        QFile f(fi.absoluteFilePath());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
          try {
            json j = json::parse(f.readAll().toStdString());
            display_names_[p.themeId] =
                QString::fromStdString(j.value("displayName", std::string()));
          } catch (...) {}
          f.close();
        }
      }
    }
  }

  // 兜底：确保 default 永远存在
  if (!palettes_.contains(QStringLiteral("default"))) {
    ThemePalette fallback;
    fallback.themeId = QStringLiteral("default");
    fallback.isDark = false;
    fallback.ribbonBaseTheme = 2;
    fallback.windowBackground = QColor(0xF0, 0xF0, 0xF0);
    fallback.panelBackground = QColor(0xF0, 0xF0, 0xF0);
    fallback.toolbarBackground = QColor(0xF0, 0xF0, 0xF0);
    fallback.hoverBackground = QColor(0xE0, 0xE0, 0xE0);
    fallback.selectionBackground = QColor(0xCC, 0xE4, 0xF7);
    fallback.tabSelectedBackground = QColor(0xFF, 0xFF, 0xFF);
    fallback.borderColor = QColor(0xCC, 0xCC, 0xCC);
    fallback.textColor = QColor(0x33, 0x33, 0x33);
    fallback.secondaryTextColor = QColor(0x88, 0x88, 0x88);
    fallback.disabledTextColor = QColor(0xAA, 0xAA, 0xAA);
    fallback.accentColor = QColor("#007ACC");
    fallback.statusBarBackground = QColor(0xF0, 0xF0, 0xF0);
    fallback.clockFaceBackground = QColor(0xF6, 0xF6, 0xF6);
    fallback.clockHandColor = QColor(0x33, 0x33, 0x33);
    fallback.clockSecondaryColor = QColor(0x55, 0x55, 0x55);
    fallback.clockAccentColor = QColor(0xFF, 0x66, 0x00);
    palettes_[QStringLiteral("default")] = fallback;
    display_names_[QStringLiteral("default")] =
        QStringLiteral("默认主题");
  }
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

QString ThemeManager::ribbonQssPath() const {
  // Auto-derive: if ribbon_<themeId>.qss exists, return its path
  if (palette_) {
    QString path = QStringLiteral(":/resources/styles/ribbon_%1.qss")
                       .arg(palette_->themeId);
    if (QFileInfo::exists(path)) {
      return path;
    }
  }
  return QString();
}

QString ThemeManager::themeDisplayName(const QString& themeId) const {
  return display_names_.value(themeId, themeId);
}

// -- 语义色板（一行委托） --

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
QColor ThemeManager::tabSelectedBackground() const {
  return palette_ ? palette_->tabSelectedBackground : QColor();
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
  return display_names_.keys();
}

void ThemeManager::setTheme(const QString& themeId) {
  // 已删主题回退 default：归一化后再比较/保存，确保配置里不写入无效 ID
  QString resolved = themeId;
  if (!palettes_.contains(resolved)) {
    resolved = QStringLiteral("default");
  }
  if (resolved == current_theme_)
    return;

  current_theme_ = resolved;
  auto it = palettes_.find(resolved);
  if (it == palettes_.end()) {
    it = palettes_.find(QStringLiteral("default"));
  }
  palette_ = (it != palettes_.end()) ? &it.value() : nullptr;
  is_dark_ = palette_ ? palette_->isDark : true;

  loadQss(resolved);
  applyEditorTheme();

  ConfigManager::instance().set(QString::fromLatin1(CONFIG_APPEARANCE_THEME),
                                resolved);

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

  // 每主题 QADS 覆盖（存在时追加到全局，覆盖浮动窗口）
  QFile adsFile(QStringLiteral(":/resources/styles/ads_%1.qss").arg(themeId));
  if (adsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qss += QStringLiteral("\n") + QString::fromUtf8(adsFile.readAll());
    adsFile.close();
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
