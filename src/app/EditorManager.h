#ifndef ETEST_APP_EDITOR_MANAGER_H_
#define ETEST_APP_EDITOR_MANAGER_H_

#include <QMap>
#include <QObject>
#include <QString>

#include "DockManager.h"

namespace etest::app {

class EditorWidget;
class EditorManager : public QObject {
  Q_OBJECT

 public:
  explicit EditorManager(ads::CDockManager* dockManager,
                         ads::CDockAreaWidget* centralArea,
                         QObject* parent = nullptr);

  void openFile(const QString& filePath);
  bool closeFile(const QString& filePath);
  bool closeAllFiles();

  EditorWidget* editorForFile(const QString& filePath) const;
  bool isOpen(const QString& filePath) const;
  bool hasUnsavedChanges() const;

  EditorWidget* currentEditor() const;
  QString currentFilePath() const;

 Q_SIGNALS:
  void fileOpened(const QString& filePath);
  void fileClosed(const QString& filePath);
  void currentEditorChanged(EditorWidget* editor);

 private:
  void onDockWidgetActivated(ads::CDockWidget* dock);
  void updateDockTitle(EditorWidget* editor, ads::CDockWidget* dock);

  ads::CDockManager* dock_manager_;
  ads::CDockAreaWidget* central_area_;
  QMap<QString, ads::CDockWidget*> dock_widgets_;
  QMap<QString, EditorWidget*> editors_;
  QString current_file_path_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EDITOR_MANAGER_H_
