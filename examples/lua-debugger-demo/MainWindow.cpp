#include "MainWindow.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  debugger_ = new LuaDebugger(this);
  setupUi();
  setupToolbar();
  setupEditorPanel();
  setupDebugPanels();
  setupConnections();
  setDefaultScript();
  updateButtonStates();
  resize(1200, 750);
  setWindowTitle("Lua Debugger Demo (QScintilla + sol2 + Lua 5.4)");
}

MainWindow::~MainWindow() {
  debugger_->stop();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (debugger_->state() == LuaDebugger::Running ||
      debugger_->state() == LuaDebugger::Paused) {
    auto ret = QMessageBox::question(this, "确认退出",
                                     "Lua 脚本正在执行中，确定要退出吗？",
                                     QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      event->ignore();
      return;
    }
    debugger_->stop();
  }
  event->accept();
}

void MainWindow::setupUi() {
  mainSplitter_ = new QSplitter(Qt::Horizontal, this);
  setCentralWidget(mainSplitter_);

  statusLabel_ = new QLabel("就绪");
  statusBar()->addWidget(statusLabel_);
}

void MainWindow::setupToolbar() {
  toolbar_ = addToolBar("Debug");
  toolbar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  actRun_ =
      toolbar_->addAction(style()->standardIcon(QStyle::SP_MediaPlay), "运行");
  actPause_ =
      toolbar_->addAction(style()->standardIcon(QStyle::SP_MediaPause), "暂停");
  actStop_ =
      toolbar_->addAction(style()->standardIcon(QStyle::SP_MediaStop), "停止");
  toolbar_->addSeparator();
  actStepInto_ = toolbar_->addAction("Step Into");
  actStepOver_ = toolbar_->addAction("Step Over");
  actStepOut_ = toolbar_->addAction("Step Out");
  toolbar_->addSeparator();
  actReset_ = toolbar_->addAction("重置");
}

void MainWindow::setupEditorPanel() {
  editor_ = new LuaEditor(this);
  editor_->setMinimumWidth(500);
  mainSplitter_->addWidget(editor_);
}

void MainWindow::setupDebugPanels() {
  debugTabs_ = new QTabWidget(this);

  varTree_ = new QTreeWidget(this);
  varTree_->setHeaderLabels({"变量名", "值", "类型"});
  varTree_->setAlternatingRowColors(true);
  varTree_->setRootIsDecorated(true);
  debugTabs_->addTab(varTree_, "变量");

  callStack_ = new QListWidget(this);
  debugTabs_->addTab(callStack_, "调用栈");

  output_ = new QTextEdit(this);
  output_->setReadOnly(true);
  output_->setFont(QFont("Consolas", 10));
  debugTabs_->addTab(output_, "输出");

  debugTabs_->setMinimumWidth(350);
  mainSplitter_->addWidget(debugTabs_);
  mainSplitter_->setStretchFactor(0, 3);
  mainSplitter_->setStretchFactor(1, 2);
}

void MainWindow::setupConnections() {
  connect(actRun_, &QAction::triggered, this, &MainWindow::onRun);
  connect(actPause_, &QAction::triggered, this, &MainWindow::onPause);
  connect(actStop_, &QAction::triggered, this, &MainWindow::onStop);
  connect(actStepInto_, &QAction::triggered, this, &MainWindow::onStepInto);
  connect(actStepOver_, &QAction::triggered, this, &MainWindow::onStepOver);
  connect(actStepOut_, &QAction::triggered, this, &MainWindow::onStepOut);
  connect(actReset_, &QAction::triggered, this, &MainWindow::onReset);

  connect(editor_, &LuaEditor::breakpointToggled, this,
          &MainWindow::onBreakpointToggled);
  connect(callStack_, &QListWidget::currentRowChanged, this,
          &MainWindow::onStackFrameClicked);

  connect(debugger_, &LuaDebugger::started, this, [this]() {
    statusLabel_->setText("运行中...");
    updateButtonStates();
  });
  connect(debugger_, &LuaDebugger::lineChanged, this,
          &MainWindow::onLineChanged);
  connect(debugger_, &LuaDebugger::paused, this, &MainWindow::onPaused);
  connect(debugger_, &LuaDebugger::finished, this, &MainWindow::onFinished);
  connect(debugger_, &LuaDebugger::error, this, &MainWindow::onError);
  connect(debugger_, &LuaDebugger::output, this, &MainWindow::onOutput);
}

void MainWindow::updateButtonStates() {
  auto state = debugger_->state();
  bool idle = (state == LuaDebugger::Idle);
  bool running = (state == LuaDebugger::Running);
  bool paused = (state == LuaDebugger::Paused);
  bool finished = (state == LuaDebugger::Finished);

  actRun_->setEnabled(idle || finished);
  actPause_->setEnabled(running);
  actStop_->setEnabled(running || paused);
  actStepInto_->setEnabled(paused);
  actStepOver_->setEnabled(paused);
  actStepOut_->setEnabled(paused);
  actReset_->setEnabled(finished || paused);
}

void MainWindow::setDefaultScript() {
  editor_->setText(R"RAW(-- Lua Debugger Demo
-- 在左侧边栏点击设置断点，然后点击"运行"
-- 使用 Step Into/Over/Out 单步调试

SetDevice("温度", 25.0)
Delay(100)

local target_temp = 37.5
SetDevice("温度", target_temp)

local ok = VerifyDevice("温度", target_temp, {min = -0.1, max = 0.1})
if ok then
    Log("温度验证通过")
else
    Log("温度验证失败")
end

for i = 1, 3 do
    SetDevice("加热器", i)
    Delay(50)
    local status = "running"
    Log("加热器档位: " .. status .. " " .. i)
end

InjectFault("温度", {type = "stuck_at", value = 999})
Log("故障已注入")

ClearFault("温度")
Log("故障已清除")

SetRecord(true)
UserAction("请观察指示灯状态")
TakePhoto()
Log("测试完成")
)RAW");
}

void MainWindow::onRun() {
  QSet<int> bps;
  for (int line : editor_->breakpointLines())
    bps.insert(line + 1);
  debugger_->setBreakpoints(bps);
  debugger_->loadScript(editor_->text());
  editor_->clearExecutionLine();
  output_->clear();
  varTree_->clear();
  callStack_->clear();
  debugger_->run();
}

void MainWindow::onPause() {
  debugger_->pause();
}

void MainWindow::onStop() {
  debugger_->stop();
  editor_->clearExecutionLine();
  statusLabel_->setText("已停止");
  updateButtonStates();
}

void MainWindow::onStepInto() {
  debugger_->stepInto();
}

void MainWindow::onStepOver() {
  debugger_->stepOver();
}

void MainWindow::onStepOut() {
  debugger_->stepOut();
}

void MainWindow::onReset() {
  debugger_->stop();
  debugger_->loadScript(QString());
  editor_->clearAllBreakpoints();
  editor_->clearExecutionLine();
  output_->clear();
  varTree_->clear();
  callStack_->clear();
  statusLabel_->setText("就绪");
  updateButtonStates();
}

void MainWindow::onBreakpointToggled(int line, bool set) {
  output_->append(
      QString("[断点] 第 %1 行 %2").arg(line).arg(set ? "已设置" : "已清除"));
}

void MainWindow::onLineChanged(int line) {
  editor_->setExecutionLine(line);
}

void MainWindow::onPaused(const DebugSnapshot& snapshot) {
  callStack_->clear();
  for (const auto& frame : snapshot.frames) {
    QString src = frame.source;
    if (src.isEmpty() || src == "=(load)")
      src = "script.lua";
    else if (src.startsWith('@'))
      src = src.mid(1);
    QString text =
        QString("%1 (%2:%3)").arg(frame.funcName).arg(src).arg(frame.line);
    callStack_->addItem(text);
  }

  varTree_->clear();

  auto* localsRoot = new QTreeWidgetItem(varTree_, {"局部变量", "", ""});
  localsRoot->setExpanded(true);
  QFont boldFont = localsRoot->font(0);
  boldFont.setBold(true);
  localsRoot->setFont(0, boldFont);

  if (!snapshot.frames.isEmpty()) {
    const auto& topFrame = snapshot.frames.first();
    for (auto it = topFrame.locals.begin(); it != topFrame.locals.end(); ++it) {
      auto* item = new QTreeWidgetItem(localsRoot);
      item->setText(0, it.key());
      QVariant val = it.value();
      if (val.type() == QVariant::Map) {
        item->setText(1, "{...}");
        item->setText(2, "table");
        auto map = val.toMap();
        for (auto mit = map.begin(); mit != map.end(); ++mit) {
          auto* child = new QTreeWidgetItem(item);
          child->setText(0, mit.key());
          child->setText(1, mit.value().toString());
          child->setText(2, mit.value().typeName());
        }
      } else {
        item->setText(1, val.isNull() ? "nil" : val.toString());
        item->setText(2, val.typeName());
      }
    }
  }

  auto* globalsRoot = new QTreeWidgetItem(varTree_, {"全局变量", "", ""});
  globalsRoot->setFont(0, boldFont);
  for (auto it = snapshot.globals.begin(); it != snapshot.globals.end(); ++it) {
    auto* item = new QTreeWidgetItem(globalsRoot);
    item->setText(0, it.key());
    QVariant val = it.value();
    if (val.type() == QVariant::Map) {
      item->setText(1, "{...}");
      item->setText(2, "table");
    } else {
      item->setText(1, val.isNull() ? "nil" : val.toString());
      item->setText(2, val.typeName());
    }
  }

  statusLabel_->setText(QString("已暂停 - 行 %1 | 栈深度 %2")
                            .arg(snapshot.currentLine)
                            .arg(snapshot.frames.size()));
  updateButtonStates();
}

void MainWindow::onFinished() {
  editor_->clearExecutionLine();
  statusLabel_->setText("执行完成");
  output_->append("[完成] Lua 脚本执行完毕");
  updateButtonStates();
}

void MainWindow::onError(const QString& message) {
  editor_->clearExecutionLine();
  statusLabel_->setText("执行出错");
  output_->append(QString("[错误] %1").arg(message));
  updateButtonStates();
}

void MainWindow::onOutput(const QString& text) {
  output_->append(text);
}

void MainWindow::onStackFrameClicked(int index) {
  if (index < 0)
    return;
  if (debugger_->state() != LuaDebugger::Paused)
    return;
  editor_->setExecutionLine(index);
}
