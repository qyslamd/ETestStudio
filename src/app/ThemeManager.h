#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace etest::app {

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

 signals:
  void themeChanged(bool isDark);

 private:
  explicit ThemeManager(QObject* parent = nullptr);

  void loadQss(const QString& themeId);
  bool detectDarkFromQss(const QString& qss) const;
  void syncLegacyState();
  void onConfigChanged(const QString& key);

  QString current_theme_;
  bool is_dark_ = true;
};

}  // namespace etest::app
