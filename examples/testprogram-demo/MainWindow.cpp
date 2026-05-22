#include "MainWindow.h"

#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include "TestProgramEditorWidget.h"
#include "api/IEditor.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  editor_ = new etest::app::TestProgramEditorWidget(QString(), this);
  setCentralWidget(editor_);
  createMenus();
  updateWindowTitle();

  connect(editor_, &etest::app::TestProgramEditorWidget::modificationChanged,
          this, [this]() { updateWindowTitle(); });
}

void MainWindow::onNew() {
  if (!confirmSave()) return;

  auto* old = editor_;
  editor_ = new etest::app::TestProgramEditorWidget(QString(), this);
  setCentralWidget(editor_);
  old->deleteLater();

  connect(editor_, &etest::app::TestProgramEditorWidget::modificationChanged,
          this, [this]() { updateWindowTitle(); });

  updateWindowTitle();
  statusBar()->showMessage(QStringLiteral("已新建测试程序"), 3000);
}

void MainWindow::onOpen() {
  if (!confirmSave()) return;

  QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开测试程序文件"), QString(),
      QStringLiteral("测试程序文件 (*.tcase);;所有文件 (*)"));
  if (path.isEmpty()) return;

  // 通过构造函数传参触发自动加载
  auto* old = editor_;
  editor_ = new etest::app::TestProgramEditorWidget(path, this);
  setCentralWidget(editor_);
  old->deleteLater();

  connect(editor_, &etest::app::TestProgramEditorWidget::modificationChanged,
          this, [this]() { updateWindowTitle(); });

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
      this, QStringLiteral("保存测试程序文件"), QString(),
      QStringLiteral("测试程序文件 (*.tcase);;所有文件 (*)"));
  if (path.isEmpty()) return;
  if (!path.endsWith(QStringLiteral(".tcase"), Qt::CaseInsensitive))
    path += QStringLiteral(".tcase");

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

  auto* undoAction = editMenu->addAction(QStringLiteral("撤销"));
  undoAction->setShortcut(QKeySequence::Undo);
  connect(undoAction, &QAction::triggered, editor_,
          [this]() { editor_->undo(); });

  auto* redoAction = editMenu->addAction(QStringLiteral("重做"));
  redoAction->setShortcut(QKeySequence::Redo);
  connect(redoAction, &QAction::triggered, editor_,
          [this]() { editor_->redo(); });
}

bool MainWindow::confirmSave() {
  if (!editor_->isModified()) return true;
  auto ret = QMessageBox::question(
      this, QStringLiteral("未保存"),
      QStringLiteral("当前测试程序已修改，是否保存？"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
  if (ret == QMessageBox::Save) onSave();
  return ret != QMessageBox::Cancel;
}

void MainWindow::updateWindowTitle() {
  QString title = QStringLiteral("测试程序编辑器 Demo");
  if (!editor_->filePath().isEmpty())
    title += QStringLiteral(" - %1").arg(editor_->filePath());
  if (editor_->isModified()) title += QStringLiteral(" *");
  setWindowTitle(title);
}
