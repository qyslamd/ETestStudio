#include "EditorManager.h"
#include "DockAreaWidget.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QApplication>
#include <QMenu>
#include <QClipboard>
#include <QDesktopServices>

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

  // 设置右键菜单
  dock->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(dock, &ads::CDockWidget::customContextMenuRequested, this, &EditorManager::onDockCustomContextMenuRequested);

  // 脏标记：修改时在标题前加 *
  connect(editor, &EditorWidget::modificationChanged, this,
          [this, editor, dock](bool modified) {
            QString title = editor->fileName();
            if (modified) {
              title.prepend("* ");
            }
            dock->setWindowTitle(title);
            emit unsavedChangesChanged();
            emit modificationChanged(modified);  // 新增：转发信号
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

  // 主动更新当前文件路径，不需要等待dock激活事件（防止刚打开文件就按Ctrl+S时空指针）
  current_file_path_ = filePath;
  emit currentEditorChanged(editor);

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
    // 获取有效的父窗口
    QWidget* parentWidget = dock_manager_->topLevelWidget();
    if (!parentWidget && qApp) {
      parentWidget = qApp->activeWindow();
    }
    int ret = QMessageBox::question(
        parentWidget, QStringLiteral("保存更改"),
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
    // 关闭的是当前文件，尝试切换到其他打开的文件
    if (!editors_.isEmpty()) {
      // 切换到第一个编辑器
      current_file_path_ = editors_.firstKey();
      // 激活对应的dock
      auto* dock = dock_widgets_[current_file_path_];
      if (dock) {
        dock->raise();
      }
      emit currentEditorChanged(editors_.first());
    } else {
      // 没有其他打开的文件
      current_file_path_.clear();
      emit currentEditorChanged(nullptr);
    }
  }

  LOG_INFO("EDITOR", "关闭文件：{}", filePath.toStdString());
  emit fileClosed(filePath);
  emit unsavedChangesChanged();
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

bool EditorManager::saveAllFiles() {
  for (auto* editor : editors_) {
    if (editor->isModified()) {
      if (!editor->saveFile()) {
        LOG_ERROR("EDITOR", "保存文件失败：{}", editor->filePath().toStdString());
        return false;
      }
    }
  }
  return true;
}

bool EditorManager::saveModifiedFiles(const QStringList& filePaths) {
  if (filePaths.isEmpty()) {
    return saveAllFiles();
  }

  // 如果只有一个路径且是目录，保存该目录下的所有未保存文件
  if (filePaths.size() == 1) {
    QString path = filePaths.first();
    QFileInfo fi(path);
    if (fi.isDir()) {
      return saveModifiedFilesInDirectory(path);
    }
  }

  // 否则保存指定的文件列表
  for (const QString& fp : filePaths) {
    auto it = editors_.find(fp);
    if (it != editors_.end() && it.value()->isModified()) {
      if (!it.value()->saveFile()) {
        LOG_ERROR("EDITOR", "保存文件失败：{}", fp.toStdString());
        return false;
      }
    }
  }
  return true;
}

bool EditorManager::saveModifiedFilesInDirectory(const QString& dirPath) {
  if (dirPath.isEmpty()) {
    return saveAllFiles();
  }

  QDir dir(dirPath);
  if (!dir.exists()) {
    return false;
  }

  for (auto it = editors_.constBegin(); it != editors_.constEnd(); ++it) {
    // 使用relativeFilePath判断：如果返回的路径不以".."开头，说明是子目录
    QString relativePath = dir.relativeFilePath(it.key());
    if (!relativePath.startsWith("..") && it.value()->isModified()) {
      if (!it.value()->saveFile()) {
        LOG_ERROR("EDITOR", "保存文件失败：{}", it.key().toStdString());
        return false;
      }
    }
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

bool EditorManager::hasUnsavedChangesInDirectory(const QString& dirPath) const {
  if (dirPath.isEmpty()) {
    return hasUnsavedChanges();
  }

  QDir dir(dirPath);
  if (!dir.exists()) {
    return false;
  }

  for (auto it = editors_.constBegin(); it != editors_.constEnd(); ++it) {
    // 使用relativeFilePath判断：如果返回的路径不以".."开头，说明是子目录
    QString relativePath = dir.relativeFilePath(it.key());
    if (!relativePath.startsWith("..") && it.value()->isModified()) {
      return true;
    }
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

void EditorManager::onFileDeleted(const QString& filePath) {
  auto it = editors_.find(filePath);
  if (it == editors_.end()) {
    return; // 文件没有打开，不需要处理
  }

  auto* editor = it.value();
  if (editor->isModified()) {
    // 文件已修改，提示用户
    // 获取有效的父窗口
    QWidget* parentWidget = dock_manager_->topLevelWidget();
    if (!parentWidget && qApp) {
      parentWidget = qApp->activeWindow();
    }
    int ret = QMessageBox::question(
        parentWidget, QStringLiteral("文件已删除"),
        QStringLiteral("文件 \"%1\" 已被删除，是否保存更改到其他位置？")
            .arg(editor->fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel) {
      // 用户取消，不关闭编辑器
      return;
    }

    if (ret == QMessageBox::Save) {
      // 用户选择另存为
      if (!editor->saveFileAs(QString())) {
        // 保存失败或用户取消，不关闭编辑器
        return;
      }
      // 保存成功，编辑器已标记为未修改，继续关闭
    } else if (ret == QMessageBox::Discard) {
      // 用户选择丢弃更改，将编辑器标记为未修改，避免closeFile重复提示
      editor->editor()->setModified(false);
    }
  }

  // 关闭编辑器，此时编辑器要么未修改，要么已保存
  closeFile(filePath);
}

void EditorManager::onFileRenamed(const QString& oldPath, const QString& newPath) {
  auto it = editors_.find(oldPath);
  if (it == editors_.end()) {
    return; // 文件没有打开，不需要处理
  }

  auto* editor = it.value();
  auto* dock = dock_widgets_[oldPath];

  // 更新编辑器的文件路径
  editor->setFilePath(newPath);

  // 更新maps中的key
  editors_.remove(oldPath);
  editors_[newPath] = editor;
  dock_widgets_.remove(oldPath);
  dock_widgets_[newPath] = dock;

  // 更新当前文件路径如果需要
  if (current_file_path_ == oldPath) {
    current_file_path_ = newPath;
  }

  // 更新dock标题
  updateDockTitle(editor, dock);

  LOG_INFO("EDITOR", "文件已重命名：{} -> {}", oldPath.toStdString(), newPath.toStdString());
}

bool EditorManager::closeFilesInDirectory(const QString& dirPath) {
  if (dirPath.isEmpty()) {
    return closeAllFiles();
  }

  QDir dir(dirPath);
  if (!dir.exists()) {
    return true;
  }

  // 收集所有属于该目录的文件
  QStringList filesToClose;
  for (const QString& filePath : editors_.keys()) {
    // 使用relativeFilePath判断：如果返回的路径不以".."开头，说明是子目录
    QString relativePath = dir.relativeFilePath(filePath);
    if (!relativePath.startsWith("..")) {
      filesToClose.append(filePath);
    }
  }

  // 逐个关闭
  for (const QString& fp : filesToClose) {
    if (!closeFile(fp)) {
      return false; // 用户取消关闭
    }
  }

  return true;
}

void EditorManager::onDockCustomContextMenuRequested(const QPoint& pos) {
  auto* dock = qobject_cast<ads::CDockWidget*>(sender());
  if (!dock) return;

  // 找到对应的文件路径
  QString filePath;
  for (auto it = dock_widgets_.constBegin(); it != dock_widgets_.constEnd(); ++it) {
    if (it.value() == dock) {
      filePath = it.key();
      break;
    }
  }
  if (filePath.isEmpty()) return;

  QMenu menu(dock);

  // 关闭当前
  QAction* closeAction = menu.addAction(QStringLiteral("关闭"));
  connect(closeAction, &QAction::triggered, this, [this, filePath]() {
    closeFile(filePath);
  });

  // 关闭其他
  QAction* closeOthersAction = menu.addAction(QStringLiteral("关闭其他"));
  connect(closeOthersAction, &QAction::triggered, this, [this, filePath]() {
    QStringList allFiles = dock_widgets_.keys();
    for (const QString& fp : allFiles) {
      if (fp != filePath) {
        closeFile(fp);
      }
    }
  });

  // 关闭右侧
  QAction* closeRightAction = menu.addAction(QStringLiteral("关闭右侧所有"));
  connect(closeRightAction, &QAction::triggered, this, [this, dock, filePath]() {
    // 获取当前dock所在的区域
    auto* area = dock->dockAreaWidget();
    if (!area) return;

    // 获取区域内的所有dock
    auto docks = area->dockWidgets();
    int currentIndex = docks.indexOf(dock);
    if (currentIndex == -1) return;

    // 关闭右侧的dock
    for (int i = currentIndex + 1; i < docks.size(); ++i) {
      // 找到对应的文件路径
      QString fp;
      for (auto it = dock_widgets_.constBegin(); it != dock_widgets_.constEnd(); ++it) {
        if (it.value() == docks[i]) {
          fp = it.key();
          break;
        }
      }
      if (!fp.isEmpty()) {
        closeFile(fp);
      }
    }
  });

  menu.addSeparator();

  // 复制文件路径
  QAction* copyPathAction = menu.addAction(QStringLiteral("复制文件路径"));
  connect(copyPathAction, &QAction::triggered, this, [filePath]() {
    QApplication::clipboard()->setText(filePath);
  });

  // 打开所在文件夹
  QAction* openFolderAction = menu.addAction(QStringLiteral("打开所在文件夹"));
  connect(openFolderAction, &QAction::triggered, this, [filePath]() {
    QFileInfo fi(filePath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
  });

  // 显示菜单
  menu.exec(dock->mapToGlobal(pos));
}

}  // namespace etest::app
