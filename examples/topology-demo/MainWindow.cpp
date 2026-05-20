#include "MainWindow.h"

#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QStatusBar>

#include "api/IEditor.h"
#include "topology/TopologyDocument.h"
#include "topology/TopologyEditorWidget.h"
#include "topology/TopologyJsonSerializer.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  editor_ = new etest::topology::TopologyEditorWidget(this);
  editor_->setEditorId(QString());
  setCentralWidget(editor_);
  createMenus();
  updateWindowTitle();

  connect(editor_, &etest::topology::TopologyEditorWidget::modificationChanged,
          this, [this]() { updateWindowTitle(); });
  connect(editor_, &etest::topology::TopologyEditorWidget::editorTitleChanged,
          this, [this]() { updateWindowTitle(); });
}

void MainWindow::onNew() {
  if (!confirmSave())
    return;
  editor_->document()->clear();
  editor_->reloadScene();
  editor_->setEditorId(QString());
  updateWindowTitle();
  statusBar()->showMessage(QStringLiteral("已新建拓扑"), 3000);
}

void MainWindow::onOpen() {
  if (!confirmSave())
    return;
  QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开拓扑文件"), QString(),
      QStringLiteral("拓扑文件 (*.etopo);;所有文件 (*)"));
  if (path.isEmpty())
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, QStringLiteral("错误"),
                         QStringLiteral("无法打开文件: %1").arg(path));
    return;
  }
  QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!jdoc.isObject()) {
    QMessageBox::warning(this, QStringLiteral("错误"),
                         QStringLiteral("无效的拓扑文件"));
    return;
  }

  auto* doc = editor_->document();
  doc->clear();
  etest::topology::TopologyJsonSerializer::deserialize(jdoc.object(), doc);
  editor_->reloadScene();
  editor_->setEditorId(path);
  if (doc->undoStack())
    doc->undoStack()->clear();
  updateWindowTitle();
  statusBar()->showMessage(QStringLiteral("已打开: %1").arg(path), 3000);
}

void MainWindow::onSave() {
  if (editor_->filePath().isEmpty()) {
    onSaveAs();
    return;
  }
  editor_->save();
  updateWindowTitle();
  statusBar()->showMessage(QStringLiteral("已保存"), 3000);
}

void MainWindow::onSaveAs() {
  QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("保存拓扑文件"), QString(),
      QStringLiteral("拓扑文件 (*.etopo);;所有文件 (*)"));
  if (path.isEmpty())
    return;
  if (!path.endsWith(QStringLiteral(".etopo"), Qt::CaseInsensitive))
    path += QStringLiteral(".etopo");
  editor_->saveAs(path);
  updateWindowTitle();
  statusBar()->showMessage(QStringLiteral("已保存: %1").arg(path), 3000);
}

void MainWindow::createMenus() {
  // ── 文件菜单 ──
  auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件"));

  auto* newAction = fileMenu->addAction(QStringLiteral("新建"));
  newAction->setShortcut(QKeySequence::New);
  connect(newAction, &QAction::triggered, this, &MainWindow::onNew);

  auto* openAction = fileMenu->addAction(QStringLiteral("打开"));
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &MainWindow::onOpen);

  fileMenu->addSeparator();

  auto* saveAction = fileMenu->addAction(QStringLiteral("保存"));
  saveAction->setShortcut(QKeySequence::Save);
  connect(saveAction, &QAction::triggered, this, &MainWindow::onSave);

  auto* saveAsAction = fileMenu->addAction(QStringLiteral("另存为..."));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAs);

  fileMenu->addSeparator();

  auto* exitAction = fileMenu->addAction(QStringLiteral("退出"));
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

  // ── 编辑菜单 ──
  auto* editMenu = menuBar()->addMenu(QStringLiteral("编辑"));

  auto* ieditor = static_cast<etest::app::IEditor*>(editor_);

  auto* undoAction = editMenu->addAction(QStringLiteral("撤销"));
  undoAction->setShortcut(QKeySequence::Undo);
  connect(undoAction, &QAction::triggered, editor_,
          [ieditor]() { ieditor->undo(); });

  auto* redoAction = editMenu->addAction(QStringLiteral("重做"));
  redoAction->setShortcut(QKeySequence::Redo);
  connect(redoAction, &QAction::triggered, editor_,
          [ieditor]() { ieditor->redo(); });

  editMenu->addSeparator();

  auto* copyAction = editMenu->addAction(QStringLiteral("复制"));
  copyAction->setShortcut(QKeySequence::Copy);
  connect(copyAction, &QAction::triggered, editor_, [this]() {
    QMetaObject::invokeMethod(editor_, "onCopy");
  });

  auto* pasteAction = editMenu->addAction(QStringLiteral("粘贴"));
  pasteAction->setShortcut(QKeySequence::Paste);
  connect(pasteAction, &QAction::triggered, editor_, [this]() {
    QMetaObject::invokeMethod(editor_, "onPaste");
  });
}

bool MainWindow::confirmSave() {
  if (!editor_->isModified())
    return true;
  auto ret = QMessageBox::question(
      this, QStringLiteral("未保存"),
      QStringLiteral("当前拓扑已修改，是否保存？"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
  if (ret == QMessageBox::Save)
    onSave();
  return ret != QMessageBox::Cancel;
}

void MainWindow::updateWindowTitle() {
  QString title = QStringLiteral("拓扑编辑器 Demo");
  if (!editor_->filePath().isEmpty())
    title += QStringLiteral(" - %1").arg(editor_->filePath());
  if (editor_->isModified())
    title += QStringLiteral(" *");
  setWindowTitle(title);
}
