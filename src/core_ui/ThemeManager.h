#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>

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

  void setTheme(const QString& themeId);

  // -- 语义色板 --
  QColor windowBackground() const;
  QColor panelBackground() const;
  QColor toolbarBackground() const;
  QColor hoverBackground() const;
  QColor selectionBackground() const;
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

  void loadQss(const QString& themeId);
  bool detectDarkFromQss(const QString& qss) const;
  void applyEditorTheme();
  void onConfigChanged(const QString& key);

  QString current_theme_;
  bool is_dark_ = true;
};

}  // namespace etest::core_ui
