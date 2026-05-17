#include "ProtocalEditorWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

#include "IcdBitLayoutView.h"
#include "IcdNodeTreeWidget.h"
#include "IcdPropertyPanel.h"

namespace etest::protocal {

ProtocalEditorWidget::ProtocalEditorWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
  initSignals();
}

void ProtocalEditorWidget::initUi() {
  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // === Top Toolbar ===
  auto* toolbar = new QWidget(this);
  toolbar->setFixedHeight(36);
  toolbar->setStyleSheet(
      "background-color: #2d2d2d; border-bottom: 1px solid #3c3c3c;");

  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(8, 0, 8, 0);

  auto* title_label = new QLabel(QStringLiteral("帧属性"), this);
  title_label->setStyleSheet(
      "font-weight: bold; color: #cccccc; padding-right: 12px;");

  frame_name_label_ = new QLabel(
      QStringLiteral("A429_00_ISI_01_发送_Label110_6272T_00"), this);
  frame_name_label_->setStyleSheet("color: #4ec9b0; padding-right: 16px;");

  frame_type_combo_ = new QComboBox(this);
  frame_type_combo_->addItem(QStringLiteral("发送 (Cmd)"));
  frame_type_combo_->addItem(QStringLiteral("接收 (Data)"));
  frame_type_combo_->addItem(QStringLiteral("配置 (Config)"));
  frame_type_combo_->setCurrentIndex(0);

  byte_order_combo_ = new QComboBox(this);
  byte_order_combo_->addItem(QStringLiteral("小端 (Little Endian)"));
  byte_order_combo_->addItem(QStringLiteral("大端 (Big Endian)"));

  auto* id_label = new QLabel(QStringLiteral("ID: 90"), this);
  id_label->setStyleSheet("color: #888; padding-left: 12px;");

  toolbar_layout->addWidget(title_label);
  toolbar_layout->addWidget(frame_name_label_);
  toolbar_layout->addWidget(frame_type_combo_);
  toolbar_layout->addWidget(byte_order_combo_);
  toolbar_layout->addWidget(id_label);
  toolbar_layout->addStretch();

  auto* length_label =
      new QLabel(QStringLiteral("帧长度: 16 bytes"), this);
  length_label->setStyleSheet("color: #888; padding-right: 8px;");
  toolbar_layout->addWidget(length_label);

  main_layout->addWidget(toolbar);

  // === Central Splitter ===
  splitter_ = new QSplitter(Qt::Horizontal, this);
  splitter_->setHandleWidth(1);
  splitter_->setChildrenCollapsible(false);

  node_tree_ = new IcdNodeTreeWidget(this);
  node_tree_->setMinimumWidth(200);

  bit_view_ = new IcdBitLayoutView(this);

  property_panel_ = new IcdPropertyPanel(this);
  property_panel_->setMinimumWidth(220);

  splitter_->addWidget(node_tree_);
  splitter_->addWidget(bit_view_);
  splitter_->addWidget(property_panel_);
  splitter_->setStretchFactor(0, 30);
  splitter_->setStretchFactor(1, 40);
  splitter_->setStretchFactor(2, 30);

  main_layout->addWidget(splitter_, 1);

  // === Bottom Status Bar ===
  auto* status_bar = new QWidget(this);
  status_bar->setFixedHeight(24);
  status_bar->setStyleSheet(
      "background-color: #007acc; color: white; font-size: 12px;");

  auto* status_layout = new QHBoxLayout(status_bar);
  status_layout->setContentsMargins(8, 0, 8, 0);

  status_label_ = new QLabel(
      QStringLiteral("GNSS_Latitude  |  Offset: 1  |  Bit: 0~20  |  "
                     "Type: uint  |  Scaled"),
      this);
  status_layout->addWidget(status_label_);
  status_layout->addStretch();

  auto* hint_label = new QLabel(
      QStringLiteral("点击信号树或色块查看属性"), this);
  status_layout->addWidget(hint_label);

  main_layout->addWidget(status_bar);
}

void ProtocalEditorWidget::initSignals() {
  // Placeholder: no business logic yet
}

}  // namespace etest::protocal
