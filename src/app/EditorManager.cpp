#include "EditorManager.h"

#include <QFileInfo>
#include <QMessageBox>

#include "EditorWidget.h"
#include "logger/Logger.h"

using namespace etest::core::logger;

namespace etest::app {

EditorManager::EditorManager(ads::CDockManager* dockManager,
                             QObject* parent)
    : QObject(parent), dock_manager_(dockManager) {
  connect(dock_manager_, &ads::CDockManager::focusedDockWidgetChanged, this,
          [this](ads::CDockWidget* old, ads::CDockWidget* now) {
            onDockWidgetActivated(now);
          });
}

void EditorManager::openFile(const QString& filePath) {
  if (editors_.contains(filePath)) {
    auto* dock = dock_widgets_[filePath];
    dock->raise();
    return;
  }

  QFileInfo fi(filePath);
  if (!fi.isFile() || !fi.isReadable()) {
    LOG_WARN("EDITOR", "无法打开文件：{}", filePath.toStdString());
    return;
  }

  auto* editor = new EditorWidget(filePath, nullptr);
  auto* dock = new ads::CDockWidget(fi.fileName());
  dock->setWidget(editor);
  dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, true);
  dock->setFeature(ads::CDockWidget::CustomCloseHandling, true);

  // 脏标记：修改时在标题前加 *
  connect(editor, &EditorWidget::modificationChanged, this,
          [this, editor, dock](bool modified) {
            QString title = editor->fileName();
            if (modified) {
              title.prepend("* ");
            }
            dock->setWindowTitle(title);
          });

  // 标签关闭按钮：走closeFile流程（脏检查）
  connect(dock, &ads::CDockWidget::closeRequested, this,
          [this, filePath]() { closeFile(filePath); });

  // 如果已有编辑器dock，tab到同一区域
  if (dock_widgets_.isEmpty()) {
    // 动态获取中央区域，避免指针悬空
    ads::CDockWidget* centralDock = dock_manager_->centralWidget();
    if (centralDock) {
        ads::CDockAreaWidget* centralArea = centralDock->dockAreaWidget();
        if (centralArea) {
            dock_manager_->addDockWidgetTabToArea(dock, centralArea);
        } else {
            dock_manager_->addDockWidget(ads::CenterDockWidgetArea, dock);
        }
    } else {
        dock_manager_->addDockWidget(ads::CenterDockWidgetArea, dock);
    }
  } else {
    auto* existingDock = *dock_widgets_.constBegin();
    auto* area = existingDock->dockAreaWidget();
    if (area) {
      dock_manager_->addDockWidgetTabToArea(dock, area);
    } else {
      dock_manager_->addDockWidget(ads::CenterDockWidgetArea, dock);
    }
  }

  dock_widgets_[filePath] = dock;
  editors_[filePath] = editor;

  dock->raise();

  LOG_INFO("EDITOR", "打开文件：{}", filePath.toStdString());
  emit fileOpened(filePath);
}

bool EditorManager::closeFile(const QString& filePath) {
  auto it = editors_.find(filePath);
  if (it == editors_.end())
    return false;

  auto* editor = it.value();
  if (editor->isModified()) {
    int ret = QMessageBox::question(
        nullptr, QStringLiteral("保存更改"),
        QStringLiteral("文件 \"%1\" 已修改，是否保存？")
            .arg(editor->fileName()),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel)
      return false;
    if (ret == QMessageBox::Yes) {
      if (!editor->saveFile())
        return false;
    }
  }

  auto* dock = dock_widgets_.take(filePath);
  editors_.erase(it);

  if (dock) {
    dock->closeDockWidget();
  }

  if (current_file_path_ == filePath) {
    current_file_path_.clear();
    emit currentEditorChanged(nullptr);
  }

  LOG_INFO("EDITOR", "关闭文件：{}", filePath.toStdString());
  emit fileClosed(filePath);
  return true;
}

bool EditorManager::closeAllFiles() {
  QStringList files = editors_.keys();
  for (const QString& fp : files) {
    if (!closeFile(fp))
      return false;
  }
  return true;
}

EditorWidget* EditorManager::editorForFile(const QString& filePath) const {
  return editors_.value(filePath, nullptr);
}

bool EditorManager::isOpen(const QString& filePath) const {
  return editors_.contains(filePath);
}

bool EditorManager::hasUnsavedChanges() const {
  for (auto* editor : editors_) {
    if (editor->isModified())
      return true;
  }
  return false;
}

EditorWidget* EditorManager::currentEditor() const {
  if (current_file_path_.isEmpty())
    return nullptr;
  return editors_.value(current_file_path_, nullptr);
}

QString EditorManager::currentFilePath() const {
  return current_file_path_;
}

void EditorManager::onDockWidgetActivated(ads::CDockWidget* dock) {
  if (!dock)
    return;

  // 查找dock对应的文件路径
  QString filePath;
  for (auto it = dock_widgets_.constBegin(); it != dock_widgets_.constEnd();
       ++it) {
    if (it.value() == dock) {
      filePath = it.key();
      break;
    }
  }

  if (filePath.isEmpty() || filePath == current_file_path_)
    return;

  current_file_path_ = filePath;
  auto* editor = editors_.value(filePath, nullptr);
  emit currentEditorChanged(editor);
}

void EditorManager::updateDockTitle(EditorWidget* editor,
                                    ads::CDockWidget* dock) {
  QString title = editor->fileName();
  if (editor->isModified()) {
    title.prepend("* ");
  }
  dock->setWindowTitle(title);
}

}  // namespace etest::app
