#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>

#include "ThemeManager.h"
#include "protocol/ProtocolEditorWidget.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("protocol-editor"));

  // ── 命令行参数 ──
  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("帧协议编辑器"));
  parser.addHelpOption();
  parser.addOption({{QStringLiteral("f"), QStringLiteral("file")},
                    QStringLiteral("打开指定的协议文件 (*.eprotox)"),
                    QStringLiteral("file")});
  parser.process(app);

  QString loadFile = parser.value(QStringLiteral("file"));

  etest::core_ui::ThemeManager::instance().setTheme(QStringLiteral("default"));

  etest::protocol::ProtocolEditorWidget editor;
  editor.resize(1200, 800);

  if (!loadFile.isEmpty()) {
    if (!QFileInfo::exists(loadFile)) {
      QMessageBox::warning(nullptr, QStringLiteral("错误"),
                           QStringLiteral("文件不存在: %1").arg(loadFile));
      return 1;
    }
    editor.openFile(loadFile);
  }

  auto* fileMenu = editor.menuBar()->addMenu(QStringLiteral("文件"));

  auto* openAction = fileMenu->addAction(QStringLiteral("打开"));
  openAction->setShortcut(QKeySequence::Open);
  QObject::connect(openAction, &QAction::triggered, &editor, [&]() {
    QString path = QFileDialog::getOpenFileName(
        &editor, QStringLiteral("打开协议文件"), QString(),
        QStringLiteral("协议文件 (*.eprotox *.eproto);;所有文件 (*)"));
    if (path.isEmpty())
      return;
    editor.openFile(path);
  });

  fileMenu->addSeparator();

  auto* saveAction = fileMenu->addAction(QStringLiteral("保存"));
  saveAction->setShortcut(QKeySequence::Save);
  QObject::connect(saveAction, &QAction::triggered, &editor, [&]() {
    if (editor.filePath().isEmpty()) {
      QString path = QFileDialog::getSaveFileName(
          &editor, QStringLiteral("保存协议文件"), QString(),
          QStringLiteral("协议文件 (*.eprotox);;所有文件 (*)"));
      if (path.isEmpty())
        return;
      if (!path.endsWith(QStringLiteral(".eprotox"), Qt::CaseInsensitive) &&
          !path.endsWith(QStringLiteral(".eproto"), Qt::CaseInsensitive))
        path += QStringLiteral(".eprotox");
      editor.saveAs(path);
    } else {
      editor.save();
    }
  });

  auto* saveAsAction = fileMenu->addAction(QStringLiteral("另存为..."));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  QObject::connect(saveAsAction, &QAction::triggered, &editor, [&]() {
    QString path = QFileDialog::getSaveFileName(
        &editor, QStringLiteral("保存协议文件"), QString(),
        QStringLiteral("协议文件 (*.eprotox);;所有文件 (*)"));
    if (path.isEmpty())
      return;
    if (!path.endsWith(QStringLiteral(".eprotox"), Qt::CaseInsensitive) &&
        !path.endsWith(QStringLiteral(".eproto"), Qt::CaseInsensitive))
      path += QStringLiteral(".eprotox");
    editor.saveAs(path);
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

  auto updateTitle = [&editor]() {
    QString title = QStringLiteral("帧协议编辑器");
    if (!editor.filePath().isEmpty())
      title += QStringLiteral(" - %1").arg(editor.filePath());
    if (editor.isModified())
      title += QStringLiteral(" *");
    editor.setWindowTitle(title);
  };
  updateTitle();
  QObject::connect(&editor,
                   &etest::protocol::ProtocolEditorWidget::modificationChanged,
                   &editor, updateTitle);

  editor.show();
  return app.exec();
}
