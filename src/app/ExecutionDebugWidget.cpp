#include "ExecutionDebugWidget.h"

#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "engine/TestExecutionEngine.h"

namespace etest::app {

ExecutionDebugWidget::ExecutionDebugWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
  initSignals();
}

void ExecutionDebugWidget::initUi() {
  setObjectName(QStringLiteral("ExecutionDebugWidget"));

  // ── 概览区（运行前提） ──
  overview_container_ = new QWidget(this);
  overview_container_->setObjectName(QStringLiteral("debugOverview"));
  auto* overview_layout = new QVBoxLayout(overview_container_);
  overview_layout->setContentsMargins(8, 4, 8, 4);
  overview_layout->setSpacing(2);

  label_overview_summary_ = new QLabel(
      QStringLiteral("运行前提: 点击验证检查"), this);
  label_overview_summary_->setObjectName(
      QStringLiteral("overviewSummary"));
  overview_layout->addWidget(label_overview_summary_);

  overview_detail_widget_ = new QWidget(this);
  overview_detail_widget_->setObjectName(
      QStringLiteral("overviewDetail"));
  overview_detail_widget_->hide();
  overview_layout->addWidget(overview_detail_widget_);

  // ── 进度树 ──
  tree_progress_ = new QTreeWidget(this);
  tree_progress_->setHeaderHidden(true);
  tree_progress_->setIndentation(16);
  tree_progress_->setRootIsDecorated(true);
  tree_progress_->setAnimated(true);
  tree_progress_->setAlternatingRowColors(true);
  tree_progress_->setFrameShape(QFrame::NoFrame);
  tree_progress_->header()->setStretchLastSection(true);

  // ── 整体布局 ──
  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);
  main_layout->addWidget(overview_container_);
  main_layout->addWidget(tree_progress_, 1);
}

void ExecutionDebugWidget::initSignals() {
  // 预留：概览区展开/折叠、与外部组件联动等信号连接
}

void ExecutionDebugWidget::bindEngine(
    etest::engine::TestExecutionEngine* engine) {
  if (engine == nullptr) {
    return;
  }

  connect(engine, &etest::engine::TestExecutionEngine::suiteStarted, this,
          &ExecutionDebugWidget::onSuiteStarted);
  connect(engine, &etest::engine::TestExecutionEngine::suiteFinished, this,
          &ExecutionDebugWidget::onSuiteFinished);
  connect(engine, &etest::engine::TestExecutionEngine::caseStarted, this,
          &ExecutionDebugWidget::onCaseStarted);
  connect(engine, &etest::engine::TestExecutionEngine::stepStarted, this,
          &ExecutionDebugWidget::onStepStarted);
  connect(engine, &etest::engine::TestExecutionEngine::stepFinished, this,
          &ExecutionDebugWidget::onStepFinished);
  connect(engine, &etest::engine::TestExecutionEngine::progressUpdated, this,
          &ExecutionDebugWidget::onProgressUpdated);
  connect(engine, &etest::engine::TestExecutionEngine::engineStateChanged, this,
          &ExecutionDebugWidget::onEngineStateChanged);

  // 绑定后触发一次前提检查
  refreshPreconditions();
}

void ExecutionDebugWidget::clear() {
  tree_progress_->clear();
  count_pass_ = 0;
  count_fail_ = 0;
  count_timeout_ = 0;
  case_items_.clear();
  case_names_.clear();
  step_items_.clear();
}

void ExecutionDebugWidget::refreshPreconditions() {
  checkPreconditions();
}

bool ExecutionDebugWidget::canRun() const {
  if (!preconditions_checked_) {
    return false;  // 未检查时保守阻断
  }
  for (const auto& ps : precondition_states_) {
    if (!ps.met && ps.isError) {
      return false;
    }
  }
  return true;
}

void ExecutionDebugWidget::checkPreconditions() {
  // TODO: 实现 6 项前提检查
  // 1. 项目已打开
  // 2. ICD 协议已定义
  // 3. 拓扑已定义
  // 4. 拓扑已绑定信号
  // 5. 测试程序可用
  // 6. 硬件/Mock 已配置
  //
  // 当前为骨架实现，后续补全具体检查逻辑
  preconditions_checked_ = true;
  precondition_states_.resize(6);
  // Placeholder: 全部标记为未检查
  for (auto& ps : precondition_states_) {
    ps.met = false;
    ps.isError = false;
    ps.label = QStringLiteral("待检查");
  }
  label_overview_summary_->setText(
      QStringLiteral("运行前提: 待验证"));
}

// ── Slots ──

void ExecutionDebugWidget::onSuiteStarted(const QString& name) {
  Q_UNUSED(name);
  clear();
}

void ExecutionDebugWidget::onSuiteFinished(const QString& name, int pass,
                                             int fail) {
  Q_UNUSED(name);
  Q_UNUSED(pass);
  Q_UNUSED(fail);
}

void ExecutionDebugWidget::onCaseStarted(int caseIndex, const QString& name) {
  case_names_[caseIndex] = name;
  auto* item = new QTreeWidgetItem(tree_progress_);
  item->setText(0, QStringLiteral("□ %1").arg(name));
  item->setExpanded(true);
  case_items_[caseIndex] = item;
}

void ExecutionDebugWidget::onStepStarted(int caseIndex,
                                           const QString& stepPath,
                                           const QString& command,
                                           const QString& target) {
  QTreeWidgetItem* item = findOrCreateStepItem(caseIndex, stepPath);
  if (item == nullptr) {
    return;
  }

  QString displayText;
  if (command.isEmpty() && target.isEmpty()) {
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

  QTreeWidgetItem* parent = item->parent();
  while (parent != nullptr) {
    if (!parent->isExpanded()) {
      parent->setExpanded(true);
    }
    parent = parent->parent();
  }
}

void ExecutionDebugWidget::onStepFinished(
    int caseIndex, const QString& stepPath,
    const etest::engine::StepResult& result) {
  QString key =
      QStringLiteral("%1/%2").arg(caseIndex).arg(stepPath);
  auto it = step_items_.find(key);
  if (it == step_items_.end()) {
    return;
  }

  QTreeWidgetItem* item = it.value();
  QString displayText =
      QStringLiteral("%1 %2 %3 (%4ms)")
          .arg(statusIcon(result.status))
          .arg(result.command)
          .arg(result.target)
          .arg(result.elapsedMs);
  item->setText(0, displayText);
  item->setData(0, Qt::UserRole, static_cast<int>(result.status));

  updateStats();
}

void ExecutionDebugWidget::onProgressUpdated(int current, int total) {
  Q_UNUSED(current);
  Q_UNUSED(total);
}

void ExecutionDebugWidget::onEngineStateChanged(
    etest::engine::EngineState state) {
  Q_UNUSED(state);
}

// ── Private Helpers ──

void ExecutionDebugWidget::updateStats() {
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
}

QString ExecutionDebugWidget::statusIcon(
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

QString ExecutionDebugWidget::statusHtmlColor(
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

QTreeWidgetItem* ExecutionDebugWidget::findOrCreateStepItem(
    int caseIndex, const QString& stepPath) {
  QString key =
      QStringLiteral("%1/%2").arg(caseIndex).arg(stepPath);
  auto it = step_items_.find(key);
  if (it != step_items_.end()) {
    return it.value();
  }

  QTreeWidgetItem* caseItem = case_items_.value(caseIndex, nullptr);
  if (caseItem == nullptr) {
    QString caseName =
        case_names_.value(caseIndex,
                          QStringLiteral("用例 %1").arg(caseIndex));
    caseItem = new QTreeWidgetItem(tree_progress_);
    caseItem->setText(0, QStringLiteral("□ %1").arg(caseName));
    caseItem->setExpanded(true);
    case_items_[caseIndex] = caseItem;
  }

  QStringList segments = stepPath.split(QLatin1Char('/'));
  QTreeWidgetItem* parent = caseItem;

  for (int i = 0; i < segments.size(); ++i) {
    const QString& segment = segments[i];
    bool isLeaf = (i == segments.size() - 1);

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
        child->setText(0, segment);
      }
      child->setExpanded(true);
    }

    parent = child;
  }

  step_items_[key] = parent;
  return parent;
}

}  // namespace etest::app
