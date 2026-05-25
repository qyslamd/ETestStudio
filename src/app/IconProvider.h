#pragma once

#include <QCache>
#include <QIcon>
#include <QObject>
#include <QString>

namespace etest::app {

class IconProvider : public QObject {
  Q_OBJECT
 public:
  static IconProvider& instance();

  IconProvider(const IconProvider&) = delete;
  IconProvider& operator=(const IconProvider&) = delete;

  QIcon icon(const QString& name) const;
  void clearCache();

 private:
  explicit IconProvider(QObject* parent = nullptr);
  ~IconProvider() override = default;

  QString resolvePath(const QString& baseName) const;

  mutable QCache<QString, QIcon> cache_;
};

}  // namespace etest::app
