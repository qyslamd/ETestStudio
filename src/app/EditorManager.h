#ifndef ETEST_APP_EDITOR_MANAGER_H_
#define ETEST_APP_EDITOR_MANAGER_H_

#include <QMap>
#include <QObject>
#include <QString>

#include "DockManager.h"
#include "api/IEditor.h"

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace icd {
class Repository;
}  // namespace icd

namespace etest::app {

class EditorManager : public QObject {
  Q_OBJECT

 public:
  explicit EditorManager(ads::CDockManager* dockManager,
                         QObject* parent = nullptr);

  static void registerEditorTypes();

  void openFile(const QString& filePath,
                const QString& forcedEditorType = QString());
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
  QList<IEditor*> allEditors() const;
  IEditor* currentEditor() const;
  QString currentFilePath() const;

  void createEditor(const QString& editorType,
                    const QString& id,
                    const QString& title);
  void updateEditorId(IEditor* editor, const QString& newId);

  void bindDockTabContextMenu(ads::CDockWidget* dock);
  void showDockContextMenu(ads::CDockWidget* dock, const QPoint& globalPos);

  // M5: 注入 ICD 信号注册表和 Repository（供 test_program 编辑器使用）
  void setSignalRegistry(etest::core::SignalRegistry* registry) { registry_ = registry; }
  void setIcdRepository(icd::Repository* repo) { repository_ = repo; }
  etest::core::SignalRegistry* signalRegistry() const { return registry_; }
  icd::Repository* icdRepository() const { return repository_; }

 signals:
  void fileOpened(const QString& filePath);
  void fileClosed(const QString& filePath);
  void currentEditorChanged(IEditor* editor);
  void unsavedChangesChanged();
  void modificationChanged(bool modified);

 private slots:
  void onDockWidgetActivated(ads::CDockWidget* dock);
  void updateDockTitle(IEditor* editor, ads::CDockWidget* dock);

 private:
  ads::CDockManager* dock_manager_;
  QMap<QString, ads::CDockWidget*> dock_widgets_;
  QMap<QString, IEditor*> editors_;
  QString current_file_path_;

  // M5: ICD 上下文（供编辑器注入）
  etest::core::SignalRegistry* registry_ = nullptr;
  icd::Repository* repository_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_EDITOR_MANAGER_H_
