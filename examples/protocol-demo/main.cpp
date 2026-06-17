#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>

#include "ThemeManager.h"
#include "protocol/ProtocalEditorWidget.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("protocal-demo"));

  // ── 命令行参数 ──
  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("帧协议编辑器演示程序"));
  parser.addHelpOption();
  parser.addOption({{QStringLiteral("f"), QStringLiteral("file")},
                    QStringLiteral("打开指定的协议文件 (*.eproto)"),
                    QStringLiteral("file")});
  parser.process(app);

  QString loadFile = parser.value(QStringLiteral("file"));

  // 使用 light 主题（demo 不依赖持久化配置）
  etest::app::ThemeManager::instance().setTheme(QStringLiteral("default"));

  etest::protocal::ProtocalEditorWidget editor;
  editor.resize(1200, 800);

  // 命令行指定文件则直接加载
  if (!loadFile.isEmpty()) {
    if (!QFileInfo::exists(loadFile)) {
      QMessageBox::warning(nullptr, QStringLiteral("错误"),
                           QStringLiteral("文件不存在: %1").arg(loadFile));
      return 1;
    }
    editor.setEditorId(loadFile);
  }

  // standalone 模式：添加文件/编辑菜单
  auto* fileMenu = editor.menuBar()->addMenu(QStringLiteral("文件"));

  auto* openAction = fileMenu->addAction(QStringLiteral("打开"));
  openAction->setShortcut(QKeySequence::Open);
  QObject::connect(openAction, &QAction::triggered, &editor, [&]() {
    QString path = QFileDialog::getOpenFileName(
        &editor, QStringLiteral("打开协议文件"), QString(),
        QStringLiteral("协议文件 (*.eproto *.epro);;所有文件 (*)"));
    if (path.isEmpty())
      return;
    editor.setEditorId(path);
  });

  fileMenu->addSeparator();

  auto* saveAction = fileMenu->addAction(QStringLiteral("保存"));
  saveAction->setShortcut(QKeySequence::Save);
  QObject::connect(saveAction, &QAction::triggered, &editor, [&]() {
    if (editor.filePath().isEmpty()) {
      QString path = QFileDialog::getSaveFileName(
          &editor, QStringLiteral("保存协议文件"), QString(),
          QStringLiteral("协议文件 (*.eproto);;所有文件 (*)"));
      if (path.isEmpty())
        return;
      if (!path.endsWith(QStringLiteral(".eproto"), Qt::CaseInsensitive))
        path += QStringLiteral(".eproto");
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
        QStringLiteral("协议文件 (*.eproto);;所有文件 (*)"));
    if (path.isEmpty())
      return;
    if (!path.endsWith(QStringLiteral(".eproto"), Qt::CaseInsensitive))
      path += QStringLiteral(".eproto");
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

  // 窗口标题
  auto updateTitle = [&editor]() {
    QString title = QStringLiteral("帧协议编辑器 Demo");
    if (!editor.filePath().isEmpty())
      title += QStringLiteral(" - %1").arg(editor.filePath());
    if (editor.isModified())
      title += QStringLiteral(" *");
    editor.setWindowTitle(title);
  };
  updateTitle();
  QObject::connect(&editor,
                   &etest::protocal::ProtocalEditorWidget::modificationChanged,
                   &editor, updateTitle);

  editor.show();
  return app.exec();
}
