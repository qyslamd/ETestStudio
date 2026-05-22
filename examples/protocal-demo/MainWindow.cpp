#include "MainWindow.h"

#include <QFileDialog>
#include <QMenuBar>
#include <QStatusBar>

#include "protocal/ProtocalEditorWidget.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  editor_ = new etest::protocal::ProtocalEditorWidget(this);
  setCentralWidget(editor_);
  createMenus();
  updateWindowTitle();

  connect(editor_, &etest::protocal::ProtocalEditorWidget::modificationChanged,
          this, [this]() { updateWindowTitle(); });
}

void MainWindow::createMenus() {
  // ── 文件菜单 ──
  auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件"));

  auto* openAction = fileMenu->addAction(QStringLiteral("打开..."));
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, [this]() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开协议文件"), QString(),
        QStringLiteral("协议文件 (*.epro);;所有文件 (*)"));
    if (path.isEmpty()) return;
    editor_->setEditorId(path);
    updateWindowTitle();
    statusBar()->showMessage(QStringLiteral("已打开: %1").arg(path), 3000);
  });

  fileMenu->addSeparator();

  auto* saveAction = fileMenu->addAction(QStringLiteral("保存"));
  saveAction->setShortcut(QKeySequence::Save);
  connect(saveAction, &QAction::triggered, this, [this]() {
    editor_->save();
    statusBar()->showMessage(QStringLiteral("已保存"), 3000);
  });

  auto* saveAsAction = fileMenu->addAction(QStringLiteral("另存为..."));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  connect(saveAsAction, &QAction::triggered, this, [this]() {
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存协议文件"), QString(),
        QStringLiteral("协议文件 (*.epro);;所有文件 (*)"));
    if (path.isEmpty()) return;
    editor_->saveAs(path);
    statusBar()->showMessage(QStringLiteral("已保存: %1").arg(path), 3000);
  });

  fileMenu->addSeparator();

  auto* exitAction = fileMenu->addAction(QStringLiteral("退出"));
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
}

void MainWindow::updateWindowTitle() {
  QString title = QStringLiteral("帧协议编辑器 Demo");
  if (!editor_->filePath().isEmpty())
    title += QStringLiteral(" - %1").arg(editor_->filePath());
  setWindowTitle(title);
}
