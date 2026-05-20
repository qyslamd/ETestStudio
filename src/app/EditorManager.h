#ifndef ETEST_APP_EDITOR_MANAGER_H_
#define ETEST_APP_EDITOR_MANAGER_H_

#include <QMap>
#include <QObject>
#include <QString>

#include "DockManager.h"
#include "api/IEditor.h"

namespace etest::app {

class EditorManager : public QObject {
  Q_OBJECT

 public:
  explicit EditorManager(ads::CDockManager* dockManager,
                         QObject* parent = nullptr);

  static void registerEditorTypes();

  void openFile(const QString& filePath);
  void openFileAtLine(const QString& filePath, int line);
  bool closeFile(const QString& editorId);
  bool closeAllFiles();
  bool saveAllFiles();
  bool saveModifiedFiles(const QStringList& filePaths = QStringList());

  void onFileDeleted(const QString& filePath);
  void onFileRenamed(const QString& oldPath, const QString& newPath);

  bool closeFilesInDirectory(const QString& dirPath);

  IEditor* editorById(const QString& id) const;
  bool isOpen(const QString& editorId) const;
  bool hasUnsavedChanges() const;
  bool hasUnsavedChangesInDirectory(const QString& dirPath) const;
  bool saveModifiedFilesInDirectory(const QString& dirPath);

  QStringList openFiles() const;
  IEditor* currentEditor() const;
  QString currentFilePath() const;

  void createEditor(const QString& editorType,
                    const QString& id,
                    const QString& title);
  void updateEditorId(IEditor* editor, const QString& newId);

 signals:
  void fileOpened(const QString& filePath);
  void fileClosed(const QString& filePath);
  void currentEditorChanged(IEditor* editor);
  void unsavedChangesChanged();
  void modificationChanged(bool modified);

 private slots:
  void onDockWidgetActivated(ads::CDockWidget* dock);
  void updateDockTitle(IEditor* editor, ads::CDockWidget* dock);
  void onDockCustomContextMenuRequested(const QPoint& pos);

 private:
  ads::CDockManager* dock_manager_;
  QMap<QString, ads::CDockWidget*> dock_widgets_;
  QMap<QString, IEditor*> editors_;
  QString current_file_path_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EDITOR_MANAGER_H_
