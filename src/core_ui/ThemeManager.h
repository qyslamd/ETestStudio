#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include "ThemePalette.h"

namespace etest::core_ui {

class ThemeManager : public QObject {
  Q_OBJECT

 public:
  static ThemeManager& instance();
  ~ThemeManager() override;

  ThemeManager(const ThemeManager&) = delete;
  ThemeManager& operator=(const ThemeManager&) = delete;

  bool isDarkTheme() const;
  QString currentTheme() const;
  QStringList availableThemes() const;
  int ribbonBaseTheme() const;
  QString ribbonQssPath() const;
  QString themeDisplayName(const QString& themeId) const;

  void setTheme(const QString& themeId);

  // -- 语义色板 --
  QColor windowBackground() const;
  QColor panelBackground() const;
  QColor sceneBackground() const;
  QColor toolbarBackground() const;
  QColor hoverBackground() const;
  QColor selectionBackground() const;
  QColor tabSelectedBackground() const;
  QColor borderColor() const;
  QColor textColor() const;
  QColor secondaryTextColor() const;
  QColor disabledTextColor() const;
  QColor accentColor() const;
  QColor statusBarBackground() const;
  QColor clockFaceBackground() const;
  QColor clockHandColor() const;
  QColor clockSecondaryColor() const;
  QColor clockAccentColor() const;

 signals:
  void themeChanged(bool isDark);

 private:
  explicit ThemeManager(QObject* parent = nullptr);

  void registerBuiltinPalettes();
  bool loadPaletteFromJson(const QString& path, ThemePalette& out);
  void loadQss(const QString& themeId);
  bool detectDarkFromQss(const QString& qss) const;
  void applyEditorTheme();
  void onConfigChanged(const QString& key);

  QHash<QString, ThemePalette> palettes_;
  QHash<QString, QString> display_names_;
  const ThemePalette* palette_ = nullptr;
  QString current_theme_;
  bool is_dark_ = true;
};

}  // namespace etest::core_ui
