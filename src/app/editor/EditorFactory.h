#pragma once

#include <QString>
#include <QMap>
#include <functional>
#include "IEditor.h"

namespace etest::app {

using EditorFactory = std::function<IEditor*(const QString& id, QWidget* parent)>;

class EditorFactoryRegistry {
 public:
  static void registerFactory(const QString& editorType, EditorFactory factory);
  static void registerExtension(const QString& suffix, const QString& editorType);
  static IEditor* create(const QString& editorType, const QString& id,
                         QWidget* parent = nullptr);
  static QString typeForExtension(const QString& suffix);

 private:
  static QMap<QString, EditorFactory>& factories();
  static QMap<QString, QString>& extensionMap();
};

}  // namespace etest::app
