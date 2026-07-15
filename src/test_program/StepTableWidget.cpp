#include "StepTableWidget.h"

#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardItemModel>

#include "CommandTypeDelegate.h"
#include "SignalRegistry.h"
#include "logger/Logger.h"

namespace etest::app {

// ── UuidDisplayDelegate ──

UuidDisplayDelegate::UuidDisplayDelegate(
    etest::core::SignalRegistry* registry, QObject* parent)
    : QStyledItemDelegate(parent), registry_(registry) {}

QString UuidDisplayDelegate::displayText(const QVariant& value,
                                          const QLocale& locale) const {
  QString text = QStyledItemDelegate::displayText(value, locale);
  if (!registry_ || text.isEmpty() || text.length() != 32) {
    return text;
  }
  // 尝试 resolve UUID → 显示可读名称
  auto resolved = registry_->resolve(text);
  if (resolved.has_value()) {
    // 显示 nodePath 末段（如 "燃油阀门1"）
    QStringList parts = resolved->nodePath.split('/');
    if (!parts.isEmpty()) return parts.last();
  }
  // resolve 失败 → 显示前 8 位 + ...
  return text.left(8) + QStringLiteral("...");
}

// ── subSteps 序列化辅助（QVector<TestStepData> ↔ JSON） ──

static QJsonArray subStepsToJsonArray(const QVector<TestStepData>& steps) {
  QJsonArray arr;
  for (const auto& s : steps) {
    arr.append(testStepToJson(s));
  }
  return arr;
}

static QVector<TestStepData> subStepsFromJsonArray(const QJsonArray& arr) {
  QVector<TestStepData> steps;
  for (const auto& v : arr) {
    steps.append(testStepFromJson(v.toObject()));
  }
  return steps;
}

static QByteArray serializeSubSteps(const QVector<TestStepData>& steps) {
  return QJsonDocument(subStepsToJsonArray(steps))
      .toJson(QJsonDocument::Compact);
}

static QVector<TestStepData> deserializeSubSteps(const QByteArray& data) {
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (doc.isArray()) {
    return subStepsFromJsonArray(doc.array());
  }
  return {};
}

// ── 构造 ──

StepTableWidget::StepTableWidget(CommandTypeDelegate::Mode delegateMode,
                                 QWidget* parent)
    : QTableView(parent), delegateMode_(delegateMode) {
  setupModel();
  setupView();

  // 命令列 ComboBox 委托
  auto* cmdDelegate = new CommandTypeDelegate(delegateMode_, this);
  setItemDelegateForColumn(kColCmd, cmdDelegate);

  // 模型数据变更转发
  connect(model_, &QStandardItemModel::itemChanged, this,
          [this](QStandardItem* item) {
            emit cellDataChanged(item->row(), item->column());
          });

  // 选中变更转发
  connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
          [this]() { emit stepSelectionChanged(); });

  // 拖拽完成后重新编号
  connect(model_, &QAbstractItemModel::rowsInserted, this,
          [this](QModelIndex, int, int) {
            if (!batch_renumber_)
              renumberSteps();
          });
  connect(model_, &QAbstractItemModel::rowsRemoved, this,
          [this](QModelIndex, int, int) {
            if (!batch_renumber_)
              renumberSteps();
          });
}

void StepTableWidget::setupModel() {
  model_ = new QStandardItemModel(0, kColCount, this);
  // 列头固定，不随命令类型变化；参数1/参数2 列的含义由命令决定（见
  // extraCellText）
  model_->setHorizontalHeaderLabels(
      {QStringLiteral("步骤说明"), QStringLiteral("命令"),
       QStringLiteral("目标"), QStringLiteral("值"), QStringLiteral("参数1"),
       QStringLiteral("参数2"), QStringLiteral("超时(ms)")});
  setModel(model_);
}

void StepTableWidget::setupView() {
  // 无QFrame的边框
  setFrameShape(QFrame::NoFrame);

  // 表头拉伸策略
  horizontalHeader()->setStretchLastSection(false);
  horizontalHeader()->setSectionResizeMode(kColDesc, QHeaderView::Stretch);
  horizontalHeader()->setSectionResizeMode(kColCmd, QHeaderView::Fixed);
  setColumnWidth(kColCmd, 120);
  horizontalHeader()->setSectionResizeMode(kColExtra,
                                           QHeaderView::ResizeToContents);
  horizontalHeader()->setSectionResizeMode(kColExtra2,
                                           QHeaderView::ResizeToContents);
  horizontalHeader()->setSectionResizeMode(kColTimeout,
                                           QHeaderView::ResizeToContents);

  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  refreshRowHeight();
  setAlternatingRowColors(true);
  // M5: target 列固定宽度 + 溢出省略
  setColumnWidth(kColTarget, 180);
  setTextElideMode(Qt::ElideRight);

  // 拖拽排序
  setDragDropMode(QAbstractItemView::InternalMove);
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDragDropOverwriteMode(false);
}

// ── 行操作 ──

int StepTableWidget::rowCount() const {
  return model_->rowCount();
}

void StepTableWidget::setRowCount(int rows) {
  // 批量增/减行时，QStandardItemModel 会逐行 emit rowsInserted/rowsRemoved，
  // 导致 renumberSteps 被调用 O(N) 次。批量模式只调一次。
  if (rows != model_->rowCount()) {
    batch_renumber_ = true;
    model_->setRowCount(rows);
    batch_renumber_ = false;
    renumberSteps();
  }
}

void StepTableWidget::removeRow(int row) {
  model_->removeRow(row);
}

int StepTableWidget::currentRow() const {
  QModelIndex idx = currentIndex();
  return idx.isValid() ? idx.row() : -1;
}

void StepTableWidget::selectRow(int row) {
  if (row >= 0 && row < model_->rowCount()) {
    QTableView::selectRow(row);
  }
}

// ── 单元格读写 ──

void StepTableWidget::setCellText(int row, int col, const QString& text) {
  QStandardItem* item = model_->item(row, col);
  if (!item) {
    item = new QStandardItem(text);
    model_->setItem(row, col, item);
  } else {
    item->setText(text);
  }
}

QString StepTableWidget::cellText(int row, int col) const {
  QStandardItem* item = model_->item(row, col);
  return item ? item->text() : QString();
}

void StepTableWidget::setCellData(int row,
                                  int col,
                                  const QVariant& value,
                                  int role) {
  QStandardItem* item = model_->item(row, col);
  if (!item) {
    item = new QStandardItem();
    model_->setItem(row, col, item);
  }
  item->setData(value, role);
}

QVariant StepTableWidget::cellData(int row, int col, int role) const {
  QStandardItem* item = model_->item(row, col);
  return item ? item->data(role) : QVariant();
}

// ── 步骤扩展数据 ──

void StepTableWidget::setStepExtData(int row, const TestStepData& step) {
  QVariantMap ext;

  if (!step.condition.target.isEmpty() || !step.condition.op.isEmpty() ||
      step.condition.value.isValid()) {
    ext["conditionTarget"] = step.condition.target;
    ext["conditionOp"] = step.condition.op;
    ext["conditionValue"] = step.condition.value;
  }
  // 容差 min/max 无条件存（enabled=false 时也保留，避免取消勾选后值丢失）
  ext["tolMin"] = step.tolerance.min;
  ext["tolMax"] = step.tolerance.max;
  ext["tolEnabled"] = step.tolerance.enabled;
  if (!step.fault.type.isEmpty()) {
    ext["faultType"] = step.fault.type;
    ext["faultValue"] = step.fault.value;
  }
  if (step.loopCount > 1 || step.loopIntervalMs > 0) {
    ext["loopCount"] = step.loopCount;
    ext["loopIntervalMs"] = step.loopIntervalMs;
  }
  if (!step.subSteps.isEmpty()) {
    ext["subStepsJson"] = QString::fromUtf8(serializeSubSteps(step.subSteps));
  }
  if (!step.elseSubSteps.isEmpty()) {
    ext["elseSubStepsJson"] =
        QString::fromUtf8(serializeSubSteps(step.elseSubSteps));
  }

  QStandardItem* item = model_->item(row, kColCmd);
  if (!item) {
    item = new QStandardItem();
    model_->setItem(row, kColCmd, item);
  }
  item->setData(ext, kStepDataRole);
}

TestStepData StepTableWidget::stepExtData(int row) const {
  TestStepData step;
  QStandardItem* item = model_->item(row, kColCmd);
  if (!item) {
    return step;
  }
  QVariant v = item->data(kStepDataRole);
  if (!v.isValid()) {
    return step;
  }
  QVariantMap ext = v.toMap();

  if (ext.contains("conditionTarget")) {
    step.condition.target = ext["conditionTarget"].toString();
    step.condition.op = ext["conditionOp"].toString();
    step.condition.value = ext["conditionValue"];
  }
  if (ext.contains("tolEnabled")) {
    step.tolerance.enabled = ext["tolEnabled"].toBool();
    step.tolerance.min = ext["tolMin"].toDouble();
    step.tolerance.max = ext["tolMax"].toDouble();
  }
  if (ext.contains("faultType")) {
    step.fault.type = ext["faultType"].toString();
    step.fault.value = ext["faultValue"];
  }
  if (ext.contains("loopCount")) {
    step.loopCount = ext["loopCount"].toInt();
    step.loopIntervalMs = ext["loopIntervalMs"].toInt();
  }
  if (ext.contains("subStepsJson")) {
    step.subSteps =
        deserializeSubSteps(ext["subStepsJson"].toString().toUtf8());
  }
  if (ext.contains("elseSubStepsJson")) {
    step.elseSubSteps =
        deserializeSubSteps(ext["elseSubStepsJson"].toString().toUtf8());
  }
  return step;
}

// ── 重新编号 ──

void StepTableWidget::setReadOnly(bool readOnly) {
  setEditTriggers(readOnly ? QAbstractItemView::NoEditTriggers
                           : QAbstractItemView::DoubleClicked);
}

void StepTableWidget::renumberSteps() {
  for (int i = 0; i < model_->rowCount(); ++i) {
    verticalHeader()->setSectionHidden(i, false);
    model_->setHeaderData(i, Qt::Vertical, QString::number(i + 1),
                          Qt::DisplayRole);
  }
}

void StepTableWidget::refreshRowHeight() {
  // 行高 = 字体高度 + 余量；余量容纳 cell editor 的 QLineEdit 边框及 QSS
  // padding， 避免主题字体较大时编辑器字体被截断
  verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 12);
}

// ── M5: setRegistry ──

void StepTableWidget::setRegistry(etest::core::SignalRegistry* reg) {
  registry_ = reg;
  if (registry_) {
    setItemDelegateForColumn(kColTarget,
                             new UuidDisplayDelegate(registry_, this));
  } else {
    setItemDelegateForColumn(kColTarget, nullptr);
  }
}

// ── M0: 编辑拦截 ──

bool StepTableWidget::edit(const QModelIndex& index, EditTrigger trigger,
                           QEvent* event) {
  // 只有 target 列 + signal_selection_ 非空 + 鼠标触发的编辑才拦截
  // 不拦截 programmatic（如 CurrentChanged → _q_editItem → edit(index)）
  if (index.column() == kColTarget && signal_selection_ &&
      (trigger == QAbstractItemView::DoubleClicked ||
       trigger == QAbstractItemView::SelectedClicked)) {
    LOG_DEBUG("UUID", "StepTableWidget::edit target column -> selectSignal()");
    QString uuid = signal_selection_->selectSignal(this);
    if (!uuid.isEmpty()) {
      model_->setData(index, uuid);
    }
    return false;
  }
  if (index.column() == kColTarget) {
    LOG_DEBUG("UUID", "StepTableWidget::edit target column -> signal_selection_ is NULL, fallback to default edit");
  }
  return QTableView::edit(index, trigger, event);
}

}  // namespace etest::app
