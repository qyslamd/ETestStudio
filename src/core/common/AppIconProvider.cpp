#include "AppIconProvider.h"

#include <QApplication>
#include <QFileInfo>

#include "ThemeManager.h"

namespace etest::app {

AppIconProvider& AppIconProvider::instance() {
  static AppIconProvider inst;
  return inst;
}

AppIconProvider::AppIconProvider(QObject* parent) : QObject(parent), cache_(200) {
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
          this, [this](bool) { clearCache(); });
}

QIcon AppIconProvider::icon(const QString& name) const {
  if (auto* cached = cache_.object(name)) {
    return *cached;
  }

  QString path = resolvePath(name);
  QIcon result(path);

  cache_.insert(name, new QIcon(result));
  return result;
}

void AppIconProvider::clearCache() {
  cache_.clear();
}

QString AppIconProvider::resolvePath(const QString& baseName) const {
  bool dark = ThemeManager::instance().isDarkTheme();

  QString candidate = dark
      ? QStringLiteral(":/resources/icons/svg/%1_light.svg").arg(baseName)
      : QStringLiteral(":/resources/icons/svg/%1_dark.svg").arg(baseName);

  if (QFileInfo::exists(candidate)) {
    return candidate;
  }

  return QStringLiteral(":/resources/icons/svg/%1.svg").arg(baseName);
}

}  // namespace etest::app
