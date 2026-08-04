#include "MonitorConfigDialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

#include "logger/Logger.h"
#include "visualizers/DigitalMeterWidget.h"
#include "visualizers/GaugeVisualizer.h"
#include "visualizers/SignalVisualizer.h"
#include "visualizers/StateLEDWidget.h"
#include "visualizers/ValueLabelWidget.h"
#include "visualizers/WaveformWidget.h"

namespace etest::app {

namespace {

// 已配置监听器行的绿色前景色（模型属性，非 QSS）
const QColor kConfiguredColor(0, 150, 0);
// 失效监听器行灰色前景色
const QColor kInvalidColor(140, 140, 140);

}  // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════════
// MonitorTypeTile
// ══════════════════════════════════════════════════════════════════════════════

MonitorTypeTile::MonitorTypeTile(const QString& displayMode,
                                 const QString& title, QWidget* parent)
    : QGroupBox(title, parent), display_mode_(displayMode) {
  setObjectName(QStringLiteral("MonitorTypeTile"));
  setProperty("selected", false);
  // 不用 checkable：未勾选会禁用子控件导致预览变灰（用户不接受）。
}

void MonitorTypeTile::mousePressEvent(QMouseEvent* event) {
  Q_UNUSED(event)
  emit clicked(display_mode_);
}

void MonitorTypeTile::setSelectedHighlight(bool selected) {
  if (property("selected").toBool() == selected) {
    return;
  }
  setProperty("selected", selected);
  style()->unpolish(this);
  style()->polish(this);
}

// ══════════════════════════════════════════════════════════════════════════════
// MonitorConfigDialog
// ══════════════════════════════════════════════════════════════════════════════

MonitorConfigDialog::MonitorConfigDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("监听器配置"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  resize(880, 560);
  initUi();
}

void MonitorConfigDialog::initUi() {
  model_ = new QStandardItemModel(this);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);

  // ── 左右分栏改为 QSplitter，用户可拖拽调整宽度 ──
  auto* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setObjectName(QStringLiteral("MonitorConfigSplitter"));
  splitter->setChildrenCollapsible(false);

  // ── 左栏：搜索 + 连接列表 ──
  auto* leftWidget = new QWidget(splitter);
  auto* leftCol = new QVBoxLayout(leftWidget);
  leftCol->setContentsMargins(0, 0, 0, 0);
  leftCol->setSpacing(4);

  search_box_ = new QLineEdit(leftWidget);
  search_box_->setObjectName(QStringLiteral("MonitorConfigSearch"));
  search_box_->setPlaceholderText(QStringLiteral("搜索通道..."));
  search_box_->setClearButtonEnabled(true);
  leftCol->addWidget(search_box_);

  list_view_ = new QListView(leftWidget);
  list_view_->setObjectName(QStringLiteral("MonitorConfigList"));
  list_view_->setModel(model_);
  list_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  leftCol->addWidget(list_view_, 1);

  splitter->addWidget(leftWidget);

  // ── 右栏：visualizer 类型瓦片 + 删除按钮 ──
  auto* rightWidget = new QWidget(splitter);
  auto* rightCol = new QVBoxLayout(rightWidget);
  rightCol->setContentsMargins(0, 0, 0, 0);
  rightCol->setSpacing(4);

  auto* scroll = new QScrollArea(rightWidget);
  scroll->setObjectName(QStringLiteral("MonitorConfigRight"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);  // 去边框
  // 白底来自 QScrollArea 的 viewport（默认调色板 Window），关闭其与内容容器的
  // autoFillBackground，让 #MonitorConfigRight 的主题背景透出来
  scroll->viewport()->setAutoFillBackground(false);
  auto* content = new QWidget(scroll);
  content->setAutoFillBackground(false);
  tiles_grid_ = new QGridLayout(content);
  tiles_grid_->setContentsMargins(4, 4, 4, 4);
  tiles_grid_->setSpacing(8);
  scroll->setWidget(content);
  rightCol->addWidget(scroll, 1);

  delete_button_ = new QPushButton(QStringLiteral("删除监听器"), rightWidget);
  delete_button_->setObjectName(QStringLiteral("MonitorConfigDelete"));
  delete_button_->setEnabled(false);
  rightCol->addWidget(delete_button_, 0, Qt::AlignLeft);

  splitter->addWidget(rightWidget);

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({340, 480});

  root->addWidget(splitter);

  buildRightPanel();

  // ── 信号 ──
  connect(search_box_, &QLineEdit::textChanged, this,
          &MonitorConfigDialog::onFilterChanged);
  connect(list_view_->selectionModel(), &QItemSelectionModel::currentRowChanged,
          this, [this](const QModelIndex& current, const QModelIndex& previous) {
            Q_UNUSED(previous)
            onRowSelected(current);
          });
  connect(model_, &QStandardItemModel::itemChanged, this,
          &MonitorConfigDialog::onItemCheckToggled);
  connect(list_view_, &QListView::doubleClicked, this,
          [this](const QModelIndex& index) {
            QStandardItem* item = model_->itemFromIndex(index);
            if (!item) {
              return;
            }
            if (item->data(Qt::UserRole + 1).toInt() != kRowConnection) {
              return;
            }
            const QString cid = item->data(Qt::UserRole).toString();
            if (!monitor_map_.contains(cid)) {
              return;
            }
            selected_connection_id_ = cid;
            onRenameCurrent();
          });
  connect(delete_button_, &QPushButton::clicked, this, [this]() {
    // 按"选中项确有监听器"判断（不能用 isEmpty 守卫：空 connectionId 的失效
    // 监听器也必须能删，审查 🟡A）
    if (monitorOf(selected_connection_id_) != nullptr) {
      emit deleteRequested(selected_connection_id_);
    }
  });
}

// ══════════════════════════════════════════════════════════════════════════════
// 右栏 — 5 种 visualizer 真实空态实例，创建一次复用
// ══════════════════════════════════════════════════════════════════════════════

void MonitorConfigDialog::buildRightPanel() {
  const QStringList modes = {QStringLiteral("waveform"), QStringLiteral("led"),
                             QStringLiteral("meter"),
                             QStringLiteral("gauge"),
                             QStringLiteral("frame")};
  const QHash<QString, QString> modeTitles = {
      {QStringLiteral("waveform"), QStringLiteral("波形")},
      {QStringLiteral("led"), QStringLiteral("LED")},
      {QStringLiteral("meter"), QStringLiteral("数字表")},
      {QStringLiteral("gauge"), QStringLiteral("指针表")},
      {QStringLiteral("frame"), QStringLiteral("帧数据")}};
  int col = 0;
  int row = 0;
  for (const QString& mode : modes) {
    auto* tile = new MonitorTypeTile(mode, modeTitles.value(mode, mode), this);
    tile->setFixedSize(230, 170);

    SignalVisualizer* vis = createPreviewVisualizer(mode);
    if (vis) {
      // 预览实例去掉根背景：改 objectName 使 QSS #XXXWidget 容器背景选择器
      // 不再命中，再关 autoFillBackground，预览即透明（融入瓦片主题背景）
      vis->setObjectName(QStringLiteral("PreviewVisualizer"));
      vis->setAutoFillBackground(false);
      makeTransparentToMouse(vis);
      auto* lay = new QVBoxLayout(tile);
      lay->setContentsMargins(4, 4, 4, 4);
      lay->addWidget(vis);
    }

    connect(tile, &MonitorTypeTile::clicked, this,
            &MonitorConfigDialog::onTileClicked);

    tiles_.insert(mode, tile);
    tiles_grid_->addWidget(tile, row, col);
    ++col;
    if (col >= 2) {
      col = 0;
      ++row;
    }
  }
}

SignalVisualizer* MonitorConfigDialog::createPreviewVisualizer(
    const QString& displayMode) {
  if (displayMode == QStringLiteral("waveform")) {
    return new WaveformWidget(QStringLiteral("波形"), this);
  }
  if (displayMode == QStringLiteral("led")) {
    return new StateLEDWidget(QStringLiteral("LED"), this);
  }
  if (displayMode == QStringLiteral("meter")) {
    return new DigitalMeterWidget(QStringLiteral("数字表"), this);
  }
  if (displayMode == QStringLiteral("gauge")) {
    return new GaugeVisualizer(QStringLiteral("指针表"), this);
  }
  return new ValueLabelWidget(QStringLiteral("帧数据"), this);  // frame
}

void MonitorConfigDialog::makeTransparentToMouse(QWidget* widget) {
  if (!widget) {
    return;
  }
  widget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  const auto children = widget->findChildren<QWidget*>();
  for (QWidget* child : children) {
    child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// 数据接口
// ══════════════════════════════════════════════════════════════════════════════

void MonitorConfigDialog::setConnections(
    const QList<QPair<QString, QString>>& connections) {
  conn_map_ = connections;
  updateTilesEnabled();
  rebuildModel();
}

void MonitorConfigDialog::setMonitors(
    const QList<etest::engine::MonitorManager::MonitorTreeEntry>& monitors) {
  monitor_map_.clear();
  for (const auto& entry : monitors) {
    monitor_map_.insert(entry.connectionId, entry);
  }
  rebuildModel();
}

void MonitorConfigDialog::setChecked(const QList<QString>& checkedIds) {
  checked_ids_.clear();
  for (const QString& cid : checkedIds) {
    checked_ids_.insert(cid);
  }
  updateCheckStates();
}

// ══════════════════════════════════════════════════════════════════════════════
// 模型构建
// ══════════════════════════════════════════════════════════════════════════════

void MonitorConfigDialog::addSeparatorRow(const QString& text) {
  auto* item = new QStandardItem(text);
  item->setFlags(Qt::ItemIsEnabled);
  item->setData(kRowSeparator, Qt::UserRole + 1);
  QFont font = item->font();
  font.setBold(true);
  item->setFont(font);
  model_->appendRow(item);
}

void MonitorConfigDialog::rebuildModel() {
  if (!model_) {
    return;
  }
  const QString prevSel = selected_connection_id_;

  {
    QSignalBlocker blocker(model_);
    model_->clear();

    // 全部连接
    addSeparatorRow(QStringLiteral("全部连接"));
    for (const auto& pair : conn_map_) {
      const QString cid = pair.first;
      auto* item = new QStandardItem(pair.second);
      item->setData(cid, Qt::UserRole);
      item->setData(kRowConnection, Qt::UserRole + 1);

      auto mit = monitor_map_.constFind(cid);
      if (mit != monitor_map_.constEnd()) {
        // 已配置：显示 checkbox（默认勾选）+ 绿色 + 粗体
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(checked_ids_.contains(cid) ? Qt::Checked
                                                       : Qt::Unchecked);
        item->setForeground(QBrush(kConfiguredColor));
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
      }
      model_->appendRow(item);
    }

    // 失效监听器（决策 6：保留 + 列表标记无效，仅可删除）
    addSeparatorRow(QStringLiteral("失效监听器"));
    for (auto it = monitor_map_.constBegin(); it != monitor_map_.constEnd();
         ++it) {
      const auto& entry = it.value();
      if (!entry.invalid) {
        continue;
      }
      auto* item = new QStandardItem(
          entry.name + QStringLiteral("  （连接已删除）"));
      item->setData(entry.connectionId, Qt::UserRole);
      item->setData(kRowInvalid, Qt::UserRole + 1);
      item->setForeground(QBrush(kInvalidColor));
      model_->appendRow(item);
    }
  }

  // 恢复选中
  if (!prevSel.isEmpty()) {
    bool restored = false;
    for (int r = 0; r < model_->rowCount(); ++r) {
      QStandardItem* it = model_->item(r);
      if (it && it->data(Qt::UserRole).toString() == prevSel) {
        list_view_->setCurrentIndex(model_->indexFromItem(it));
        restored = true;
        break;
      }
    }
    if (!restored) {
      selected_connection_id_.clear();
    }
  } else {
    list_view_->setCurrentIndex(QModelIndex());
    selected_connection_id_.clear();
  }

  updateTileHighlights();
  onFilterChanged(search_box_ ? search_box_->text() : QString());
}

void MonitorConfigDialog::updateCheckStates() {
  if (!model_) {
    return;
  }
  QSignalBlocker blocker(model_);
  for (int r = 0; r < model_->rowCount(); ++r) {
    QStandardItem* item = model_->item(r);
    if (!item || item->data(Qt::UserRole + 1).toInt() != kRowConnection) {
      continue;
    }
    const QString cid = item->data(Qt::UserRole).toString();
    if (monitor_map_.contains(cid)) {
      item->setCheckState(checked_ids_.contains(cid) ? Qt::Checked
                                                     : Qt::Unchecked);
    }
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// 交互
// ══════════════════════════════════════════════════════════════════════════════

void MonitorConfigDialog::onRowSelected(const QModelIndex& index) {
  if (!index.isValid()) {
    return;
  }
  QStandardItem* item = model_->itemFromIndex(index);
  if (!item || item->data(Qt::UserRole + 1).toInt() == kRowSeparator) {
    return;
  }
  selected_connection_id_ = item->data(Qt::UserRole).toString();
  updateTileHighlights();
  emit channelSelected(selected_connection_id_);
}

void MonitorConfigDialog::onItemCheckToggled(QStandardItem* item) {
  if (!item || item->data(Qt::UserRole + 1).toInt() != kRowConnection) {
    return;
  }
  const QString cid = item->data(Qt::UserRole).toString();
  const bool checked = (item->checkState() == Qt::Checked);
  LOG_DEBUG("VISUAL", "MonitorConfigDialog checkToggled cid={} checked={}",
            cid.toStdString(), checked);
  emit checkToggled(cid, checked);
}

void MonitorConfigDialog::onTileClicked(const QString& displayMode) {
  if (selected_connection_id_.isEmpty()) {
    LOG_DEBUG("VISUAL", "未选中连接，忽略瓦片点击 mode={}", displayMode.toStdString());
    return;
  }
  const auto* entry = monitorOf(selected_connection_id_);
  if (entry) {
    if (entry->invalid) {
      return;  // 失效监听器仅可删除（走「删除监听器」按钮，决策 6/17）
    }
    if (entry->displayMode == displayMode) {
      // 再点已选类型 = 取消配置（删除监听器）
      emit deleteRequested(selected_connection_id_);
    } else {
      // 切换展示方式
      emit visualizerChosen(selected_connection_id_, displayMode);
    }
  } else {
    // 创建（创建即所见）
    emit visualizerChosen(selected_connection_id_, displayMode);
  }
}

void MonitorConfigDialog::onRenameCurrent() {
  if (selected_connection_id_.isEmpty()) {
    return;
  }
  const auto* entry = monitorOf(selected_connection_id_);
  if (!entry || entry->invalid) {
    return;  // 仅已配置（非失效）可重命名
  }
  bool ok = false;
  const QString newName = QInputDialog::getText(
      this, QStringLiteral("重命名监听器"), QStringLiteral("主标题:"),
      QLineEdit::Normal, entry->name, &ok);
  if (ok && !newName.trimmed().isEmpty() && newName.trimmed() != entry->name) {
    emit renameRequested(selected_connection_id_, newName.trimmed());
  }
}

void MonitorConfigDialog::onFilterChanged(const QString& text) {
  if (!model_) {
    return;
  }
  const QString filter = text.trimmed().toLower();
  int connVisible = 0;
  int invalidVisible = 0;

  for (int r = 0; r < model_->rowCount(); ++r) {
    QStandardItem* item = model_->item(r);
    if (!item) {
      continue;
    }
    const int kind = item->data(Qt::UserRole + 1).toInt();
    if (kind == kRowSeparator) {
      continue;
    }
    bool visible = true;
    if (!filter.isEmpty()) {
      visible = item->text().toLower().contains(filter);
    }
    list_view_->setRowHidden(r, !visible);
    if (visible) {
      if (kind == kRowConnection) {
        ++connVisible;
      } else if (kind == kRowInvalid) {
        ++invalidVisible;
      }
    }
  }

  // 分隔符仅在其分组有可见行时显示
  for (int r = 0; r < model_->rowCount(); ++r) {
    QStandardItem* item = model_->item(r);
    if (!item || item->data(Qt::UserRole + 1).toInt() != kRowSeparator) {
      continue;
    }
    const bool show = item->text() == QStringLiteral("全部连接")
                          ? (connVisible > 0)
                          : (invalidVisible > 0);
    list_view_->setRowHidden(r, !show);
  }
}

const etest::engine::MonitorManager::MonitorTreeEntry*
MonitorConfigDialog::monitorOf(const QString& connectionId) const {
  auto it = monitor_map_.constFind(connectionId);
  if (it != monitor_map_.constEnd()) {
    return &it.value();
  }
  return nullptr;
}

void MonitorConfigDialog::updateTileHighlights() {
  QString targetMode;
  const auto* entry = monitorOf(selected_connection_id_);
  if (entry) {
    targetMode = entry->displayMode;
  }
  for (auto it = tiles_.constBegin(); it != tiles_.constEnd(); ++it) {
    it.value()->setSelectedHighlight(it.key() == targetMode);
  }
  // 删除按钮：选中连接已配置 或 选中失效监听器时可删除
  if (delete_button_) {
    delete_button_->setEnabled(entry != nullptr);
  }
}

void MonitorConfigDialog::updateTilesEnabled() {
  // 无拓扑/无连接时禁用右栏类型瓦片（审查 🟡6）
  const bool enabled = !conn_map_.isEmpty();
  for (auto it = tiles_.constBegin(); it != tiles_.constEnd(); ++it) {
    it.value()->setEnabled(enabled);
  }
}

}  // namespace etest::app
