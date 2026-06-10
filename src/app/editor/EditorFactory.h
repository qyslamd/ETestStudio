#pragma once

#include <QMap>
#include <QString>
#include <functional>

#include "IEditor.h"

namespace ads {
class CDockWidget;
}

namespace etest::app {

class EditorManager;

using EditorFactory =
    std::function<IEditor*(const QString& id, QWidget* parent)>;

using EditorSignalBinder =
    std::function<void(IEditor* editor, ads::CDockWidget* dock,
                       EditorManager* manager)>;

class EditorFactoryRegistry {
 public:
  static void registerFactory(const QString& editorType, EditorFactory factory);
  static void registerFactory(const QString& editorType, EditorFactory factory,
                               EditorSignalBinder binder);
  static void registerExtension(const QString& suffix,
                                 const QString& editorType);
  static IEditor* create(const QString& editorType,
                          const QString& id,
                          QWidget* parent = nullptr);
  static QString typeForExtension(const QString& suffix);
  static EditorSignalBinder binderForType(const QString& editorType);

 private:
  static QMap<QString, EditorFactory>& factories();
  static QMap<QString, EditorSignalBinder>& binders();
  static QMap<QString, QString>& extensionMap();
};

}  // namespace etest::app
