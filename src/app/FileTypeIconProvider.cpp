#include "FileTypeIconProvider.h"

#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

#include "logger/Logger.h"

using namespace etest::core::logger;

namespace etest::app {

FileTypeIconProvider::FileTypeIconProvider() : QFileIconProvider() {
  loadIcons();
  LOG_INFO("UI",
           "FileTypeIconProvider loaded, {} extensions mapped, folder null={}",
           extension_icons_.size(), folder_icon_.isNull());
}

void FileTypeIconProvider::loadIcons() {
  folder_icon_ = loadDualThemeIcon("folder");
  generic_file_icon_ = loadDualThemeIcon("file_generic");

  QIcon cppIcon = loadDualThemeIcon("file_cpp");
  for (const auto& ext : {"cpp", "h", "c", "hpp", "cc", "cxx"}) {
    extension_icons_[ext] = cppIcon;
  }

  extension_icons_["lua"] = loadDualThemeIcon("file_lua");
  extension_icons_["json"] = loadDualThemeIcon("file_json");

  QIcon xmlIcon = loadDualThemeIcon("file_xml");
  for (const auto& ext : {"xml", "html", "htm", "svg"}) {
    extension_icons_[ext] = xmlIcon;
  }

  QIcon pyIcon = loadDualThemeIcon("file_python");
  for (const auto& ext : {"py", "pyw"}) {
    extension_icons_[ext] = pyIcon;
  }

  QIcon yamlIcon = loadDualThemeIcon("file_yaml");
  for (const auto& ext : {"yaml", "yml"}) {
    extension_icons_[ext] = yamlIcon;
  }

  extension_icons_["md"] = loadDualThemeIcon("file_markdown");
  extension_icons_["cmake"] = loadDualThemeIcon("file_cmake");
  extension_icons_["js"] = loadDualThemeIcon("file_js");
  extension_icons_["eproto"] = loadDualThemeIcon("file_eproto");
  extension_icons_["etopo"] = loadDualThemeIcon("file_etopo");
}

QIcon FileTypeIconProvider::loadDualThemeIcon(const QString& baseName) const {
  QString lightPath =
      QStringLiteral(":/resources/icons/svg/%1_light.svg").arg(baseName);
  QIcon icon(lightPath);
  if (icon.isNull()) {
    LOG_WARN("UI", "Failed to load icon: {}", lightPath.toStdString());
  }
  return icon;
}

QIcon FileTypeIconProvider::coloredFolderIcon(const QColor& accentColor) const {
  QString key = accentColor.name();
  auto it = colored_folder_cache_.constFind(key);
  if (it != colored_folder_cache_.constEnd())
    return it.value();

  // Render base folder icon to pixmap for QPainter manipulation
  QPixmap base = folder_icon_.pixmap(24, 24);
  if (base.isNull() || base.size().isEmpty()) {
    colored_folder_cache_[key] = folder_icon_;
    return folder_icon_;
  }

  QPixmap result(base.size());
  result.fill(Qt::transparent);
  QPainter p(&result);
  p.setRenderHint(QPainter::Antialiasing);
  p.drawPixmap(0, 0, base);

  // Draw a colored vertical stripe on the left edge as a directory indicator
  p.fillRect(2, 4, 3, base.height() - 8, accentColor);
  p.end();

  QIcon icon(result);
  colored_folder_cache_[key] = icon;
  return icon;
}

QIcon FileTypeIconProvider::icon(IconType type) const {
  if (type == Folder) {
    return folder_icon_;
  }
  return QFileIconProvider::icon(type);
}

QIcon FileTypeIconProvider::icon(const QFileInfo& info) const {
  if (info.isDir()) {
    // Return colored icon for known project directories
    static const QMap<QString, QColor> dirColors = {
        {QStringLiteral("backup"), QColor("#E67E22")},    // orange
        {QStringLiteral("cases"), QColor("#2ECC71")},     // green
        {QStringLiteral("config"), QColor("#3498DB")},     // blue
        {QStringLiteral("scripts"), QColor("#9B59B6")},    // purple
        {QStringLiteral("protocol"), QColor("#1ABC9C")},   // teal
        {QStringLiteral("topology"), QColor("#E74C3C")},   // red
        {QStringLiteral("reports"), QColor("#F39C12")},    // gold
    };
    auto it = dirColors.constFind(info.fileName());
    if (it != dirColors.constEnd()) {
      return coloredFolderIcon(it.value());
    }
    return folder_icon_;
  }

  QString suffix = info.suffix().toLower();
  auto it = extension_icons_.constFind(suffix);
  if (it != extension_icons_.constEnd()) {
    return it.value();
  }

  return generic_file_icon_;
}

}  // namespace etest::app
