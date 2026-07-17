#include "RunStatusPanel.h"

#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// 构造 / UI 初始化
// ══════════════════════════════════════════════════════════════════════════════

RunStatusPanel::RunStatusPanel(QWidget* parent)
    : QWidget(parent) {
  initUi();
}

void RunStatusPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  // 运行结果树
  tree_ = new QTreeWidget(this);
  tree_->setObjectName(QStringLiteral("RunStatusTree"));
  tree_->setHeaderHidden(true);
  tree_->setFrameShape(QFrame::NoFrame);
  tree_->setRootIsDecorated(true);
  tree_->setAnimated(true);
  layout->addWidget(tree_, 1);

  // 统计标签
  stats_label_ = new QLabel(QStringLiteral("PASS: 0  FAIL: 0  TIMEOUT: 0"), this);
  stats_label_->setObjectName(QStringLiteral("RunStatusStats"));
  layout->addWidget(stats_label_);
}

// ══════════════════════════════════════════════════════════════════════════════
// onSuiteStarted — 清空树，设置套件名为根节点
// ══════════════════════════════════════════════════════════════════════════════

void RunStatusPanel::onSuiteStarted(const QString& name) {
  clearAll();

  auto* root = new QTreeWidgetItem(tree_);
  root->setText(0, name);
  root->setExpanded(true);
  root->setFlags(root->flags() & ~Qt::ItemIsUserCheckable);
}

// ══════════════════════════════════════════════════════════════════════════════
// onSuiteFinished — 更新套件节点的最终统计
// ══════════════════════════════════════════════════════════════════════════════

void RunStatusPanel::onSuiteFinished(const QString& name, int pass, int fail) {
  if (tree_->topLevelItemCount() == 0) {
    return;
  }
  auto* root = tree_->topLevelItem(0);
  root->setText(0, QStringLiteral("%1  (PASS: %2  FAIL: %3)")
                     .arg(name)
                     .arg(pass)
                     .arg(fail));
}

// ══════════════════════════════════════════════════════════════════════════════
// onCaseStarted — 添加 case 为顶级节点
// ══════════════════════════════════════════════════════════════════════════════

void RunStatusPanel::onCaseStarted(int caseIndex, const QString& name) {
  if (tree_->topLevelItemCount() == 0) {
    return;
  }
  auto* root = tree_->topLevelItem(0);

  auto* item = new QTreeWidgetItem(root);
  item->setText(0, name);
  item->setExpanded(true);
  item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);

  case_items_.insert(caseIndex, item);
}

// ══════════════════════════════════════════════════════════════════════════════
// onCaseFinished — 更新 case 节点状态
// ══════════════════════════════════════════════════════════════════════════════

void RunStatusPanel::onCaseFinished(int caseIndex, const QString& name,
                                     int result) {
  auto it = case_items_.constFind(caseIndex);
  if (it == case_items_.constEnd()) {
    return;
  }

  QString suffix;
  if (result == 0) {
    suffix = QStringLiteral(" ✅");
  } else {
    suffix = QStringLiteral(" ❌");
  }
  it.value()->setText(0, name + suffix);
}

// ══════════════════════════════════════════════════════════════════════════════
// onStepFinished — 在对应 case 下添加或更新步骤节点
// ══════════════════════════════════════════════════════════════════════════════

void RunStatusPanel::onStepFinished(int caseIndex, const QString& stepPath,
                                     const QString& status,
                                     const QString& message) {
  auto* item = findOrCreateStepItem(caseIndex, stepPath);
  if (!item) {
    return;
  }

  QString icon = statusIcon(status);
  item->setText(0, QStringLiteral("%1 %2").arg(icon, message.isEmpty() ? stepPath : message));
  item->setToolTip(0, stepPath);
  item->setExpanded(true);
}

// ══════════════════════════════════════════════════════════════════════════════
// updateStats — 更新统计
// ══════════════════════════════════════════════════════════════════════════════

void RunStatusPanel::updateStats(int pass, int fail, int timeout) {
  pass_count_ = pass;
  fail_count_ = fail;
  timeout_count_ = timeout;

  stats_label_->setText(
      QStringLiteral("PASS: %1  FAIL: %2  TIMEOUT: %3")
          .arg(pass_count_)
          .arg(fail_count_)
          .arg(timeout_count_));
}

// ══════════════════════════════════════════════════════════════════════════════
// clearAll — 重置
// ══════════════════════════════════════════════════════════════════════════════

void RunStatusPanel::clearAll() {
  tree_->clear();
  case_items_.clear();
  step_items_.clear();
  pass_count_ = 0;
  fail_count_ = 0;
  timeout_count_ = 0;
  stats_label_->setText(QStringLiteral("PASS: 0  FAIL: 0  TIMEOUT: 0"));
}

// ══════════════════════════════════════════════════════════════════════════════
// helper
// ══════════════════════════════════════════════════════════════════════════════

QString RunStatusPanel::statusIcon(const QString& status) {
  if (status == QStringLiteral("PASS")) {
    return QStringLiteral("✅");
  }
  if (status == QStringLiteral("FAIL")) {
    return QStringLiteral("❌");
  }
  if (status == QStringLiteral("TIMEOUT")) {
    return QStringLiteral("⏱");
  }
  return QStringLiteral("⏳");
}

QTreeWidgetItem* RunStatusPanel::findOrCreateStepItem(int caseIndex,
                                                       const QString& stepPath) {
  // 先查 step_items_ 缓存
  QString key = QStringLiteral("%1/%2").arg(caseIndex).arg(stepPath);
  auto it = step_items_.constFind(key);
  if (it != step_items_.constEnd()) {
    return it.value();
  }

  // 没有则创建
  auto caseIt = case_items_.constFind(caseIndex);
  if (caseIt == case_items_.constEnd()) {
    return nullptr;
  }

  auto* item = new QTreeWidgetItem(caseIt.value());
  item->setText(0, stepPath);
  item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);

  step_items_.insert(key, item);
  return item;
}

}  // namespace etest::app
