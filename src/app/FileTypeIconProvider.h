#ifndef ETEST_APP_FILE_TYPE_ICON_PROVIDER_H_
#define ETEST_APP_FILE_TYPE_ICON_PROVIDER_H_

#include <QFileIconProvider>
#include <QMap>
#include <QString>

class QIcon;

namespace etest::app {

class FileTypeIconProvider : public QFileIconProvider {
 public:
  FileTypeIconProvider();

  QIcon icon(IconType type) const override;
  QIcon icon(const QFileInfo& info) const override;

 private:
  void loadIcons();
  QIcon loadDualThemeIcon(const QString& baseName) const;

  QMap<QString, QIcon> extension_icons_;
  QIcon folder_icon_;
  QIcon generic_file_icon_;
};

}  // namespace etest::app

#endif  // ETEST_APP_FILE_TYPE_ICON_PROVIDER_H_
