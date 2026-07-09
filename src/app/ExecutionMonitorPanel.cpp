#include "ExecutionMonitorPanel.h"

#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "engine/HardwareManager.h"
#include "engine/TestExecutionEngine.h"

namespace etest::app {

ExecutionMonitorPanel::ExecutionMonitorPanel(QWidget* parent)
    : QWidget(parent) {
  initUi();
}

void ExecutionMonitorPanel::initUi() {
  setObjectName(QStringLiteral("ExecutionMonitorPanel"));

  // 加载独立 QSS 样式
  QFile styleFile(
      QStringLiteral(":/resources/styles/execution_monitor_panel.qss"));
  if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
    setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    styleFile.close();
  }

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ── 进度树 ──
  tree_progress_ = new QTreeWidget(this);
  tree_progress_->setHeaderHidden(true);
  tree_progress_->setIndentation(16);
  tree_progress_->setRootIsDecorated(true);
  tree_progress_->setAnimated(true);
  tree_progress_->setAlternatingRowColors(true);
  tree_progress_->setFrameShape(QFrame::NoFrame);
  tree_progress_->header()->setStretchLastSection(true);
  mainLayout->addWidget(tree_progress_, 1);

  // ── 实时日志 ──
  text_log_ = new QPlainTextEdit(this);
  text_log_->setReadOnly(true);
  text_log_->setFrameShape(QFrame::NoFrame);
  text_log_->setFixedHeight(150);
  text_log_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  mainLayout->addWidget(text_log_);

  // ── 底部状态栏 ──
  auto* bottomBar = new QHBoxLayout();
  bottomBar->setContentsMargins(8, 2, 8, 2);
  bottomBar->setSpacing(6);

  label_pass_ = new QLabel(QStringLiteral("✅ 0"), this);
  label_pass_->setObjectName(QStringLiteral("statPass"));

  label_fail_ = new QLabel(QStringLiteral("❌ 0"), this);
  label_fail_->setObjectName(QStringLiteral("statFail"));

  label_timeout_ = new QLabel(QStringLiteral("⏱ 0"), this);
  label_timeout_->setObjectName(QStringLiteral("statTimeout"));

  auto* sep1 = new QLabel(QStringLiteral("│"), this);
  sep1->setStyleSheet(QStringLiteral("color: #C0C0C0;"));

  label_device_ = new QLabel(QStringLiteral("设备: --"), this);
  label_device_->setObjectName(QStringLiteral("deviceLabel"));

  auto* sep2 = new QLabel(QStringLiteral("│"), this);
  sep2->setStyleSheet(QStringLiteral("color: #C0C0C0;"));

  label_status_ = new QLabel(QStringLiteral("空闲"), this);
  label_status_->setObjectName(QStringLiteral("statusLabel"));

  bottomBar->addWidget(label_pass_);
  bottomBar->addWidget(label_fail_);
  bottomBar->addWidget(label_timeout_);
  bottomBar->addSpacing(8);
  bottomBar->addWidget(sep1);
  bottomBar->addWidget(label_device_);
  bottomBar->addWidget(sep2);
  bottomBar->addWidget(label_status_);
  bottomBar->addStretch();

  mainLayout->addLayout(bottomBar);
}

void ExecutionMonitorPanel::bindEngine(
    etest::engine::TestExecutionEngine* engine) {
  if (engine == nullptr) {
    return;
  }

  connect(engine, &etest::engine::TestExecutionEngine::suiteStarted, this,
          &ExecutionMonitorPanel::onSuiteStarted);
  connect(engine, &etest::engine::TestExecutionEngine::suiteFinished, this,
          &ExecutionMonitorPanel::onSuiteFinished);
  connect(engine, &etest::engine::TestExecutionEngine::caseStarted, this,
          &ExecutionMonitorPanel::onCaseStarted);
  connect(engine, &etest::engine::TestExecutionEngine::stepStarted, this,
          &ExecutionMonitorPanel::onStepStarted);
  connect(engine, &etest::engine::TestExecutionEngine::stepFinished, this,
          &ExecutionMonitorPanel::onStepFinished);
  connect(engine, &etest::engine::TestExecutionEngine::progressUpdated, this,
          &ExecutionMonitorPanel::onProgressUpdated);
  connect(engine, &etest::engine::TestExecutionEngine::engineStateChanged, this,
          &ExecutionMonitorPanel::onEngineStateChanged);
}

void ExecutionMonitorPanel::clear() {
  tree_progress_->clear();
  text_log_->clear();
  count_pass_ = 0;
  count_fail_ = 0;
  count_timeout_ = 0;
  case_items_.clear();
  case_names_.clear();
  step_items_.clear();
  online_devices_.clear();
  label_pass_->setText(QStringLiteral("✅ 0"));
  label_fail_->setText(QStringLiteral("❌ 0"));
  label_timeout_->setText(QStringLiteral("⏱ 0"));
  label_device_->setText(QStringLiteral("设备: --"));
  label_status_->setText(QStringLiteral("空闲"));
}

// ── Slots ──

void ExecutionMonitorPanel::onSuiteStarted(const QString& name) {
  clear();
  label_status_->setText(QStringLiteral("运行中"));
}

void ExecutionMonitorPanel::onSuiteFinished(const QString& name, int pass,
                                             int fail) {
  Q_UNUSED(name);
  Q_UNUSED(pass);
  Q_UNUSED(fail);
  label_status_->setText(QStringLiteral("完成"));
}

void ExecutionMonitorPanel::onCaseStarted(int caseIndex, const QString& name) {
  // Store case name for later use
  case_names_[caseIndex] = name;

  // Create top-level tree item
  auto* item = new QTreeWidgetItem(tree_progress_);
  item->setText(0, QStringLiteral("□ %1").arg(name));
  item->setExpanded(true);
  case_items_[caseIndex] = item;
}

void ExecutionMonitorPanel::onStepStarted(int caseIndex,
                                           const QString& stepPath,
                                           const QString& command,
                                           const QString& target) {
  QTreeWidgetItem* item = findOrCreateStepItem(caseIndex, stepPath);
  if (item == nullptr) {
    return;
  }

  // Set display text for the leaf
  QString displayText;
  if (command.isEmpty() && target.isEmpty()) {
    // Intermediate or structural node — keep segment name
    QStringList segments = stepPath.split(QLatin1Char('/'));
    displayText = segments.last();
  } else {
    displayText =
        QStringLiteral("%1 %2 %3").arg(statusIcon(etest::engine::PENDING))
            .arg(command)
            .arg(target);
  }
  item->setText(0, displayText);
  item->setData(0, Qt::UserRole, static_cast<int>(etest::engine::PENDING));

  // Expand all ancestors
  QTreeWidgetItem* parent = item->parent();
  while (parent != nullptr) {
    if (!parent->isExpanded()) {
      parent->setExpanded(true);
    }
    parent = parent->parent();
  }
}

void ExecutionMonitorPanel::onStepFinished(
    int caseIndex, const QString& stepPath,
    const etest::engine::StepResult& result) {
  QString key =
      QStringLiteral("%1/%2").arg(caseIndex).arg(stepPath);
  auto it = step_items_.find(key);
  if (it == step_items_.end()) {
    return;
  }

  QTreeWidgetItem* item = it.value();
  QString icon = statusIcon(result.status);
  QString displayText =
      QStringLiteral("%1 %2 %3 (%4ms)")
          .arg(icon)
          .arg(result.command)
          .arg(result.target)
          .arg(result.elapsedMs);
  item->setText(0, displayText);
  item->setData(0, Qt::UserRole, static_cast<int>(result.status));

  // 追加日志
  QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  QString color = statusHtmlColor(result.status);
  QString statusStr = statusText(result.status);
  QString htmlLog = QStringLiteral(
                        "<span style='color:#888'>[%1]</span> "
                        "<span style='color:%2;font-weight:bold'>%3 %4</span> "
                        "%5 (<span style='color:%6'>%7ms</span>)")
                        .arg(timestamp)
                        .arg(color)
                        .arg(icon)
                        .arg(statusStr)
                        .arg(result.command + QStringLiteral(" ") +
                             result.target)
                        .arg(color)
                        .arg(result.elapsedMs);
  text_log_->appendHtml(htmlLog);

  // 更新统计
  updateStats();
}

void ExecutionMonitorPanel::onProgressUpdated(int current, int total) {
  Q_UNUSED(current);
  Q_UNUSED(total);
}

void ExecutionMonitorPanel::onEngineStateChanged(
    etest::engine::EngineState state) {
  QString text;
  switch (state) {
    case etest::engine::EngineState::Idle:
      text = QStringLiteral("空闲");
      break;
    case etest::engine::EngineState::Running:
      text = QStringLiteral("运行中");
      break;
    case etest::engine::EngineState::Paused:
      text = QStringLiteral("暂停");
      break;
    case etest::engine::EngineState::Finished:
      text = QStringLiteral("完成");
      break;
    case etest::engine::EngineState::Error:
      text = QStringLiteral("错误");
      break;
  }
  label_status_->setText(text);
}

void ExecutionMonitorPanel::onDeviceStatusChanged(
    const QString& deviceId, etest::engine::DeviceStatus status) {
  if (status == etest::engine::DeviceStatus::Online) {
    online_devices_.insert(deviceId);
  } else {
    online_devices_.remove(deviceId);
  }

  if (online_devices_.isEmpty()) {
    label_device_->setText(QStringLiteral("设备: --"));
  } else {
    QString firstDevice = *online_devices_.begin();
    label_device_->setText(
        QStringLiteral("● %1 (%2 在线)")
            .arg(firstDevice)
            .arg(online_devices_.size()));
  }
}

// ── Private Helpers ──

void ExecutionMonitorPanel::updateStats() {
  count_pass_ = 0;
  count_fail_ = 0;
  count_timeout_ = 0;

  for (auto it = step_items_.constBegin(); it != step_items_.constEnd(); ++it) {
    int statusVal = it.value()->data(0, Qt::UserRole).toInt();
    auto status = static_cast<etest::engine::StepStatus>(statusVal);
    switch (status) {
      case etest::engine::PASS:
        ++count_pass_;
        break;
      case etest::engine::FAIL:
        ++count_fail_;
        break;
      case etest::engine::TIMEOUT:
        ++count_timeout_;
        break;
      default:
        break;
    }
  }

  label_pass_->setText(QStringLiteral("✅ %1").arg(count_pass_));
  label_fail_->setText(QStringLiteral("❌ %1").arg(count_fail_));
  label_timeout_->setText(QStringLiteral("⏱ %1").arg(count_timeout_));
}

QString ExecutionMonitorPanel::statusIcon(
    etest::engine::StepStatus status) const {
  switch (status) {
    case etest::engine::PASS:
      return QStringLiteral("✅");
    case etest::engine::FAIL:
      return QStringLiteral("❌");
    case etest::engine::TIMEOUT:
      return QStringLiteral("⏱");
    case etest::engine::SKIPPED:
      return QStringLiteral("⬜");
    case etest::engine::ERROR:
      return QStringLiteral("❌");
    case etest::engine::PENDING:
    default:
      return QStringLiteral("⏳");
  }
}

QString ExecutionMonitorPanel::statusText(
    etest::engine::StepStatus status) const {
  switch (status) {
    case etest::engine::PASS:
      return QStringLiteral("PASS");
    case etest::engine::FAIL:
      return QStringLiteral("FAIL");
    case etest::engine::TIMEOUT:
      return QStringLiteral("TIMEOUT");
    case etest::engine::SKIPPED:
      return QStringLiteral("SKIPPED");
    case etest::engine::ERROR:
      return QStringLiteral("ERROR");
    case etest::engine::PENDING:
    default:
      return QStringLiteral("PENDING");
  }
}

QString ExecutionMonitorPanel::statusHtmlColor(
    etest::engine::StepStatus status) const {
  switch (status) {
    case etest::engine::PASS:
      return QStringLiteral("#1B7A2B");
    case etest::engine::FAIL:
      return QStringLiteral("#C62828");
    case etest::engine::TIMEOUT:
      return QStringLiteral("#BD6B00");
    case etest::engine::SKIPPED:
      return QStringLiteral("#999999");
    case etest::engine::ERROR:
      return QStringLiteral("#C62828");
    case etest::engine::PENDING:
    default:
      return QStringLiteral("#888888");
  }
}

QTreeWidgetItem* ExecutionMonitorPanel::findOrCreateStepItem(
    int caseIndex, const QString& stepPath) {
  // Check flat map first
  QString key =
      QStringLiteral("%1/%2").arg(caseIndex).arg(stepPath);
  auto it = step_items_.find(key);
  if (it != step_items_.end()) {
    return it.value();
  }

  // Get or create the case top-level item
  QTreeWidgetItem* caseItem = case_items_.value(caseIndex, nullptr);
  if (caseItem == nullptr) {
    // Case item not created yet — create a placeholder
    QString caseName =
        case_names_.value(caseIndex,
                          QStringLiteral("用例 %1").arg(caseIndex));
    caseItem = new QTreeWidgetItem(tree_progress_);
    caseItem->setText(0, QStringLiteral("□ %1").arg(caseName));
    caseItem->setExpanded(true);
    case_items_[caseIndex] = caseItem;
  }

  // Split stepPath and navigate/create hierarchy
  QStringList segments = stepPath.split(QLatin1Char('/'));
  QTreeWidgetItem* parent = caseItem;

  for (int i = 0; i < segments.size(); ++i) {
    const QString& segment = segments[i];
    bool isLeaf = (i == segments.size() - 1);

    // Find existing child by text
    QTreeWidgetItem* child = nullptr;
    for (int j = 0; j < parent->childCount(); ++j) {
      if (parent->child(j)->text(0) == segment) {
        child = parent->child(j);
        break;
      }
    }

    if (child == nullptr) {
      child = new QTreeWidgetItem(parent);
      if (!isLeaf) {
        // Intermediate node: show segment name
        child->setText(0, segment);
      }
      child->setExpanded(true);
    }

    parent = child;
  }

  // Store leaf in flat map
  step_items_[key] = parent;
  return parent;
}

}  // namespace etest::app
