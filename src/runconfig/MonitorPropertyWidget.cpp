#include "MonitorPropertyWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace etest::runconfig {

namespace {
const int kConnectionIdRole = Qt::UserRole + 1;
const char* const kDisplayModes[] = {"waveform", "led", "meter", "gauge",
                                     "frame"};
const char* const kDisplayNames[] = {"波形", "LED", "数字表", "指针表",
                                     "帧数据"};
}  // namespace

MonitorPropertyWidget::MonitorPropertyWidget(QWidget* parent)
    : QWidget(parent) {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  // QScrollArea 包裹表单（属性增多时可滚动）
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* form = new QWidget(scroll);
  auto* layout = new QVBoxLayout(form);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  // 名称
  layout->addWidget(new QLabel(QStringLiteral("名称"), form));
  name_edit_ = new QLineEdit(form);
  name_edit_->setPlaceholderText(QStringLiteral("监听器名称"));
  layout->addWidget(name_edit_);

  // 类型切换
  layout->addWidget(new QLabel(QStringLiteral("类型"), form));
  type_combo_ = new QComboBox(form);
  for (int i = 0; i < 5; ++i) {
    type_combo_->addItem(QString::fromUtf8(kDisplayNames[i]),
                         QString::fromLatin1(kDisplayModes[i]));
  }
  layout->addWidget(type_combo_);

  // 绑定连线
  layout->addWidget(new QLabel(QStringLiteral("绑定连线"), form));
  search_box_ = new QLineEdit(form);
  search_box_->setPlaceholderText(QStringLiteral("搜索连线..."));
  layout->addWidget(search_box_);
  conn_model_ = new QStandardItemModel(this);
  conn_list_ = new QListView(form);
  conn_list_->setModel(conn_model_);
  conn_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(conn_list_, 1);

  // 删除
  delete_btn_ = new QPushButton(QStringLiteral("删除监听器"), form);
  layout->addWidget(delete_btn_);

  scroll->setWidget(form);
  outer->addWidget(scroll);

  // 信号
  connect(name_edit_, &QLineEdit::textChanged, this, [this](const QString& t) {
    if (loading_ || monitor_id_.isEmpty()) {
      return;
    }
    emit nameChanged(monitor_id_, t);
  });
  connect(type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            if (loading_ || monitor_id_.isEmpty()) {
              return;
            }
            emit typeChanged(monitor_id_,
                             type_combo_->itemData(index).toString());
          });
  // 搜索框过滤连接列表（实时重建，用缓存 conns_/bound_）
  connect(search_box_, &QLineEdit::textChanged, this, [this](const QString&) {
    if (loading_ || monitor_id_.isEmpty()) {
      return;
    }
    rebuildConnectionList(conns_, current_connection_, bound_);
  });
  connect(conn_list_, &QListView::clicked, this,
          [this](const QModelIndex& idx) {
            if (loading_ || monitor_id_.isEmpty()) {
              return;
            }
            const QString cid = idx.data(kConnectionIdRole).toString();
            if (cid.isEmpty()) {
              return;
            }
            emit connectionBound(monitor_id_, cid);
          });
  connect(delete_btn_, &QPushButton::clicked, this, [this]() {
    if (!monitor_id_.isEmpty()) {
      emit deleteRequested(monitor_id_);
    }
  });

  clear();
}

void MonitorPropertyWidget::setMonitor(
    const RunConfig::Monitor& monitor,
    const QList<QPair<QString, QString>>& connections,
    const QSet<QString>& boundConnectionIds) {
  loading_ = true;
  monitor_id_ = monitor.id;
  current_connection_ = monitor.connectionId;
  conns_ = connections;
  bound_ = boundConnectionIds;
  name_edit_->setText(monitor.name);
  const int typeIdx = type_combo_->findData(monitor.displayMode);
  type_combo_->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);
  rebuildConnectionList(connections, monitor.connectionId, boundConnectionIds);
  const bool enabled = !monitor.id.isEmpty();
  name_edit_->setEnabled(enabled);
  type_combo_->setEnabled(enabled);
  search_box_->setEnabled(enabled);
  conn_list_->setEnabled(enabled);
  delete_btn_->setEnabled(enabled);
  loading_ = false;
}

void MonitorPropertyWidget::clear() {
  loading_ = true;
  monitor_id_.clear();
  current_connection_.clear();
  name_edit_->clear();
  type_combo_->setCurrentIndex(0);
  conn_model_->clear();
  name_edit_->setEnabled(false);
  type_combo_->setEnabled(false);
  search_box_->setEnabled(false);
  conn_list_->setEnabled(false);
  delete_btn_->setEnabled(false);
  loading_ = false;
}

void MonitorPropertyWidget::rebuildConnectionList(
    const QList<QPair<QString, QString>>& connections,
    const QString& currentConnectionId,
    const QSet<QString>& boundConnectionIds) {
  conn_model_->clear();
  const QString filter = search_box_->text().trimmed();
  for (const auto& conn : connections) {
    if (!filter.isEmpty() &&
        !conn.second.contains(filter, Qt::CaseInsensitive)) {
      continue;
    }
    auto* item = new QStandardItem(conn.second);
    item->setData(conn.first, kConnectionIdRole);
    // 已绑定其他卡片 → 视觉禁用 + 不可选（一连接一监听器）；
    // 当前绑定的连接保持可选（高亮）
    if (conn.first != currentConnectionId &&
        boundConnectionIds.contains(conn.first)) {
      item->setEnabled(false);
    }
    conn_model_->appendRow(item);
  }
}

}  // namespace etest::runconfig
