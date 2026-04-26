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
                         QObject* parent = nullptr);

  void openFile(const QString& filePath);
  bool closeFile(const QString& filePath);
  bool closeAllFiles();
  bool saveAllFiles();
  bool saveModifiedFiles(const QStringList& filePaths = QStringList());

  // 文件变更同步
  void onFileDeleted(const QString& filePath);
  void onFileRenamed(const QString& oldPath, const QString& newPath);

  // 关闭指定目录下的所有文件（用于项目关闭时保留项目外的文件）
  bool closeFilesInDirectory(const QString& dirPath);

  EditorWidget* editorForFile(const QString& filePath) const;
  bool isOpen(const QString& filePath) const;
  bool hasUnsavedChanges() const;
  bool hasUnsavedChangesInDirectory(const QString& dirPath) const;
  bool saveModifiedFilesInDirectory(const QString& dirPath);

  EditorWidget* currentEditor() const;
  QString currentFilePath() const;

 Q_SIGNALS:
  void fileOpened(const QString& filePath);
  void fileClosed(const QString& filePath);
  void currentEditorChanged(EditorWidget* editor);
  void unsavedChangesChanged();
  void modificationChanged(bool modified);  // 新增：转发单个编辑器的脏标记变化

 private slots:
  void onDockWidgetActivated(ads::CDockWidget* dock);
  void updateDockTitle(EditorWidget* editor, ads::CDockWidget* dock);
  void onDockCustomContextMenuRequested(const QPoint& pos);

 private:

  ads::CDockManager* dock_manager_;
  QMap<QString, ads::CDockWidget*> dock_widgets_;
  QMap<QString, EditorWidget*> editors_;
  QString current_file_path_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EDITOR_MANAGER_H_
