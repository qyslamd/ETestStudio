#include "IcdNodeTreeWidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStandardItem>
#include <QTreeView>
#include <QVBoxLayout>

namespace etest::protocal {

IcdNodeTreeWidget::IcdNodeTreeWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  populatePlaceholderData();
}

void IcdNodeTreeWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  filter_input_ = new QLineEdit(this);
  filter_input_->setPlaceholderText(QStringLiteral("搜索信号..."));
  filter_input_->setClearButtonEnabled(true);

  auto* header = new QLabel(QStringLiteral("信号列表"), this);
  header->setStyleSheet("font-weight: bold; padding: 4px 8px;");

  tree_view_ = new QTreeView(this);
  tree_view_->setHeaderHidden(true);
  tree_view_->setAnimated(true);
  tree_view_->setIndentation(16);
  tree_view_->setExpandsOnDoubleClick(true);
  tree_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);

  model_ = new QStandardItemModel(this);

  layout->addWidget(header);
  layout->addWidget(filter_input_);
  layout->addWidget(tree_view_, 1);
}

void IcdNodeTreeWidget::populatePlaceholderData() {
  auto* frame_item = new QStandardItem(
      QStringLiteral("A429_00_ISI_01_发送_Label110_6272T_00"));
  frame_item->setEditable(false);

  auto add_signal = [&](QStandardItem* parent, const QString& name,
                         const QString& info) {
    auto* item = new QStandardItem(name);
    item->setEditable(false);
    item->setToolTip(info);
    parent->appendRow(item);
    return item;
  };

  auto* send = add_signal(frame_item, QStringLiteral("发送数据 (Offset:0, Bit:0~31)"),
                          QStringLiteral("Type:dword | Tag:none"));
  add_signal(send, QStringLiteral("Label (Offset:0, Bit:0~7)"),
             QStringLiteral("Type:byte | Tag:none | Value:110"));
  add_signal(send, QStringLiteral("SDI (Offset:0, Bit:8~9)"),
             QStringLiteral("Type:uint | BitWidth:2"));
  add_signal(send, QStringLiteral("Data (Offset:0, Bit:10~28)"),
             QStringLiteral("Type:uint | BitWidth:19 | IsScaled"));
  add_signal(send, QStringLiteral("SSM (Offset:0, Bit:29~30)"),
             QStringLiteral("Type:uint | BitWidth:2"));
  add_signal(send, QStringLiteral("Parity (Offset:0, Bit:31)"),
             QStringLiteral("Type:boolean | BitWidth:1 | Tag:Sum"));

  auto* gnss = add_signal(frame_item, QStringLiteral("GNSS_Latitude (Offset:1, Bit:0~20)"),
                          QStringLiteral("Type:uint | BitWidth:21 | Scaled"));
  add_signal(gnss, QStringLiteral("纬度值 (Offset:1, Bit:0~20)"),
             QStringLiteral("ScaleA:0.00017166 | ScaleB:0 | Unit:°"));

  add_signal(frame_item, QStringLiteral("GNSS_Longitude (Offset:4, Bit:0~20)"),
             QStringLiteral("Type:uint | BitWidth:21 | Scaled"));

  add_signal(frame_item, QStringLiteral("GNSS_Altitude (Offset:8, Bit:0~15)"),
             QStringLiteral("Type:uint | BitWidth:16 | Unit:m"));

  model_->appendRow(frame_item);
  tree_view_->setModel(model_);
  tree_view_->expandAll();
}

}  // namespace etest::protocal
