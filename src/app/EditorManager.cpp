#include "EditorManager.h"
#include "DockAreaWidget.h"
#include "DockWidgetTab.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMenu>
#include <QMessageBox>

#include "TextEditorWidget.h"
#include "editor/EditorFactory.h"
#include "logger/Logger.h"
#include "protocal/ProtocalEditorWidget.h"
#include "topology/TopologyDocument.h"
#include "topology/TopologyEditorWidget.h"
#include "topology/TopologyJsonSerializer.h"

#include "project/ProjectManager.h"

using namespace etest::core::logger;

namespace etest::app {

EditorManager::EditorManager(ads::CDockManager* dockManager, QObject* parent)
    : QObject(parent), dock_manager_(dockManager) {
  // QADS的focusedDockWidgetChanged在某些场景下不会触发
  // （如QGraphicsView捕获焦点时dock收不到focus event），
  // 因此额外使用QApplication::focusChanged做全局焦点追踪。
  connect(dock_manager_, &ads::CDockManager::focusedDockWidgetChanged, this,
          [this](ads::CDockWidget* /*old*/, ads::CDockWidget* now) {
            onDockWidgetActivated(now);
          });

  connect(qApp, &QApplication::focusChanged, this,
          [this](QWidget* /*old*/, QWidget* now) {
            if (!now)
              return;
            // 沿parent链向上查找所属的CDockWidget
            QWidget* w = now;
            while (w) {
              if (auto* dock = qobject_cast<ads::CDockWidget*>(w)) {
                onDockWidgetActivated(dock);
                return;
              }
              w = w->parentWidget();
            }
          });
}

void EditorManager::registerEditorTypes() {
  // 注册编辑器工厂
  EditorFactoryRegistry::registerExtension("cpp", "text");
  EditorFactoryRegistry::registerExtension("h", "text");
  EditorFactoryRegistry::registerExtension("hpp", "text");
  EditorFactoryRegistry::registerExtension("c", "text");
  EditorFactoryRegistry::registerExtension("cc", "text");
  EditorFactoryRegistry::registerExtension("cxx", "text");
  EditorFactoryRegistry::registerExtension("py", "text");
  EditorFactoryRegistry::registerExtension("lua", "text");
  EditorFactoryRegistry::registerExtension("json", "text");
  EditorFactoryRegistry::registerExtension("xml", "text");
  EditorFactoryRegistry::registerExtension("html", "text");
  EditorFactoryRegistry::registerExtension("yaml", "text");
  EditorFactoryRegistry::registerExtension("yml", "text");
  EditorFactoryRegistry::registerExtension("md", "text");
  EditorFactoryRegistry::registerExtension("js", "text");
  EditorFactoryRegistry::registerExtension("cmake", "text");
  EditorFactoryRegistry::registerExtension("txt", "text");
  EditorFactoryRegistry::registerExtension("etopo", "topology");
  EditorFactoryRegistry::registerExtension("eproto", "protocal");

  EditorFactoryRegistry::registerFactory(
      "text", [](const QString& path, QWidget* parent) {
        return new TextEditorWidget(path, parent);
      });

  EditorFactoryRegistry::registerFactory(
      "topology", [](const QString& id, QWidget* parent) {
        auto* editor = new etest::topology::TopologyEditorWidget(parent);
        if (!id.startsWith("editor://") && QFileInfo::exists(id)) {
          QFile file(id);
          if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll(), &err);
            file.close();
            if (err.error == QJsonParseError::NoError) {
              etest::topology::TopologyJsonSerializer::deserialize(
                  jdoc.object(), editor->document());
              editor->document()->undoStack()->clear();
              editor->reloadScene();
              editor->setEditorId(id);
            }
          }
        }
        return editor;
      });

  EditorFactoryRegistry::registerFactory(
      "protocal", [](const QString& id, QWidget* parent) {
        auto* editor = new etest::protocal::ProtocalEditorWidget(parent);
        if (!id.startsWith("editor://") && QFileInfo::exists(id)) {
          editor->setEditorId(id);
        }
        return editor;
      });
}

void EditorManager::openFile(const QString& filePath) {
  QString editorKey = filePath;
  if (editors_.contains(editorKey)) {
    auto* dock = dock_widgets_[editorKey];
    dock->raise();
    return;
  }

  QFileInfo fi(filePath);
  if (!fi.isFile() || !fi.isReadable()) {
    LOG_WARN("EDITOR", "无法打开文件：{}", filePath.toStdString());
    return;
  }

  QString suffix = fi.suffix().toLower();
  QString editorType = EditorFactoryRegistry::typeForExtension(suffix);
  if (editorType.isEmpty()) {
    editorType = QStringLiteral("text");
  }

  IEditor* editor =
      EditorFactoryRegistry::create(editorType, filePath, nullptr);
  if (!editor) {
    LOG_WARN("EDITOR", "无法创建编辑器：{}", filePath.toStdString());
    return;
  }

  auto* dock = new ads::CDockWidget(fi.fileName());
  dock->setWidget(editor->widget());
  dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, true);
  dock->setFeature(ads::CDockWidget::CustomCloseHandling, true);
  dock->tabWidget()->setElideMode(Qt::ElideNone);

  dock->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(dock, &ads::CDockWidget::customContextMenuRequested, this,
          &EditorManager::onDockCustomContextMenuRequested);

  auto* obj = editor->signalObject();
  if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
    connect(textEditor, &TextEditorWidget::modificationChanged, this,
            [this, editor, dock](bool modified) {
              QString title = editor->displayName();
              if (modified) {
                title.prepend("* ");
              }
              dock->setWindowTitle(title);
              emit unsavedChangesChanged();
              emit modificationChanged(modified);
            });
  } else if (auto* topoEditor =
                 dynamic_cast<etest::topology::TopologyEditorWidget*>(editor)) {
    connect(topoEditor,
            &etest::topology::TopologyEditorWidget::modificationChanged, this,
            [this, editor, dock](bool modified) {
              QString title = editor->displayName();
              if (modified) {
                title.prepend("* ");
              }
              dock->setWindowTitle(title);
              emit unsavedChangesChanged();
              emit modificationChanged(modified);
            });
    connect(topoEditor, &etest::topology::TopologyEditorWidget::editorIdChanged,
            this, [this, editor](const QString&, const QString& newId) {
              updateEditorId(editor, newId);
            });
  } else if (auto* protocalEditor =
                 dynamic_cast<etest::protocal::ProtocalEditorWidget*>(editor)) {
    connect(protocalEditor,
            &etest::protocal::ProtocalEditorWidget::modificationChanged, this,
            [this, editor, dock](bool modified) {
              QString title = editor->displayName();
              if (modified) {
                title.prepend("* ");
              }
              dock->setWindowTitle(title);
              emit unsavedChangesChanged();
              emit modificationChanged(modified);
            });
    connect(protocalEditor,
            &etest::protocal::ProtocalEditorWidget::editorIdChanged, this,
            [this, editor](const QString&, const QString& newId) {
              updateEditorId(editor, newId);
            });
  }

  connect(dock, &ads::CDockWidget::closeRequested, this,
          [this, editorKey]() { closeFile(editorKey); });

  if (dock_widgets_.isEmpty()) {
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

  dock_widgets_[editorKey] = dock;
  editors_[editorKey] = editor;

  current_file_path_ = editorKey;
  emit currentEditorChanged(editor);

  dock->raise();

  LOG_INFO("EDITOR", "打开文件：{}", filePath.toStdString());
  emit fileOpened(filePath);

  // 注册项目工件引用
  using etest::core::project::ProjectManager;
  auto& pm = ProjectManager::instance();
  if (suffix == QStringLiteral("etopo")) {
    pm.registerTopologyRef(filePath);
  } else if (suffix == QStringLiteral("eproto")) {
    pm.registerProtocolRef(filePath);
  }
}

void EditorManager::openFileAtLine(const QString& filePath, int line) {
  openFile(filePath);

  auto it = editors_.find(filePath);
  if (it != editors_.end()) {
    auto* editor = it.value();
    if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
      int targetLine = qMax(0, line - 1);
      textEditor->editor()->setCursorPosition(targetLine, 0);
      textEditor->editor()->ensureLineVisible(targetLine);
    }
  }
}

bool EditorManager::closeFile(const QString& editorId) {
  auto it = editors_.find(editorId);
  if (it == editors_.end())
    return false;

  auto* editor = it.value();
  if (editor->isModified()) {
    QWidget* parentWidget = dock_manager_->topLevelWidget();
    if (!parentWidget && qApp) {
      parentWidget = qApp->activeWindow();
    }
    int ret = QMessageBox::question(
        parentWidget, QStringLiteral("保存更改"),
        QStringLiteral("文件 \"%1\" 已修改，是否保存？")
            .arg(editor->displayName()),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel)
      return false;
    if (ret == QMessageBox::Yes) {
      if (!editor->save())
        return false;
    }
  }

  auto* dock = dock_widgets_.take(editorId);
  editors_.erase(it);

  if (dock) {
    dock->closeDockWidget();
  }

  if (current_file_path_ == editorId) {
    if (!editors_.isEmpty()) {
      current_file_path_ = editors_.firstKey();
      auto* dock = dock_widgets_[current_file_path_];
      if (dock) {
        dock->raise();
      }
      emit currentEditorChanged(editors_.first());
    } else {
      current_file_path_.clear();
      emit currentEditorChanged(nullptr);
    }
  }

  LOG_INFO("EDITOR", "关闭编辑器：{}", editorId.toStdString());
  emit fileClosed(editorId);

  // 注销项目工件引用
  using etest::core::project::ProjectManager;
  auto& pm = ProjectManager::instance();
  QFileInfo fi(editorId);
  QString suffix = fi.suffix().toLower();
  if (suffix == QStringLiteral("etopo")) {
    pm.removeTopologyRef(editorId);
  } else if (suffix == QStringLiteral("eproto")) {
    pm.removeProtocolRef(editorId);
  }

  emit unsavedChangesChanged();
  return true;
}

bool EditorManager::closeAllFiles() {
  QStringList ids = editors_.keys();
  for (const QString& id : ids) {
    if (!closeFile(id))
      return false;
  }
  return true;
}

bool EditorManager::saveAllFiles() {
  for (auto* editor : editors_) {
    if (editor->isModified()) {
      if (!editor->save()) {
        LOG_ERROR("EDITOR", "保存失败：{}", editor->filePath().toStdString());
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

  if (filePaths.size() == 1) {
    QString path = filePaths.first();
    QFileInfo fi(path);
    if (fi.isDir()) {
      return saveModifiedFilesInDirectory(path);
    }
  }

  for (const QString& fp : filePaths) {
    auto it = editors_.find(fp);
    if (it != editors_.end() && it.value()->isModified()) {
      if (!it.value()->save()) {
        LOG_ERROR("EDITOR", "保存失败：{}", fp.toStdString());
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
    QString relativePath = dir.relativeFilePath(it.key());
    if (!relativePath.startsWith("..") && it.value()->isModified()) {
      if (!it.value()->save()) {
        LOG_ERROR("EDITOR", "保存失败：{}", it.key().toStdString());
        return false;
      }
    }
  }
  return true;
}

IEditor* EditorManager::editorById(const QString& id) const {
  return editors_.value(id, nullptr);
}

QStringList EditorManager::openFiles() const {
  return editors_.keys();
}

bool EditorManager::isOpen(const QString& editorId) const {
  return editors_.contains(editorId);
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
    QString relativePath = dir.relativeFilePath(it.key());
    if (!relativePath.startsWith("..") && it.value()->isModified()) {
      return true;
    }
  }
  return false;
}

IEditor* EditorManager::currentEditor() const {
  if (current_file_path_.isEmpty())
    return nullptr;
  return editors_.value(current_file_path_, nullptr);
}

QString EditorManager::currentFilePath() const {
  return current_file_path_;
}

void EditorManager::createEditor(const QString& editorType,
                                 const QString& id,
                                 const QString& title) {
  if (editors_.contains(id)) {
    auto* dock = dock_widgets_[id];
    dock->raise();
    return;
  }

  IEditor* editor = EditorFactoryRegistry::create(editorType, id, nullptr);
  if (!editor) {
    LOG_WARN("EDITOR", "无法创建编辑器类型：{}", editorType.toStdString());
    return;
  }

  auto* dock = new ads::CDockWidget(title);
  dock->setWidget(editor->widget());
  dock->setFeature(ads::CDockWidget::DockWidgetDeleteOnClose, true);
  dock->setFeature(ads::CDockWidget::CustomCloseHandling, true);

  auto* obj = editor->signalObject();
  if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
    connect(textEditor, &TextEditorWidget::modificationChanged, this,
            [this, editor, dock](bool modified) {
              QString title = editor->displayName();
              if (modified)
                title.prepend("* ");
              dock->setWindowTitle(title);
              emit unsavedChangesChanged();
              emit modificationChanged(modified);
            });
  } else if (auto* topoEditor =
                 dynamic_cast<etest::topology::TopologyEditorWidget*>(editor)) {
    connect(topoEditor,
            &etest::topology::TopologyEditorWidget::modificationChanged, this,
            [this, editor, dock](bool modified) {
              QString title = editor->displayName();
              if (modified)
                title.prepend("* ");
              dock->setWindowTitle(title);
              emit unsavedChangesChanged();
              emit modificationChanged(modified);
            });
    connect(topoEditor, &etest::topology::TopologyEditorWidget::editorIdChanged,
            this, [this, editor](const QString&, const QString& newId) {
              updateEditorId(editor, newId);
            });
  } else if (auto* protocalEditor =
                 dynamic_cast<etest::protocal::ProtocalEditorWidget*>(editor)) {
    connect(protocalEditor,
            &etest::protocal::ProtocalEditorWidget::modificationChanged, this,
            [this, editor, dock](bool modified) {
              QString title = editor->displayName();
              if (modified)
                title.prepend("* ");
              dock->setWindowTitle(title);
              emit unsavedChangesChanged();
              emit modificationChanged(modified);
            });
    connect(protocalEditor,
            &etest::protocal::ProtocalEditorWidget::editorIdChanged, this,
            [this, editor](const QString&, const QString& newId) {
              updateEditorId(editor, newId);
            });
  }

  connect(dock, &ads::CDockWidget::closeRequested, this,
          [this, id]() { closeFile(id); });

  if (dock_widgets_.isEmpty()) {
    dock_manager_->addDockWidget(ads::CenterDockWidgetArea, dock);
  } else {
    auto* existingDock = *dock_widgets_.constBegin();
    auto* area = existingDock->dockAreaWidget();
    if (area) {
      dock_manager_->addDockWidgetTabToArea(dock, area);
    } else {
      dock_manager_->addDockWidget(ads::CenterDockWidgetArea, dock);
    }
  }

  dock_widgets_[id] = dock;
  editors_[id] = editor;

  current_file_path_ = id;
  emit currentEditorChanged(editor);

  dock->raise();
}

void EditorManager::updateEditorId(IEditor* editor, const QString& newId) {
  // Find old key
  QString oldId;
  for (auto it = editors_.constBegin(); it != editors_.constEnd(); ++it) {
    if (it.value() == editor) {
      oldId = it.key();
      break;
    }
  }
  if (oldId.isEmpty() || oldId == newId)
    return;

  editors_.remove(oldId);
  editors_[newId] = editor;

  auto* dock = dock_widgets_.take(oldId);
  if (dock) {
    dock_widgets_[newId] = dock;
  }

  if (current_file_path_ == oldId) {
    current_file_path_ = newId;
  }

  updateDockTitle(editor, dock);

  // 编辑器 ID 变化时同步更新工件引用（如另存为场景）
  using etest::core::project::ProjectManager;
  auto& pm = ProjectManager::instance();
  QFileInfo oldFi(oldId);
  QFileInfo newFi(newId);
  QString oldSuffix = oldFi.suffix().toLower();
  QString newSuffix = newFi.suffix().toLower();

  if (oldSuffix == QStringLiteral("etopo")) {
    pm.removeTopologyRef(oldId);
  } else if (oldSuffix == QStringLiteral("eproto")) {
    pm.removeProtocolRef(oldId);
  }

  if (newSuffix == QStringLiteral("etopo")) {
    pm.registerTopologyRef(newId);
  } else if (newSuffix == QStringLiteral("eproto")) {
    pm.registerProtocolRef(newId);
  }
}

void EditorManager::onDockWidgetActivated(ads::CDockWidget* dock) {
  if (!dock)
    return;

  QString editorId;
  for (auto it = dock_widgets_.constBegin(); it != dock_widgets_.constEnd();
       ++it) {
    if (it.value() == dock) {
      editorId = it.key();
      break;
    }
  }

  if (editorId.isEmpty() || editorId == current_file_path_)
    return;

  current_file_path_ = editorId;
  auto* editor = editors_.value(editorId, nullptr);
  emit currentEditorChanged(editor);
}

void EditorManager::updateDockTitle(IEditor* editor, ads::CDockWidget* dock) {
  QString title = editor->displayName();
  if (editor->isModified()) {
    title.prepend("* ");
  }
  dock->setWindowTitle(title);
}

void EditorManager::onFileDeleted(const QString& filePath) {
  auto it = editors_.find(filePath);
  if (it == editors_.end()) {
    return;
  }

  auto* editor = it.value();
  if (editor->isModified()) {
    QWidget* parentWidget = dock_manager_->topLevelWidget();
    if (!parentWidget && qApp) {
      parentWidget = qApp->activeWindow();
    }
    int ret = QMessageBox::question(
        parentWidget, QStringLiteral("文件已删除"),
        QStringLiteral("文件 \"%1\" 已被删除，是否保存更改到其他位置？")
            .arg(editor->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Cancel) {
      return;
    }

    if (ret == QMessageBox::Save) {
      if (!editor->saveAs(QString())) {
        return;
      }
    } else if (ret == QMessageBox::Discard) {
      if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
        textEditor->editor()->setModified(false);
      }
    }
  }

  closeFile(filePath);
}

void EditorManager::onFileRenamed(const QString& oldPath,
                                  const QString& newPath) {
  auto it = editors_.find(oldPath);
  if (it == editors_.end()) {
    return;
  }

  auto* editor = it.value();
  auto* dock = dock_widgets_[oldPath];

  if (auto* textEditor = dynamic_cast<TextEditorWidget*>(editor)) {
    textEditor->setFilePath(newPath);
  }

  editors_.remove(oldPath);
  editors_[newPath] = editor;
  dock_widgets_.remove(oldPath);
  dock_widgets_[newPath] = dock;

  if (current_file_path_ == oldPath) {
    current_file_path_ = newPath;
  }

  updateDockTitle(editor, dock);

  LOG_INFO("EDITOR", "文件已重命名：{} -> {}", oldPath.toStdString(),
           newPath.toStdString());
}

bool EditorManager::closeFilesInDirectory(const QString& dirPath) {
  if (dirPath.isEmpty()) {
    return closeAllFiles();
  }

  QDir dir(dirPath);
  if (!dir.exists()) {
    return true;
  }

  QStringList filesToClose;
  for (const QString& editorId : editors_.keys()) {
    QString relativePath = dir.relativeFilePath(editorId);
    if (!relativePath.startsWith("..")) {
      filesToClose.append(editorId);
    }
  }

  for (const QString& fp : filesToClose) {
    if (!closeFile(fp)) {
      return false;
    }
  }

  return true;
}

void EditorManager::onDockCustomContextMenuRequested(const QPoint& pos) {
  auto* dock = qobject_cast<ads::CDockWidget*>(sender());
  if (!dock)
    return;

  QString editorId;
  for (auto it = dock_widgets_.constBegin(); it != dock_widgets_.constEnd();
       ++it) {
    if (it.value() == dock) {
      editorId = it.key();
      break;
    }
  }
  if (editorId.isEmpty())
    return;

  QMenu menu(dock);

  QAction* closeAction = menu.addAction(QStringLiteral("关闭"));
  connect(closeAction, &QAction::triggered, this,
          [this, editorId]() { closeFile(editorId); });

  QAction* closeOthersAction = menu.addAction(QStringLiteral("关闭其他"));
  connect(closeOthersAction, &QAction::triggered, this, [this, editorId]() {
    QStringList allIds = dock_widgets_.keys();
    for (const QString& id : allIds) {
      if (id != editorId) {
        closeFile(id);
      }
    }
  });

  QAction* closeRightAction = menu.addAction(QStringLiteral("关闭右侧所有"));
  connect(closeRightAction, &QAction::triggered, this,
          [this, dock, editorId]() {
            auto* area = dock->dockAreaWidget();
            if (!area)
              return;

            auto docks = area->dockWidgets();
            int currentIndex = docks.indexOf(dock);
            if (currentIndex == -1)
              return;

            for (int i = currentIndex + 1; i < docks.size(); ++i) {
              QString id;
              for (auto it = dock_widgets_.constBegin();
                   it != dock_widgets_.constEnd(); ++it) {
                if (it.value() == docks[i]) {
                  id = it.key();
                  break;
                }
              }
              if (!id.isEmpty()) {
                closeFile(id);
              }
            }
          });

  menu.addSeparator();

  QAction* copyPathAction = menu.addAction(QStringLiteral("复制文件路径"));
  connect(copyPathAction, &QAction::triggered, this,
          [editorId]() { QApplication::clipboard()->setText(editorId); });

  QAction* openFolderAction = menu.addAction(QStringLiteral("打开所在文件夹"));
  connect(openFolderAction, &QAction::triggered, this, [editorId]() {
    QFileInfo fi(editorId);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
  });

  menu.exec(dock->mapToGlobal(pos));
}

}  // namespace etest::app
