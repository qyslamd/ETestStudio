#include "EditorManager.h"
#include "DockAreaWidget.h"
#include "DockWidgetTab.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMenu>
#include <QMessageBox>

#include "editors/EditorFactory.h"
#include "editors/ImageViewerWidget.h"
#include "editors/EtlogViewerWidget.h"
#include "editors/TextEditorWidget.h"
#include "TestProgramEditorWidget.h"
#include "SignalRegistry.h"
#include "dialogs/IcdSignalSelection.h"
#include "icd/repository.hpp"
#include "logger/Logger.h"
#include "utils/SignalSyncHelper.h"
#include "protocol/ProtocolEditorWidget.h"
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
  EditorFactoryRegistry::registerExtension("eproto", "protocol");
  EditorFactoryRegistry::registerExtension("eprotox", "protocol");
  EditorFactoryRegistry::registerExtension("etprog", "testprogram");

  EditorFactoryRegistry::registerExtension("png", "image");
  EditorFactoryRegistry::registerExtension("jpg", "image");
  EditorFactoryRegistry::registerExtension("jpeg", "image");
  EditorFactoryRegistry::registerExtension("bmp", "image");
  EditorFactoryRegistry::registerExtension("gif", "image");
  EditorFactoryRegistry::registerExtension("webp", "image");
  EditorFactoryRegistry::registerExtension("ico", "image");

  EditorFactoryRegistry::registerExtension("etlog", "etlog");
  EditorFactoryRegistry::registerFactory(
      "etlog", [](const QString& id, QWidget* parent) {
        return new EtlogViewerWidget(id, parent);
      });

  EditorFactoryRegistry::registerFactory(
      "text",
      [](const QString& path, QWidget* parent) {
        return new TextEditorWidget(path, parent);
      },
      [](IEditor* editor, ads::CDockWidget* dock, EditorManager* mgr) {
        auto* te = qobject_cast<TextEditorWidget*>(editor->widget());
        if (!te)
          return;
        QObject::connect(
            te, &TextEditorWidget::modificationChanged, mgr,
            [editor, dock, mgr](bool modified) {
              dock->setWindowTitle((modified ? QStringLiteral("* ") : QString())
                                       .append(editor->displayName()));
              emit mgr->unsavedChangesChanged();
              emit mgr->modificationChanged(modified);
            });
      });

  EditorFactoryRegistry::registerFactory(
      "topology",
      [](const QString& id, QWidget* parent) {
        auto* editor = new etest::topology::TopologyEditorWidget(parent);
        editor->setEmbeddedMode(true);
        return editor;
      },
      [](IEditor* editor, ads::CDockWidget* dock, EditorManager* mgr) {
        auto* te = qobject_cast<etest::topology::TopologyEditorWidget*>(
            editor->widget());
        if (!te)
          return;
        QObject::connect(
            te, &etest::topology::TopologyEditorWidget::modificationChanged,
            mgr, [editor, dock, mgr](bool modified) {
              dock->setWindowTitle((modified ? QStringLiteral("* ") : QString())
                                       .append(editor->displayName()));
              emit mgr->unsavedChangesChanged();
              emit mgr->modificationChanged(modified);
            });
        QObject::connect(
            te, &etest::topology::TopologyEditorWidget::editorIdChanged, mgr,
            [editor, mgr](const QString&, const QString& newId) {
              mgr->updateEditorId(editor, newId);
            });
        // M3+: 注入可用 ICD 帧名到拓扑属性面板
        if (mgr->icdRepository()) {
          QStringList frames;
          for (const auto& frame : mgr->icdRepository()->frames()) {
            if (!frame) continue;
            auto name = frame->name();
            frames.append(QString::fromUtf8(name.data(),
                                             static_cast<int>(name.size())));
          }
          te->setAvailableIcdFrames(frames);
        }

        // M2+: 将拓扑设备及其端口绑定注册到 SignalRegistry
        auto* sigReg = mgr->signalRegistry();
        auto* icdRepo = mgr->icdRepository();
        if (sigReg) {
          auto* doc = te->document();
          if (doc) {
            // 注册已有设备
            for (int i = 0; i < doc->deviceCount(); ++i) {
              const auto* dev = doc->device(i);
              LOG_DEBUG("UUID", "topology binder: registerDevice id={} name={}",
                       dev->id.toStdString(), dev->name.toStdString());
              sigReg->registerDevice(dev->id, dev->name,
                                        dev->deviceType);
              for (int pi = 0; pi < dev->ports.size(); ++pi) {
                sigReg->bindPortToFrames(dev->id, dev->ports[pi].name,
                                          dev->ports[pi].boundFrameNames);
              }
            }
            LOG_DEBUG("UUID", "topology binder: registered {} devices from doc (icdRepo={})",
                     doc->deviceCount(), icdRepo ? "yes" : "no");
            // 若有 ICD 数据，重建信号索引
            if (icdRepo) {
              etest::app::synchronizeRegistry(*sigReg, icdRepo);
              LOG_DEBUG("UUID", "topology binder: synchronizeRegistry done, total devices={}",
                       sigReg->registeredDeviceIds().size());
            }
          }
          // 监听设备/端口变更 → 动态更新 registry
          QObject::connect(doc, &etest::topology::TopologyDocument::deviceAdded,
                           mgr, [sigReg, icdRepo, doc](int index) {
            const auto* dev = doc->device(index);
            if (dev) {
              LOG_DEBUG("UUID", "deviceAdded signal: id={} name={}",
                       dev->id.toStdString(), dev->name.toStdString());
              sigReg->registerDevice(dev->id, dev->name,
                                        dev->deviceType);
              for (int pi = 0; pi < dev->ports.size(); ++pi) {
                sigReg->bindPortToFrames(dev->id, dev->ports[pi].name,
                                          dev->ports[pi].boundFrameNames);
              }
              if (icdRepo) {
                etest::app::synchronizeRegistry(*sigReg, icdRepo);
              }
            }
          });
          QObject::connect(doc,
                           &etest::topology::TopologyDocument::deviceRemoved,
                           mgr, [sigReg, icdRepo, doc]() {
            // 设备已从 doc 中删除，全量重建 registry
            sigReg->clear();
            for (int i = 0; i < doc->deviceCount(); ++i) {
              const auto* dev = doc->device(i);
              sigReg->registerDevice(dev->id, dev->name,
                                        dev->deviceType);
              for (int pi = 0; pi < dev->ports.size(); ++pi) {
                sigReg->bindPortToFrames(dev->id, dev->ports[pi].name,
                                          dev->ports[pi].boundFrameNames);
              }
            }
            if (icdRepo) {
              etest::app::synchronizeRegistry(*sigReg, icdRepo);
            }
          });
          QObject::connect(
              doc, &etest::topology::TopologyDocument::deviceChanged, mgr,
              [sigReg, icdRepo, doc](int index) {
            const auto* dev = doc->device(index);
            if (dev) {
              sigReg->registerDevice(dev->id, dev->name,
                                        dev->deviceType);
              if (icdRepo) {
                etest::app::synchronizeRegistry(*sigReg, icdRepo);
              }
            }
          });
          QObject::connect(
              doc, &etest::topology::TopologyDocument::devicePortFramesChanged,
              mgr, [sigReg, icdRepo]() {
                if (icdRepo) {
                  etest::app::synchronizeRegistry(*sigReg, icdRepo);
                }
              });
          QObject::connect(doc,
                           &etest::topology::TopologyDocument::documentCleared,
                           mgr, [sigReg]() { sigReg->clear(); });
        }
      });

  EditorFactoryRegistry::registerFactory(
      "protocol",
      [](const QString& id, QWidget* parent) {
        auto* editor = new etest::protocol::ProtocolEditorWidget(parent);
        editor->setEmbeddedMode(true);
        return editor;
      },
      [](IEditor* editor, ads::CDockWidget* dock, EditorManager* mgr) {
        auto* pe = qobject_cast<etest::protocol::ProtocolEditorWidget*>(
            editor->widget());
        if (!pe)
          return;
        QObject::connect(
            pe, &etest::protocol::ProtocolEditorWidget::modificationChanged,
            mgr, [editor, dock, mgr](bool modified) {
              dock->setWindowTitle((modified ? QStringLiteral("* ") : QString())
                                       .append(editor->displayName()));
              emit mgr->unsavedChangesChanged();
              emit mgr->modificationChanged(modified);
            });
        QObject::connect(
            pe, &etest::protocol::ProtocolEditorWidget::editorIdChanged, mgr,
            [editor, mgr](const QString&, const QString& newId) {
              mgr->updateEditorId(editor, newId);
            });
      });

  EditorFactoryRegistry::registerFactory(
      "image", [](const QString& id, QWidget* parent) {
        return new ImageViewerWidget(id, parent);
      });

  EditorFactoryRegistry::registerFactory(
      "testprogram",
      [](const QString& id, QWidget* parent) {
        auto* editor = new TestProgramEditorWidget(id, parent);
        editor->setEmbeddedMode(true);
        return editor;
      },
      [](IEditor* editor, ads::CDockWidget* dock, EditorManager* mgr) {
        auto* te = qobject_cast<TestProgramEditorWidget*>(editor->widget());
        if (!te)
          return;
        // M5: 注入 ICD 信号选择器和 registry（非 null 时才注入）
        LOG_DEBUG("UUID", "testprogram binder: sigReg={} icdRepo={}",
                 (mgr->signalRegistry() ? "ok" : "null"),
                 (mgr->icdRepository() ? "ok" : "null"));
        if (mgr->signalRegistry() && mgr->icdRepository()) {
          auto* sel = new IcdSignalSelection(mgr->signalRegistry(),
                                             mgr->icdRepository());
          te->setSignalSelection(sel);
          te->setRegistry(mgr->signalRegistry());
          LOG_DEBUG("UUID", "testprogram binder: IcdSignalSelection injected");
        }
        QObject::connect(
            te, &TestProgramEditorWidget::modificationChanged, mgr,
            [editor, dock, mgr](bool modified) {
              dock->setWindowTitle((modified ? QStringLiteral("* ") : QString())
                                       .append(editor->displayName()));
              emit mgr->unsavedChangesChanged();
              emit mgr->modificationChanged(modified);
            });
        QObject::connect(te, &TestProgramEditorWidget::editorIdChanged, mgr,
                         [editor, mgr](const QString&, const QString& newId) {
                           mgr->updateEditorId(editor, newId);
                         });
      });
}

void EditorManager::openFile(const QString& filePath,
                             const QString& forcedEditorType) {
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
  QString editorType = forcedEditorType.isEmpty()
                           ? EditorFactoryRegistry::typeForExtension(suffix)
                           : forcedEditorType;
  if (editorType.isEmpty()) {
    editorType = QStringLiteral("text");
  }

  // ICDConfig content detection for .xml/.json files
  if (forcedEditorType.isEmpty() &&
      (suffix == QStringLiteral("xml") || suffix == QStringLiteral("json"))) {
    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly)) {
      QByteArray header = f.read(4096);
      f.close();
      if (header.contains("<ICDConfig>")) {
        editorType = QStringLiteral("protocol");
      }
    }
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

  bindDockTabContextMenu(dock);

  auto binder = EditorFactoryRegistry::binderForType(editorType);
  if (binder) {
    binder(editor, dock, this);
  }
  editor->openFile(filePath);

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

  if (current_file_path_ == editorId) {
    // 优先激活相邻tab，而非字母序第一个
    QString nextEditorId;
    if (dock) {
      auto* area = dock->dockAreaWidget();
      if (area) {
        auto docks = area->dockWidgets();
        for (int i = 0; i < docks.size(); ++i) {
          if (docks[i] == dock) {
            if (i + 1 < docks.size()) {
              for (auto it = dock_widgets_.constBegin();
                   it != dock_widgets_.constEnd(); ++it) {
                if (it.value() == docks[i + 1]) {
                  nextEditorId = it.key();
                  break;
                }
              }
            } else if (i > 0) {
              for (auto it = dock_widgets_.constBegin();
                   it != dock_widgets_.constEnd(); ++it) {
                if (it.value() == docks[i - 1]) {
                  nextEditorId = it.key();
                  break;
                }
              }
            }
            break;
          }
        }
      }
    }

    if (nextEditorId.isEmpty() && !editors_.isEmpty()) {
      nextEditorId = editors_.firstKey();
    }

    if (!nextEditorId.isEmpty()) {
      current_file_path_ = nextEditorId;
      auto* nextDock = dock_widgets_[nextEditorId];
      if (nextDock) {
        nextDock->raise();
      }
      emit currentEditorChanged(editors_.value(nextEditorId));
    } else {
      current_file_path_.clear();
      emit currentEditorChanged(nullptr);
    }
  }

  if (dock) {
    dock->closeDockWidget();
  }

  LOG_INFO("EDITOR", "关闭编辑器：{}", editorId.toStdString());
  emit fileClosed(editorId);

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

QList<IEditor*> EditorManager::allEditors() const {
  return editors_.values();
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

  bindDockTabContextMenu(dock);

  auto binder = EditorFactoryRegistry::binderForType(editorType);
  if (binder) {
    binder(editor, dock, this);
  }
  editor->openFile(id);

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
      QString newPath = QFileDialog::getSaveFileName(
          parentWidget, QStringLiteral("保存文件到"), editor->filePath(),
          QStringLiteral("所有文件 (*)"));
      if (newPath.isEmpty())
        return;
      if (!editor->saveAs(newPath))
        return;
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
  } else if (auto* protocolEditor =
                 dynamic_cast<etest::protocol::ProtocolEditorWidget*>(editor)) {
    protocolEditor->openFile(newPath);
  } else if (auto* topoEditor =
                 dynamic_cast<etest::topology::TopologyEditorWidget*>(editor)) {
    topoEditor->openFile(newPath);
  } else if (auto* tpEditor = dynamic_cast<TestProgramEditorWidget*>(editor)) {
    tpEditor->openFile(newPath);
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

void EditorManager::bindDockTabContextMenu(ads::CDockWidget* dock) {
  if (!dock || !dock->tabWidget()) {
    return;
  }

  auto* tab = dock->tabWidget();
  tab->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(tab, &QWidget::customContextMenuRequested, this,
          [this, dock, tab](const QPoint& pos) {
            showDockContextMenu(dock, tab->mapToGlobal(pos));
          });
}

void EditorManager::showDockContextMenu(ads::CDockWidget* dock,
                                        const QPoint& globalPos) {
  if (!dock) {
    return;
  }

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
          [this, editorId]() {
            LOG_INFO("PROJECT_UI", "编辑器标签关闭 [action=关闭] [path={}]", QFileInfo(editorId).fileName().toStdString());
            closeFile(editorId);
          });

  QAction* closeOthersAction = menu.addAction(QStringLiteral("关闭其他"));
  connect(closeOthersAction, &QAction::triggered, this, [this, editorId]() {
    LOG_INFO("PROJECT_UI", "编辑器标签关闭 [action=关闭其他]");
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
            LOG_INFO("PROJECT_UI", "编辑器标签关闭 [action=关闭右侧所有]");
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
          [editorId]() {
            LOG_INFO("PROJECT_UI", "复制文件路径 [path={}]", QFileInfo(editorId).fileName().toStdString());
            QApplication::clipboard()->setText(editorId);
          });

  QAction* openFolderAction = menu.addAction(QStringLiteral("打开所在文件夹"));
  connect(openFolderAction, &QAction::triggered, this, [editorId]() {
    LOG_INFO("PROJECT_UI", "打开所在文件夹 [path={}]", QFileInfo(editorId).fileName().toStdString());
    QFileInfo fi(editorId);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
  });

  menu.exec(globalPos);
}

}  // namespace etest::app
