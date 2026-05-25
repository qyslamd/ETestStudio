#include "IconProvider.h"

#include <QApplication>
#include <QFileInfo>

#include "ThemeManager.h"

namespace etest::app {

IconProvider& IconProvider::instance() {
  static IconProvider inst;
  return inst;
}

IconProvider::IconProvider(QObject* parent) : QObject(parent), cache_(200) {
  // 主题切换时清空缓存
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
          this, [this](bool) { clearCache(); });
}

QIcon IconProvider::icon(const QString& name) const {
  // 缓存命中直接返回
  if (auto* cached = cache_.object(name)) {
    return *cached;
  }

  // 解析路径并加载
  QString path = resolvePath(name);
  QIcon result(path);

  // 即使空图标也缓存（防止反复命中不存在的文件）
  cache_.insert(name, new QIcon(result));
  return result;
}

void IconProvider::clearCache() {
  cache_.clear();
}

QString IconProvider::resolvePath(const QString& baseName) const {
  bool dark = ThemeManager::instance().isDarkTheme();

  // 暗色背景 → _light 变体（浅色图标在暗色上清晰）
  // 亮色背景 → _dark 变体（深色图标在亮色上清晰）
  QString candidate = dark
      ? QStringLiteral(":/resources/icons/svg/%1_light.svg").arg(baseName)
      : QStringLiteral(":/resources/icons/svg/%1_dark.svg").arg(baseName);

  // 双状态图标存在则返回
  if (QFileInfo::exists(candidate)) {
    return candidate;
  }

  // 回退到单状态图标
  return QStringLiteral(":/resources/icons/svg/%1.svg").arg(baseName);
}

}  // namespace etest::app
