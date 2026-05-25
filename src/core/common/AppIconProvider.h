#pragma once

#include <QCache>
#include <QIcon>
#include <QObject>
#include <QString>

namespace etest::app {

class AppIconProvider : public QObject {
  Q_OBJECT
 public:
  static AppIconProvider& instance();

  AppIconProvider(const AppIconProvider&) = delete;
  AppIconProvider& operator=(const AppIconProvider&) = delete;

  QIcon icon(const QString& name) const;
  void clearCache();

 private:
  explicit AppIconProvider(QObject* parent = nullptr);
  ~AppIconProvider() override = default;

  QString resolvePath(const QString& baseName) const;

  mutable QCache<QString, QIcon> cache_;
};

}  // namespace etest::app
