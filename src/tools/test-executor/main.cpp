/// @file main.cpp
/// @brief test-executor standalone GUI — loads .etopo / .etprog, drives
///        TestExecutionEngine, displays live results in a tree + log view.

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QCommandLineParser>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QSize>
#include <QSplitter>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <spdlog/spdlog.h>

#include "engine/StepRunner.h"
#include "engine/TestExecutionEngine.h"
#include "core/SignalRegistry.h"

#include <icd/repository.hpp>

// =============================================================================
// Forward declarations
// =============================================================================
namespace etest::engine {
struct TestStepData;
struct TestCaseData;
}  // namespace etest::engine

// =============================================================================
// Anonymous namespace: helpers
// =============================================================================
namespace {

constexpr int kToolBarIconSize = 24;

/// Convert .etprog step JSON to a TestStepData (recursive).
etest::engine::TestStepData parseStepJson(const QJsonObject& obj) {
  etest::engine::TestStepData step;
  step.command = obj.value(QStringLiteral("cmd")).toString();
  step.target = obj.value(QStringLiteral("target")).toString();
  step.timeoutMs = obj.value(QStringLiteral("timeoutMs")).toInt(5000);

  // value is stored as a string in the .etprog JSON
  QString valStr = obj.value(QStringLiteral("value")).toString();
  if (!valStr.isEmpty()) {
    bool ok = false;
    double v = valStr.toDouble(&ok);
    if (ok) {
      step.value = v;
    }
  }

  // tolerance object (optional)
  QJsonValue tolVal = obj.value(QStringLiteral("tolerance"));
  if (tolVal.isObject()) {
    QJsonObject tolObj = tolVal.toObject();
    if (tolObj.value(QStringLiteral("enabled")).toBool(false)) {
      double tmax = tolObj.value(QStringLiteral("max")).toDouble(0.0);
      step.tolerance = tmax;
    }
  }

  // delayMs → extra field (used by DELAY steps)
  int delayMs = obj.value(QStringLiteral("delayMs")).toInt(0);
  if (delayMs > 0) {
    step.extra = QString::number(delayMs);
  }

  // loop support
  step.loopCount = obj.value(QStringLiteral("loopCount")).toInt(0);

  // condition (IF / WHILE)
  QJsonValue condVal = obj.value(QStringLiteral("condition"));
  if (condVal.isObject()) {
    QJsonObject condObj = condVal.toObject();
    QString op = condObj.value(QStringLiteral("op")).toString();
    QString condTarget = condObj.value(QStringLiteral("target")).toString();
    QString condValue = condObj.value(QStringLiteral("value")).toString();

    // Condition string: "{op}{condValue}" so evaluateCondition can parse it
    if (!op.isEmpty() && !condValue.isEmpty()) {
      step.condition = op + condValue;
    } else if (!condValue.isEmpty()) {
      step.condition = QStringLiteral("==") + condValue;
    }
    // If condition has a target, store extra info in extra field
    if (!condTarget.isEmpty()) {
      if (!step.extra.isEmpty()) {
        step.extra += QStringLiteral("|target=") + condTarget;
      } else {
        step.extra = QStringLiteral("target=") + condTarget;
      }
    }
  }

  // Recursive: subSteps (LOOP/WHILE body, or IF body when then only)
  QJsonArray subArr = obj.value(QStringLiteral("subSteps")).toArray();
  for (const QJsonValue& sv : subArr) {
    step.subSteps.append(parseStepJson(sv.toObject()));
  }

  // Recursive: elseSubSteps (IF-else branch)
  QJsonArray elseArr = obj.value(QStringLiteral("elseSubSteps")).toArray();
  for (const QJsonValue& ev : elseArr) {
    step.elseSteps.append(parseStepJson(ev.toObject()));
  }

  // For IF commands, subSteps → thenSteps, elseSubSteps → elseSteps
  QString upperCmd = step.command.trimmed().toUpper();
  if (upperCmd == QStringLiteral("IF")) {
    step.thenSteps = step.subSteps;
    step.subSteps.clear();
  }

  return step;
}

/// Parse a .etprog JSON file into a ProgramData struct.
etest::engine::ProgramData parseEtProg(const QString& path) {
  etest::engine::ProgramData program;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    spdlog::error("[executor] Cannot open .etprog: {}", path.toStdString());
    return program;
  }

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  file.close();

  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    spdlog::error("[executor] Invalid .etprog JSON: {}",
                  err.errorString().toStdString());
    return program;
  }

  QJsonObject root = doc.object();

  // Suite name
  program.suiteName = root.value(QStringLiteral("name")).toString();

  // Cases
  QJsonArray casesArr = root.value(QStringLiteral("cases")).toArray();
  for (const QJsonValue& cv : casesArr) {
    QJsonObject caseObj = cv.toObject();
    etest::engine::TestCaseData tc;
    tc.caseName = caseObj.value(QStringLiteral("name")).toString();

    QJsonArray stepsArr = caseObj.value(QStringLiteral("steps")).toArray();
    for (const QJsonValue& sv : stepsArr) {
      tc.steps.append(parseStepJson(sv.toObject()));
    }

    program.cases.append(tc);
  }

  return program;
}

/// Extract device name list from a .etopo JSON root object.
QStringList extractDevicesFromTopology(const QJsonObject& root) {
  QStringList devices;
  QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
  for (const QJsonValue& val : nodes) {
    QJsonObject obj = val.toObject();
    QString name = obj.value(QStringLiteral("name")).toString();
    if (!name.isEmpty()) {
      devices.append(name);
    }
  }
  return devices;
}

/// Format a log timestamp string.
QString logTimestamp() {
  return QDateTime::currentDateTime().toString(QStringLiteral("[HH:mm:ss]"));
}

/// Convert StepStatus to display icon string.
QString statusIcon(int status) {
  switch (status) {
    case etest::engine::PASS:
      return QStringLiteral("✅");  // check mark
    case etest::engine::FAIL:
      return QStringLiteral("❌");  // cross mark
    case etest::engine::TIMEOUT:
      return QStringLiteral("⏱");  // stopwatch
    case etest::engine::ERROR:
      return QStringLiteral("⚠");  // warning
    case etest::engine::SKIPPED:
      return QStringLiteral("⏭");  // skip
    default:
      return QStringLiteral("⏳");  // hourglass (pending)
  }
}

/// Convert StepStatus to short label.
QString statusLabel(int status) {
  switch (status) {
    case etest::engine::PASS:
      return QStringLiteral("PASS");
    case etest::engine::FAIL:
      return QStringLiteral("FAIL");
    case etest::engine::TIMEOUT:
      return QStringLiteral("TIMEOUT");
    case etest::engine::ERROR:
      return QStringLiteral("ERROR");
    case etest::engine::SKIPPED:
      return QStringLiteral("SKIPPED");
    default:
      return QStringLiteral("PENDING");
  }
}

/// Store a user role key for the step path in tree items.
enum TreeItemRole {
  StepPathRole = Qt::UserRole + 1,
  CaseIndexRole
};

}  // anonymous namespace

// =============================================================================
// LogDoubleClickFilter — intercepts double-click on log to select tree item
// =============================================================================

class LogDoubleClickFilter : public QObject {
  Q_OBJECT
 public:
  using ClickCallback = std::function<void(const QString& /*stepPath*/)>;

  LogDoubleClickFilter(QPlainTextEdit* editor, ClickCallback cb,
                       QObject* parent = nullptr)
      : QObject(parent), editor_(editor), callback_(std::move(cb)) {
    if (editor_ != nullptr) {
      editor_->viewport()->installEventFilter(this);
    }
  }

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override {
    if (event->type() == QEvent::MouseButtonDblClick && editor_ != nullptr &&
        callback_) {
      auto* me = static_cast<QMouseEvent*>(event);
      QTextCursor cursor = editor_->cursorForPosition(me->pos());
      QString line = cursor.block().text();

      // Extract step path from log line format:
      //   "[HH:mm:ss] ICON STATUS path: msg"
      // The step path is between the final space before path and the ':'
      int colonIdx = line.indexOf(QStringLiteral(": "));
      if (colonIdx < 0) {
        return QObject::eventFilter(obj, event);
      }
      QString beforeColon = line.left(colonIdx);
      int lastSpace = beforeColon.lastIndexOf(QLatin1Char(' '));
      if (lastSpace >= 0) {
        QString maybePath = beforeColon.mid(lastSpace + 1).trimmed();
        if (!maybePath.isEmpty()) {
          callback_(maybePath);
        }
      }
    }
    return QObject::eventFilter(obj, event);
  }

 private:
  QPlainTextEdit* editor_;
  ClickCallback callback_;
};

// =============================================================================
// MainWindow
// =============================================================================

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow(const QString& topologyPath, const QString& icdPath,
             const QString& programPath, bool verifyOnly,
             QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void onRunClicked();
  void onPauseClicked();
  void onStopClicked();
  void onEngineStarted();
  void onEngineFinished();
  void onEngineError(const QString& message);
  void onStepStarted(int caseIndex, const QString& stepPath,
                     const QString& command, const QString& target);
  void onStepFinished(int caseIndex, const QString& stepPath,
                      const etest::engine::StepResult& result);
  void onSuiteFinished(const QString& name, int pass, int fail);

 private:
  void initUi();
  void initSignals();
  void initEngine();
  void loadFiles();
  void populateTree(const etest::engine::ProgramData& program);
  void addStepTreeItems(QTreeWidgetItem* parent,
                        const QList<etest::engine::TestStepData>& steps,
                        int caseIndex, const QString& prefix);
  void updateStats();
  void setEngineBusy(bool busy);
  void appendLog(const QString& message);
  void updateWindowTitle();
  void onLogDoubleClick(const QString& stepPath);

  // ── UI widgets ──
  QTreeWidget* stepTree_;
  QPlainTextEdit* logView_;
  QLabel* statusLabel_;      // toolbar status text
  QLabel* statsLabel_;       // pass / fail / timeout summary
  QAction* runAction_;
  QAction* pauseAction_;
  QAction* stopAction_;

  // ── Data ──
  QString topologyPath_;
  QString icdPath_;
  QString programPath_;
  bool verifyOnly_ = false;

  // ── Engine ──
  etest::core::SignalRegistry* registry_ = nullptr;
  icd::Repository* icdRepo_ = nullptr;
  etest::engine::TestExecutionEngine* engine_ = nullptr;

  // ── Program data ──
  etest::engine::ProgramData currentProgram_;
  QString topologyDeviceInfo_;

  // ── Tracking ──
  int passCount_ = 0;
  int failCount_ = 0;
  int timeoutCount_ = 0;
  QHash<QString, QTreeWidgetItem*> stepItems_;
};

// =============================================================================
// MainWindow implementation
// =============================================================================

MainWindow::MainWindow(const QString& topologyPath, const QString& icdPath,
                       const QString& programPath, bool verifyOnly,
                       QWidget* parent)
    : QMainWindow(parent),
      topologyPath_(topologyPath),
      icdPath_(icdPath),
      programPath_(programPath),
      verifyOnly_(verifyOnly) {
  setWindowTitle(QStringLiteral("测试执行引擎"));

  registry_ = new etest::core::SignalRegistry(this);
  icdRepo_ = new icd::Repository();

  initUi();
  initSignals();
  initEngine();    // engine must exist before loadFiles()
  loadFiles();
}

MainWindow::~MainWindow() {
  if (engine_ != nullptr) {
    engine_->stop();
  }
  delete icdRepo_;
}

// ── UI construction ──

void MainWindow::initUi() {
  resize(1200, 800);

  // ── Menu bar ──
  auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
  auto* exitAction = fileMenu->addAction(QStringLiteral("退出(&X)"));
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, this, &QWidget::close);

  auto* execMenu = menuBar()->addMenu(QStringLiteral("执行(&R)"));
  auto* runMenuAction = execMenu->addAction(QStringLiteral("运行"));
  runMenuAction->setShortcut(QKeySequence(QStringLiteral("F5")));
  auto* pauseMenuAction =
      execMenu->addAction(QStringLiteral("暂停"));
  pauseMenuAction->setShortcut(QKeySequence(QStringLiteral("F6")));
  auto* stopMenuAction =
      execMenu->addAction(QStringLiteral("停止"));
  stopMenuAction->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));

  connect(runMenuAction, &QAction::triggered, this, &MainWindow::onRunClicked);
  connect(pauseMenuAction, &QAction::triggered, this,
          &MainWindow::onPauseClicked);
  connect(stopMenuAction, &QAction::triggered, this,
          &MainWindow::onStopClicked);

  auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
  auto* clearLogAction =
      viewMenu->addAction(QStringLiteral("清空日志"));
  connect(clearLogAction, &QAction::triggered, this, [this]() {
    logView_->clear();
  });

  auto* helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  auto* aboutAction =
      helpMenu->addAction(QStringLiteral("关于"));
  connect(aboutAction, &QAction::triggered, this, [this]() {
    QMessageBox::about(
        this, QStringLiteral("关于"),
        QStringLiteral(
            "测试执行引擎 v1.0\n\n"
            "独立的测试程序执行器，"
            "支持 .etopo 拓扑和 "
            ".etprog 测试程序。"));
  });

  // ── Toolbar ──
  auto* toolbar = addToolBar(QStringLiteral("执行控制"));
  toolbar->setIconSize(QSize(kToolBarIconSize, kToolBarIconSize));
  toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  runAction_ =
      toolbar->addAction(QStringLiteral("▶ 运行"));
  runAction_->setToolTip(QStringLiteral("开始执行测试 (F5)"));
  connect(runAction_, &QAction::triggered, this, &MainWindow::onRunClicked);

  pauseAction_ =
      toolbar->addAction(QStringLiteral("⏸ 暂停"));
  pauseAction_->setToolTip(
      QStringLiteral("暂停执行 (F6)"));
  pauseAction_->setEnabled(false);
  connect(pauseAction_, &QAction::triggered, this,
          &MainWindow::onPauseClicked);

  stopAction_ =
      toolbar->addAction(QStringLiteral("⏹ 停止"));
  stopAction_->setToolTip(
      QStringLiteral("停止执行 (Shift+F5)"));
  stopAction_->setEnabled(false);
  connect(stopAction_, &QAction::triggered, this,
          &MainWindow::onStopClicked);

  toolbar->addSeparator();

  statusLabel_ = new QLabel(
      QStringLiteral("状态: 空闲"));
  toolbar->addWidget(statusLabel_);

  // ── Central widget: splitter ──
  auto* splitter = new QSplitter(Qt::Horizontal, this);

  // Left: step tree
  stepTree_ = new QTreeWidget(splitter);
  stepTree_->setHeaderLabels(
      {QStringLiteral("步骤"),
       QStringLiteral("命令"),
       QStringLiteral("目标")});
  stepTree_->header()->setStretchLastSection(true);
  stepTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  stepTree_->setAlternatingRowColors(true);
  stepTree_->setAnimated(true);
  stepTree_->setRootIsDecorated(true);

  // Right container: log + stats
  auto* rightPanel = new QWidget(splitter);
  auto* rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(2);

  logView_ = new QPlainTextEdit(rightPanel);
  logView_->setReadOnly(true);
  logView_->setFont(QFont(QStringLiteral("Consolas"), 10));
  logView_->setLineWrapMode(QPlainTextEdit::NoWrap);
  logView_->setObjectName(QStringLiteral("logView"));

  statsLabel_ = new QLabel(
      QStringLiteral(
          "✅ PASS: 0    ❌ FAIL: 0    ⏱ TIMEOUT: 0"),
      rightPanel);
  statsLabel_->setObjectName(QStringLiteral("statsLabel"));

  rightLayout->addWidget(logView_, 1);
  rightLayout->addWidget(statsLabel_, 0);

  splitter->addWidget(stepTree_);
  splitter->addWidget(rightPanel);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 2);

  setCentralWidget(splitter);

  // ── Status bar ──
  statusBar()->showMessage(QStringLiteral("就绪"));
}

// ── Signal connections ──

void MainWindow::initSignals() {
  auto* dblClickFilter = new LogDoubleClickFilter(
      logView_,
      [this](const QString& stepPath) { onLogDoubleClick(stepPath); },
      this);
  Q_UNUSED(dblClickFilter);
}

// ── Engine setup ──

void MainWindow::initEngine() {
  if (engine_ != nullptr) {
    return;
  }

  engine_ = new etest::engine::TestExecutionEngine(registry_, icdRepo_, this);

  connect(engine_, &etest::engine::TestExecutionEngine::engineStarted,
          this, &MainWindow::onEngineStarted);
  connect(engine_, &etest::engine::TestExecutionEngine::engineFinished,
          this, &MainWindow::onEngineFinished);
  connect(engine_, &etest::engine::TestExecutionEngine::engineError,
          this, &MainWindow::onEngineError);
  connect(engine_, &etest::engine::TestExecutionEngine::stepStarted,
          this, &MainWindow::onStepStarted);
  connect(engine_, &etest::engine::TestExecutionEngine::stepFinished,
          this, &MainWindow::onStepFinished);
  connect(engine_, &etest::engine::TestExecutionEngine::suiteFinished,
          this, &MainWindow::onSuiteFinished);
}

// ── File loading ──

void MainWindow::loadFiles() {
  // 1. Topology
  if (!topologyPath_.isEmpty()) {
    bool ok = engine_->loadTopology(topologyPath_);
    if (ok) {
      QFile f(topologyPath_);
      if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isObject()) {
          QStringList devices = extractDevicesFromTopology(doc.object());
          if (!devices.isEmpty()) {
            topologyDeviceInfo_ = devices.join(QStringLiteral(", "));
          }
        }
      }
      appendLog(QStringLiteral("[%1] %2: %3")
                    .arg(QStringLiteral("拓扑"))
                    .arg(QStringLiteral("已加载"))
                    .arg(topologyPath_));
    } else {
      appendLog(QStringLiteral("[%1] %2: %3")
                    .arg(QStringLiteral("拓扑"))
                    .arg(QStringLiteral("加载失败"))
                    .arg(topologyPath_));
      spdlog::error("[executor] Failed to load topology: {}",
                    topologyPath_.toStdString());
    }
  }

  // 2. Test program
  if (!programPath_.isEmpty()) {
    currentProgram_ = parseEtProg(programPath_);
    if (!currentProgram_.suiteName.isEmpty()) {
      engine_->setProgram(currentProgram_);
      populateTree(currentProgram_);
      appendLog(
          QStringLiteral("[%1] %2: %3 (%4 cases)")
              .arg(QStringLiteral("程序"))
              .arg(QStringLiteral("已加载"))
              .arg(programPath_)
              .arg(currentProgram_.cases.size()));
      updateWindowTitle();
    } else {
      appendLog(QStringLiteral("[%1] %2: %3")
                    .arg(QStringLiteral("程序"))
                    .arg(QStringLiteral("加载失败或为空"))
                    .arg(programPath_));
      spdlog::error("[executor] Failed to parse .etprog: {}",
                    programPath_.toStdString());
    }
  }

  // ── Update status bar with file info ──
  QStringList statusParts;
  if (!topologyPath_.isEmpty()) {
    statusParts << QStringLiteral("✅ 拓扑");
  } else {
    statusParts << QStringLiteral("❌ 拓扑");
  }
  if (!icdPath_.isEmpty()) {
    statusParts << QStringLiteral("✅ ICD");
  } else {
    statusParts << QStringLiteral("⚪ ICD");
  }
  if (!programPath_.isEmpty() && !currentProgram_.suiteName.isEmpty()) {
    statusParts << QStringLiteral("✅ 程序");
  } else {
    statusParts << QStringLiteral("❌ 程序");
  }
  statusBar()->showMessage(statusParts.join(QStringLiteral(" | ")));

  // ── Verify-only mode: show info and quit ──
  if (verifyOnly_) {
    if (!programPath_.isEmpty()) {
      int totalSteps = 0;
      for (const auto& tc : currentProgram_.cases) {
        totalSteps += tc.steps.size();
      }
      QMessageBox::information(
          this, QStringLiteral("验证结果"),
          QStringLiteral("程序: %1\n用例数: %2\n"
                         "步骤数: %3\n\n文件有效")
              .arg(currentProgram_.suiteName)
              .arg(currentProgram_.cases.size())
              .arg(totalSteps));
    }
    QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
  }
}

// ── Tree population ──

void MainWindow::populateTree(const etest::engine::ProgramData& program) {
  stepTree_->clear();
  stepItems_.clear();

  for (int i = 0; i < program.cases.size(); ++i) {
    const auto& tc = program.cases[i];
    auto* caseItem =
        new QTreeWidgetItem({tc.caseName, QString(), QString()});
    caseItem->setData(0, CaseIndexRole, i);
    caseItem->setData(0, StepPathRole, QString::number(i + 1));
    caseItem->setToolTip(
        0, QStringLiteral("用例 %1").arg(i + 1));
    stepTree_->addTopLevelItem(caseItem);

    addStepTreeItems(caseItem, tc.steps, i, QString::number(i + 1));
  }

  stepTree_->expandAll();
}

void MainWindow::addStepTreeItems(
    QTreeWidgetItem* parent,
    const QList<etest::engine::TestStepData>& steps, int caseIndex,
    const QString& prefix) {
  for (int j = 0; j < steps.size(); ++j) {
    const auto& step = steps[j];
    QString stepPath = QStringLiteral("%1.%2").arg(prefix).arg(j + 1);

    QString cmd = step.command;
    QString target = step.target;

    // Determine if this step has sub-items
    QString upperCmd = cmd.trimmed().toUpper();
    bool hasSubSteps =
        !step.subSteps.isEmpty() ||
        (upperCmd == QStringLiteral("IF") && !step.thenSteps.isEmpty());

    if (!hasSubSteps) {
      // Simple step
      auto* item = new QTreeWidgetItem(
          {QStringLiteral("  %1 %2")
               .arg(statusIcon(etest::engine::PENDING))
               .arg(cmd),
           cmd, target});
      item->setData(0, StepPathRole, stepPath);
      item->setData(0, CaseIndexRole, caseIndex);
      item->setToolTip(
          0, QStringLiteral("路径: %1").arg(stepPath));
      parent->addChild(item);
      stepItems_[stepPath] = item;
    } else {
      // Step with sub-items (LOOP / WHILE / IF)
      auto* stepItem = new QTreeWidgetItem(
          {QStringLiteral("  %1 %2")
               .arg(statusIcon(etest::engine::PENDING))
               .arg(cmd),
           cmd, target});
      stepItem->setData(0, StepPathRole, stepPath);
      stepItem->setData(0, CaseIndexRole, caseIndex);
      stepItem->setToolTip(
          0, QStringLiteral("路径: %1 (含子步骤)")
                 .arg(stepPath));
      parent->addChild(stepItem);
      stepItems_[stepPath] = stepItem;

      // Sub-steps (LOOP/WHILE body)
      for (int k = 0; k < step.subSteps.size(); ++k) {
        QString subPath = QStringLiteral("%1.%2").arg(stepPath).arg(k + 1);
        const auto& sub = step.subSteps[k];
        auto* subItem = new QTreeWidgetItem(
            {sub.command, sub.command, sub.target});
        subItem->setData(0, StepPathRole, subPath);
        subItem->setData(0, CaseIndexRole, caseIndex);
        subItem->setToolTip(
            0, QStringLiteral("路径: %1").arg(subPath));
        stepItem->addChild(subItem);
        stepItems_[subPath] = subItem;

        // Recursive sub-steps of sub-step
        if (!sub.subSteps.isEmpty() || !sub.thenSteps.isEmpty() ||
            !sub.elseSteps.isEmpty()) {
          addStepTreeItems(subItem, sub.subSteps, caseIndex, subPath);
        }
        if (!sub.thenSteps.isEmpty()) {
          addStepTreeItems(subItem, sub.thenSteps, caseIndex,
                           subPath + QStringLiteral(".THEN"));
        }
        if (!sub.elseSteps.isEmpty()) {
          addStepTreeItems(subItem, sub.elseSteps, caseIndex,
                           subPath + QStringLiteral(".ELSE"));
        }
      }

      // IF-then branch
      for (int k = 0; k < step.thenSteps.size(); ++k) {
        QString thenPath =
            QStringLiteral("%1.THEN.%2").arg(stepPath).arg(k + 1);
        const auto& ts = step.thenSteps[k];
        auto* thenItem = new QTreeWidgetItem(
            {ts.command, ts.command, ts.target});
        thenItem->setData(0, StepPathRole, thenPath);
        thenItem->setData(0, CaseIndexRole, caseIndex);
        thenItem->setToolTip(
            0, QStringLiteral("[THEN] 路径: %1").arg(thenPath));
        stepItem->addChild(thenItem);
        stepItems_[thenPath] = thenItem;
      }

      // IF-else branch
      for (int k = 0; k < step.elseSteps.size(); ++k) {
        QString elsePath =
            QStringLiteral("%1.ELSE.%2").arg(stepPath).arg(k + 1);
        const auto& es = step.elseSteps[k];
        auto* elseItem = new QTreeWidgetItem(
            {es.command, es.command, es.target});
        elseItem->setData(0, StepPathRole, elsePath);
        elseItem->setData(0, CaseIndexRole, caseIndex);
        elseItem->setToolTip(
            0, QStringLiteral("[ELSE] 路径: %1").arg(elsePath));
        stepItem->addChild(elseItem);
        stepItems_[elsePath] = elseItem;
      }

      stepItem->setExpanded(true);
    }
  }
}

// ── Stats update ──

void MainWindow::updateStats() {
  statsLabel_->setText(
      QStringLiteral(
          "✅ PASS: %1    ❌ FAIL: %2    ⏱ TIMEOUT: %3")
          .arg(passCount_)
          .arg(failCount_)
          .arg(timeoutCount_));
}

// ── Busy state ──

void MainWindow::setEngineBusy(bool busy) {
  runAction_->setEnabled(!busy);
  pauseAction_->setEnabled(busy);
  stopAction_->setEnabled(busy);
  if (busy) {
    statusLabel_->setText(
        QStringLiteral("状态: 运行中"));
  } else {
    statusLabel_->setText(
        QStringLiteral("状态: 空闲"));
  }
}

// ── Log ──

void MainWindow::appendLog(const QString& message) {
  logView_->appendPlainText(message);
  // Auto-scroll to bottom
  QTextCursor cursor = logView_->textCursor();
  cursor.movePosition(QTextCursor::End);
  logView_->setTextCursor(cursor);
}

// ── Window title ──

void MainWindow::updateWindowTitle() {
  QString title =
      QStringLiteral("测试执行引擎");
  if (!currentProgram_.suiteName.isEmpty()) {
    title += QStringLiteral(" - %1").arg(currentProgram_.suiteName);
  }
  setWindowTitle(title);
}

// ── Double-click log → select tree item ──

void MainWindow::onLogDoubleClick(const QString& stepPath) {
  auto it = stepItems_.constFind(stepPath);
  if (it != stepItems_.constEnd()) {
    stepTree_->setCurrentItem(it.value());
    stepTree_->scrollToItem(it.value());
  }
}

// =============================================================================
// Slots
// =============================================================================

void MainWindow::onRunClicked() {
  if (engine_ == nullptr) {
    initEngine();
  }

  // Re-set the program in case it was changed
  engine_->setProgram(currentProgram_);

  // Reset counters
  passCount_ = 0;
  failCount_ = 0;
  timeoutCount_ = 0;
  updateStats();

  // Clear previous step icons (reset to PENDING)
  for (auto it = stepItems_.begin(); it != stepItems_.end(); ++it) {
    QTreeWidgetItem* item = it.value();
    QString text = item->text(0);
    // Replace the first icon part
    int spaceIdx = text.indexOf(QLatin1Char(' '));
    if (spaceIdx >= 0) {
      item->setText(
          0, statusIcon(etest::engine::PENDING) + text.mid(spaceIdx));
    }
    item->setBackground(0, QBrush());
  }

  appendLog(
      QStringLiteral("[%1] %2")
          .arg(QStringLiteral("系统"))
          .arg(QStringLiteral("开始执行测试...")));

  engine_->start();
}

void MainWindow::onPauseClicked() {
  if (engine_ == nullptr) {
    return;
  }
  etest::engine::EngineState state = engine_->state();
  if (state == etest::engine::EngineState::Running) {
    engine_->pause();
    statusLabel_->setText(
        QStringLiteral("状态: 已暂停"));
    appendLog(
        QStringLiteral("[%1] %2")
            .arg(QStringLiteral("系统"))
            .arg(QStringLiteral("执行已暂停")));
  } else if (state == etest::engine::EngineState::Paused) {
    engine_->resume();
    statusLabel_->setText(
        QStringLiteral("状态: 运行中"));
    appendLog(
        QStringLiteral("[%1] %2")
            .arg(QStringLiteral("系统"))
            .arg(QStringLiteral("执行已恢复")));
  }
}

void MainWindow::onStopClicked() {
  if (engine_ == nullptr) {
    return;
  }
  engine_->stop();
  appendLog(
      QStringLiteral("[%1] %2")
          .arg(QStringLiteral("系统"))
          .arg(QStringLiteral("执行已停止")));
  setEngineBusy(false);
}

void MainWindow::onEngineStarted() {
  appendLog(
      QStringLiteral("[%1] %2")
          .arg(QStringLiteral("系统"))
          .arg(QStringLiteral("引擎已启动")));
  setEngineBusy(true);
}

void MainWindow::onEngineFinished() {
  appendLog(
      QStringLiteral("[%1] %2")
          .arg(QStringLiteral("系统"))
          .arg(QStringLiteral("执行完成")));
  setEngineBusy(false);
}

void MainWindow::onEngineError(const QString& message) {
  appendLog(
      QStringLiteral("[%1] %2")
          .arg(QStringLiteral("错误"))
          .arg(message));
  setEngineBusy(false);
}

void MainWindow::onStepStarted(int caseIndex, const QString& stepPath,
                                const QString& command,
                                const QString& target) {
  Q_UNUSED(caseIndex);
  Q_UNUSED(target);

  // Highlight tree item if found; dynamic iteration paths are logged only
  auto it = stepItems_.find(stepPath);
  if (it != stepItems_.end()) {
    it.value()->setBackground(
        0, QBrush(QColor(255, 255, 200)));  // yellow highlight
    stepTree_->scrollToItem(it.value());
    stepTree_->setCurrentItem(it.value());
  }

  appendLog(QStringLiteral("%1 %2 %3: %4")
                .arg(logTimestamp())
                .arg(statusIcon(etest::engine::PENDING))
                .arg(command)
                .arg(stepPath));
}

void MainWindow::onStepFinished(
    int caseIndex, const QString& stepPath,
    const etest::engine::StepResult& result) {
  Q_UNUSED(caseIndex);

  // Update tree item
  auto it = stepItems_.find(stepPath);
  if (it != stepItems_.end()) {
    QString text = it.value()->text(0);
    int spaceIdx = text.indexOf(QLatin1Char(' '));
    if (spaceIdx >= 0) {
      it.value()->setText(
          0, statusIcon(result.status) + text.mid(spaceIdx));
    }
    // Color code by result
    it.value()->setBackground(0, QBrush());  // reset highlight
    if (result.status == etest::engine::FAIL) {
      it.value()->setBackground(
          0, QBrush(QColor(255, 200, 200)));
    } else if (result.status == etest::engine::TIMEOUT) {
      it.value()->setBackground(
          0, QBrush(QColor(255, 230, 180)));
    } else if (result.status == etest::engine::ERROR) {
      it.value()->setBackground(
          0, QBrush(QColor(255, 180, 180)));
    }
  }

  // Update counters
  if (result.status == etest::engine::PASS) {
    ++passCount_;
  } else if (result.status == etest::engine::FAIL) {
    ++failCount_;
  } else if (result.status == etest::engine::TIMEOUT) {
    ++timeoutCount_;
  }
  updateStats();

  // Log entry
  QString msg = result.message;
  if (msg.isEmpty()) {
    msg = QStringLiteral("OK");
  }
  appendLog(QStringLiteral("%1 %2 %3 %4 (%5ms) - %6")
                .arg(logTimestamp())
                .arg(statusIcon(result.status))
                .arg(statusLabel(result.status))
                .arg(stepPath)
                .arg(result.elapsedMs)
                .arg(msg));
}

void MainWindow::onSuiteFinished(const QString& name, int pass, int fail) {
  appendLog(QStringLiteral("[%1] %2: %3  PASS: %4  FAIL: %5")
                .arg(QStringLiteral("完成"))
                .arg(QStringLiteral("套件"))
                .arg(name)
                .arg(pass)
                .arg(fail));
}

// =============================================================================
// main
// =============================================================================

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("test-executor"));
  app.setApplicationVersion(QStringLiteral("1.0.0"));

  // ── Command line ──
  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("测试执行引擎 - "
                     "独立的测试程序执行器"));
  parser.addHelpOption();
  parser.addVersionOption();

  parser.addOption(
      {{QStringLiteral("t"), QStringLiteral("topology")},
       QStringLiteral("拓扑文件路径 (*.etopo)"),
       QStringLiteral("path")});
  parser.addOption(
      {{QStringLiteral("i"), QStringLiteral("icd")},
       QStringLiteral("ICD 文件路径 (*.eproto)"),
       QStringLiteral("path")});
  parser.addOption(
      {{QStringLiteral("p"), QStringLiteral("program")},
       QStringLiteral(
           "测试程序文件路径 (*.etprog)"),
       QStringLiteral("path")});
  parser.addOption(
      {{QStringLiteral("verify-only")},
       QStringLiteral("仅验证文件，不执行")});

  parser.process(app);

  QString topologyPath = parser.value(QStringLiteral("topology"));
  QString icdPath = parser.value(QStringLiteral("icd"));
  QString programPath = parser.value(QStringLiteral("program"));
  bool verifyOnly = parser.isSet(QStringLiteral("verify-only"));

  // ── Validate ──
  auto checkFile = [](const QString& path, const QString& label) -> bool {
    if (path.isEmpty()) {
      return true;  // optional
    }
    if (!QFileInfo::exists(path)) {
      QMessageBox::warning(
          nullptr, QStringLiteral("错误"),
          QStringLiteral("%1 文件不存在: %2")
              .arg(label).arg(path));
      return false;
    }
    return true;
  };

  if (!checkFile(topologyPath,
                 QStringLiteral("拓扑"))) {
    return 1;
  }
  if (!checkFile(icdPath, QStringLiteral("ICD"))) {
    return 1;
  }
  if (!checkFile(programPath,
                 QStringLiteral("程序"))) {
    return 1;
  }

  // ── Window ──
  MainWindow window(topologyPath, icdPath, programPath, verifyOnly);

  if (!verifyOnly) {
    window.show();
    return app.exec();
  }

  // verifyOnly: MainWindow triggered close() via QMetaObject::invokeMethod
  // after showing the validation dialog; run a short event loop to process it
  if (verifyOnly) {
    QTimer::singleShot(100, &app, &QApplication::quit);
    return app.exec();
  }

  return app.exec();
}

// Include moc for Q_OBJECT in .cpp file
#include "main.moc"
