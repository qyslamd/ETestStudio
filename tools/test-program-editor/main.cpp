#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include "TestProgramEditorWidget.h"

namespace {

bool saveAs(etest::app::TestProgramEditorWidget* editor) {
  QString path = QFileDialog::getSaveFileName(
      editor, QStringLiteral("保存测试程序文件"), QString(),
      QStringLiteral("测试程序文件 (*.tcase);;所有文件 (*)"));
  if (path.isEmpty()) {
    return false;
  }
  if (!path.endsWith(QStringLiteral(".tcase"), Qt::CaseInsensitive)) {
    path += QStringLiteral(".tcase");
  }
  return editor->saveAs(path);
}

bool save(etest::app::TestProgramEditorWidget* editor) {
  if (editor->filePath().isEmpty()) {
    return saveAs(editor);
  }
  return editor->save();
}

bool confirmSave(etest::app::TestProgramEditorWidget* editor) {
  if (!editor->isModified()) {
    return true;
  }

  auto ret = QMessageBox::question(
      editor, QStringLiteral("未保存"),
      QStringLiteral("当前测试程序已修改，是否保存？"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
  if (ret == QMessageBox::Save) {
    return save(editor);
  }
  return ret != QMessageBox::Cancel;
}

void updateTitle(etest::app::TestProgramEditorWidget* editor) {
  QString title = QStringLiteral("测试程序编辑器");
  if (!editor->filePath().isEmpty()) {
    title += QStringLiteral(" - %1").arg(editor->filePath());
  } else {
    title += QStringLiteral(" - %1").arg(editor->displayName());
  }
  if (editor->isModified()) {
    title += QStringLiteral(" *");
  }
  editor->setWindowTitle(title);
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("test-program-editor"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("测试程序编辑器"));
  parser.addHelpOption();
  parser.addOption({{QStringLiteral("f"), QStringLiteral("file")},
                    QStringLiteral("打开指定的测试程序文件 (*.tcase)"),
                    QStringLiteral("file")});
  parser.process(app);

  QString loadFile = parser.value(QStringLiteral("file"));
  if (!loadFile.isEmpty() && !QFileInfo::exists(loadFile)) {
    QMessageBox::warning(nullptr, QStringLiteral("错误"),
                         QStringLiteral("文件不存在: %1").arg(loadFile));
    return 1;
  }

  etest::app::TestProgramEditorWidget editor(loadFile);
  editor.resize(1200, 800);

  auto* fileMenu = editor.menuBar()->addMenu(QStringLiteral("文件"));

  auto* newAction = fileMenu->addAction(QStringLiteral("新建"));
  newAction->setShortcut(QKeySequence::New);
  QObject::connect(newAction, &QAction::triggered, &editor, [&]() {
    if (!confirmSave(&editor)) {
      return;
    }
    editor.newProgram();
    editor.statusBar()->showMessage(QStringLiteral("已新建测试程序"), 3000);
    updateTitle(&editor);
  });

  auto* openAction = fileMenu->addAction(QStringLiteral("打开"));
  openAction->setShortcut(QKeySequence::Open);
  QObject::connect(openAction, &QAction::triggered, &editor, [&]() {
    if (!confirmSave(&editor)) {
      return;
    }
    QString path = QFileDialog::getOpenFileName(
        &editor, QStringLiteral("打开测试程序文件"), QString(),
        QStringLiteral("测试程序文件 (*.tcase);;所有文件 (*)"));
    if (path.isEmpty()) {
      return;
    }
    editor.openFile(path);
    editor.statusBar()->showMessage(QStringLiteral("已打开: %1").arg(path),
                                    3000);
    updateTitle(&editor);
  });

  fileMenu->addSeparator();

  auto* saveAction = fileMenu->addAction(QStringLiteral("保存"));
  saveAction->setShortcut(QKeySequence::Save);
  QObject::connect(saveAction, &QAction::triggered, &editor, [&]() {
    if (save(&editor)) {
      editor.statusBar()->showMessage(QStringLiteral("已保存"), 3000);
      updateTitle(&editor);
    }
  });

  auto* saveAsAction = fileMenu->addAction(QStringLiteral("另存为..."));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  QObject::connect(saveAsAction, &QAction::triggered, &editor, [&]() {
    if (saveAs(&editor)) {
      editor.statusBar()->showMessage(
          QStringLiteral("已保存: %1").arg(editor.filePath()), 3000);
      updateTitle(&editor);
    }
  });

  fileMenu->addSeparator();

  auto* exitAction = fileMenu->addAction(QStringLiteral("退出"));
  exitAction->setShortcut(QKeySequence::Quit);
  QObject::connect(exitAction, &QAction::triggered, &editor, &QWidget::close);

  auto* editMenu = editor.menuBar()->addMenu(QStringLiteral("编辑"));
  auto* undoAction = editMenu->addAction(QStringLiteral("撤销"));
  undoAction->setShortcut(QKeySequence::Undo);
  QObject::connect(undoAction, &QAction::triggered, &editor,
                   [&]() { editor.undo(); });

  auto* redoAction = editMenu->addAction(QStringLiteral("重做"));
  redoAction->setShortcut(QKeySequence::Redo);
  QObject::connect(redoAction, &QAction::triggered, &editor,
                   [&]() { editor.redo(); });

  updateTitle(&editor);
  QObject::connect(&editor,
                   &etest::app::TestProgramEditorWidget::modificationChanged,
                   &editor, [&]() { updateTitle(&editor); });
  QObject::connect(&editor, &etest::app::TestProgramEditorWidget::editorIdChanged,
                   &editor, [&]() { updateTitle(&editor); });

  editor.show();
  return app.exec();
}
