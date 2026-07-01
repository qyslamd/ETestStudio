#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMessageBox>

#include "topology/TopologyDocument.h"
#include "topology/TopologyEditorWidget.h"
#include "topology/TopologyJsonSerializer.h"
#include "ThemeManager.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("topology-demo"));

  // ── 命令行参数 ──
  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("拓扑编辑器演示程序"));
  parser.addHelpOption();
  parser.addOption(
      {{QStringLiteral("f"), QStringLiteral("file")},
       QStringLiteral("打开指定的拓扑文件 (*.etopo)"),
       QStringLiteral("file")});
  parser.process(app);

  QString loadFile = parser.value(QStringLiteral("file"));

  // 使用 light 主题（demo 不依赖持久化配置）
  etest::app::ThemeManager::instance().setTheme(QStringLiteral("default"));

  etest::topology::TopologyEditorWidget editor;
  editor.resize(1200, 800);

  // 命令行指定文件则直接加载
  if (!loadFile.isEmpty()) {
    if (!QFileInfo::exists(loadFile)) {
      QMessageBox::warning(nullptr, QStringLiteral("错误"),
          QStringLiteral("文件不存在: %1").arg(loadFile));
      return 1;
    }
    editor.openFile(loadFile);
  } else {
    editor.openFile(QString());
  }

  // standalone 模式：添加文件/编辑菜单
  auto* fileMenu = editor.menuBar()->addMenu(QStringLiteral("文件"));

  auto* newAction = fileMenu->addAction(QStringLiteral("新建"));
  newAction->setShortcut(QKeySequence::New);
  QObject::connect(newAction, &QAction::triggered, &editor, [&]() {
    if (editor.isModified()) {
      auto ret = QMessageBox::question(&editor, QStringLiteral("未保存"),
          QStringLiteral("当前拓扑已修改，是否保存？"),
          QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
      if (ret == QMessageBox::Save)
        editor.save();
      if (ret == QMessageBox::Cancel) return;
    }
    editor.document()->clear();
    editor.reloadScene();
    editor.openFile(QString());
  });

  auto* openAction = fileMenu->addAction(QStringLiteral("打开"));
  openAction->setShortcut(QKeySequence::Open);
  QObject::connect(openAction, &QAction::triggered, &editor, [&]() {
    QString path = QFileDialog::getOpenFileName(&editor,
        QStringLiteral("打开拓扑文件"), QString(),
        QStringLiteral("拓扑文件 (*.etopo);;所有文件 (*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      QMessageBox::warning(&editor, QStringLiteral("错误"),
          QStringLiteral("无法打开文件: %1").arg(path));
      return;
    }
    QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!jdoc.isObject()) {
      QMessageBox::warning(&editor, QStringLiteral("错误"),
          QStringLiteral("无效的拓扑文件"));
      return;
    }
    auto* doc = editor.document();
    doc->clear();
    etest::topology::TopologyJsonSerializer::deserialize(jdoc.object(), doc);
    editor.reloadScene();
    editor.openFile(path);
    if (doc->undoStack()) doc->undoStack()->clear();
  });

  fileMenu->addSeparator();

  auto* saveAction = fileMenu->addAction(QStringLiteral("保存"));
  saveAction->setShortcut(QKeySequence::Save);
  QObject::connect(saveAction, &QAction::triggered, &editor, [&]() {
    if (editor.filePath().isEmpty()) {
      QString path = QFileDialog::getSaveFileName(&editor,
          QStringLiteral("保存拓扑文件"), QString(),
          QStringLiteral("拓扑文件 (*.etopo);;所有文件 (*)"));
      if (path.isEmpty()) return;
      if (!path.endsWith(QStringLiteral(".etopo"), Qt::CaseInsensitive))
        path += QStringLiteral(".etopo");
      editor.saveAs(path);
    } else {
      editor.save();
    }
  });

  auto* saveAsAction = fileMenu->addAction(QStringLiteral("另存为..."));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  QObject::connect(saveAsAction, &QAction::triggered, &editor, [&]() {
    QString path = QFileDialog::getSaveFileName(&editor,
        QStringLiteral("保存拓扑文件"), QString(),
        QStringLiteral("拓扑文件 (*.etopo);;所有文件 (*)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".etopo"), Qt::CaseInsensitive))
      path += QStringLiteral(".etopo");
    editor.saveAs(path);
  });

  fileMenu->addSeparator();

  auto* exitAction = fileMenu->addAction(QStringLiteral("退出"));
  exitAction->setShortcut(QKeySequence::Quit);
  QObject::connect(exitAction, &QAction::triggered, &editor, &QWidget::close);

  auto* editMenu = editor.menuBar()->addMenu(QStringLiteral("编辑"));
  auto* undoAction = editMenu->addAction(QStringLiteral("撤销"));
  undoAction->setShortcut(QKeySequence::Undo);
  QObject::connect(undoAction, &QAction::triggered, &editor, [&]() { editor.undo(); });

  auto* redoAction = editMenu->addAction(QStringLiteral("重做"));
  redoAction->setShortcut(QKeySequence::Redo);
  QObject::connect(redoAction, &QAction::triggered, &editor, [&]() { editor.redo(); });

  // 窗口标题
  auto updateTitle = [&editor]() {
    QString title = QStringLiteral("拓扑编辑器 Demo");
    if (!editor.filePath().isEmpty())
      title += QStringLiteral(" - %1").arg(editor.filePath());
    if (editor.isModified())
      title += QStringLiteral(" *");
    editor.setWindowTitle(title);
  };
  updateTitle();
  QObject::connect(&editor, &etest::topology::TopologyEditorWidget::modificationChanged,
      &editor, updateTitle);
  QObject::connect(&editor, &etest::topology::TopologyEditorWidget::editorTitleChanged,
      &editor, updateTitle);

  editor.show();
  return app.exec();
}
