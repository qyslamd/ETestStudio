#include "EditorFactory.h"

namespace etest::app {

QMap<QString, EditorFactory>& EditorFactoryRegistry::factories() {
  static QMap<QString, EditorFactory> instance;
  return instance;
}

QMap<QString, QString>& EditorFactoryRegistry::extensionMap() {
  static QMap<QString, QString> instance;
  return instance;
}

void EditorFactoryRegistry::registerFactory(const QString& editorType,
                                            EditorFactory factory) {
  factories()[editorType] = std::move(factory);
}

void EditorFactoryRegistry::registerExtension(const QString& suffix,
                                              const QString& editorType) {
  extensionMap()[suffix.toLower()] = editorType;
}

IEditor* EditorFactoryRegistry::create(const QString& editorType,
                                       const QString& id,
                                       QWidget* parent) {
  auto& f = factories();
  auto it = f.find(editorType);
  if (it != f.end()) {
    return it.value()(id, parent);
  }
  return nullptr;
}

QString EditorFactoryRegistry::typeForExtension(const QString& suffix) {
  auto& m = extensionMap();
  auto it = m.find(suffix.toLower());
  if (it != m.end()) {
    return it.value();
  }
  return {};
}

}  // namespace etest::app
