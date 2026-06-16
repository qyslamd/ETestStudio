#include "IcdPropertyPanel.h"
#include "IcdProtocolUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace etest::protocal {

using namespace utils;

// ===========================================================================
// Constructor & UI construction
// ===========================================================================

IcdPropertyPanel::IcdPropertyPanel(QWidget* parent) : QWidget(parent) {
  initUi();
}

void IcdPropertyPanel::initUi() {
  auto* outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);

  // -- Section header (styled via #sectionHeader in QSS) --
  auto* header = new QLabel(QStringLiteral("信号属性"), this);
  header->setObjectName(QStringLiteral("sectionHeader"));

  // -- Scrollable form container --
  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  form_widget_ = new QWidget(this);
  form_layout_ = new QFormLayout(form_widget_);
  form_layout_->setContentsMargins(8, 8, 8, 8);
  form_layout_->setSpacing(6);
  form_layout_->setLabelAlignment(Qt::AlignRight);

  // =====================================================================
  //  1. Basic info  (name / description)
  // =====================================================================
  basic_group_ = new QGroupBox(QStringLiteral("基本信息"), form_widget_);
  auto* basic_form = new QFormLayout(basic_group_);
  basic_form->setSpacing(4);

  edit_name_ = new QLineEdit(basic_group_);
  edit_desc_ = new QLineEdit(basic_group_);
  basic_form->addRow(QStringLiteral("名称"), edit_name_);
  basic_form->addRow(QStringLiteral("描述"), edit_desc_);

  form_layout_->addRow(basic_group_);

  // =====================================================================
  //  2. Frame-specific properties  (hidden by default)
  // =====================================================================
  frame_group_ = new QGroupBox(QStringLiteral("帧属性"), form_widget_);
  auto* frame_form = new QFormLayout(frame_group_);
  frame_form->setSpacing(4);

  spin_frame_id_ = new QSpinBox(frame_group_);
  spin_frame_id_->setRange(0, 65535);

  combo_frame_type_ = new QComboBox(frame_group_);
  combo_frame_type_->addItems({
      QStringLiteral("数据"),
      QStringLiteral("命令"),
      QStringLiteral("数据/命令"),
  });
  combo_byte_order_ = new QComboBox(frame_group_);
  combo_byte_order_->addItems({
      QStringLiteral("小端"),
      QStringLiteral("大端"),
  });

  frame_form->addRow(QStringLiteral("帧 ID"), spin_frame_id_);
  frame_form->addRow(QStringLiteral("类型"), combo_frame_type_);
  frame_form->addRow(QStringLiteral("字节序"), combo_byte_order_);
  form_layout_->addRow(frame_group_);
  frame_group_->hide();

  // =====================================================================
  //  3. Node properties (offset, bit-layout, type, tag)
  // =====================================================================
  node_group_ = new QGroupBox(QStringLiteral("节点属性"), form_widget_);
  auto* node_form = new QFormLayout(node_group_);
  node_form->setSpacing(4);

  spin_offset_ = new QSpinBox(node_group_);
  spin_offset_->setRange(0, 65535);

  spin_start_bit_ = new QSpinBox(node_group_);
  spin_start_bit_->setRange(0, 7);

  spin_bit_width_ = new QSpinBox(node_group_);
  spin_bit_width_->setRange(1, 64);

  combo_type_ = new QComboBox(node_group_);
  combo_type_->addItems({
      QStringLiteral("uint8"),
      QStringLiteral("uint16"),
      QStringLiteral("int16"),
      QStringLiteral("uint32"),
      QStringLiteral("int32"),
      QStringLiteral("uint64"),
      QStringLiteral("float"),
      QStringLiteral("double"),
      QStringLiteral("boolean"),
      QStringLiteral("bytes"),
      QStringLiteral("string"),
      QStringLiteral("unknown"),
  });

  combo_tag_ = new QComboBox(node_group_);
  combo_tag_->addItems({
      QStringLiteral("none"),
      QStringLiteral("head"),
      QStringLiteral("length"),
      QStringLiteral("count"),
      QStringLiteral("sum"),
      QStringLiteral("xor"),
      QStringLiteral("signal_in_value"),
  });

  node_form->addRow(QStringLiteral("偏移"), spin_offset_);
  node_form->addRow(QStringLiteral("起始位"), spin_start_bit_);
  node_form->addRow(QStringLiteral("位宽"), spin_bit_width_);
  node_form->addRow(QStringLiteral("类型"), combo_type_);
  node_form->addRow(QStringLiteral("标签"), combo_tag_);
  form_layout_->addRow(node_group_);

  // =====================================================================
  //  4. Scaling  (is_scaled, scale_a/b, unit, formula, convertor)
  // =====================================================================
  scale_group_ = new QGroupBox(QStringLiteral("缩放"), form_widget_);
  auto* scale_form = new QFormLayout(scale_group_);
  scale_form->setSpacing(4);

  check_scaled_ = new QCheckBox(QStringLiteral("启用缩放"), scale_group_);

  dspin_scale_a_ = new QDoubleSpinBox(scale_group_);
  dspin_scale_a_->setRange(-1e9, 1e9);
  dspin_scale_a_->setDecimals(8);

  dspin_scale_b_ = new QDoubleSpinBox(scale_group_);
  dspin_scale_b_->setRange(-1e9, 1e9);
  dspin_scale_b_->setDecimals(8);

  edit_unit_ = new QLineEdit(scale_group_);
  edit_scale_formula_ = new QLineEdit(scale_group_);
  edit_scale_convertor_ = new QLineEdit(scale_group_);

  scale_form->addRow(check_scaled_);
  scale_form->addRow(QStringLiteral("Scale A"), dspin_scale_a_);
  scale_form->addRow(QStringLiteral("Scale B"), dspin_scale_b_);
  scale_form->addRow(QStringLiteral("单位"), edit_unit_);
  scale_form->addRow(QStringLiteral("缩放公式"), edit_scale_formula_);
  scale_form->addRow(QStringLiteral("缩放转换器"), edit_scale_convertor_);
  form_layout_->addRow(scale_group_);

  // =====================================================================
  //  5. Extended attributes  (system, group, min, max, values-text, link)
  // =====================================================================
  ext_group_ = new QGroupBox(QStringLiteral("扩展属性"), form_widget_);
  auto* ext_form = new QFormLayout(ext_group_);
  ext_form->setSpacing(4);

  edit_system_ = new QLineEdit(ext_group_);
  combo_group_ = new QComboBox(ext_group_);
  combo_group_->setEditable(true);
  combo_group_->lineEdit()->setPlaceholderText(QStringLiteral("输入或选择分组..."));
  combo_group_->addItems({
      QStringLiteral(""),
      QStringLiteral("header"),
      QStringLiteral("payload"),
      QStringLiteral("checksum"),
      QStringLiteral("length"),
      QStringLiteral("count"),
      QStringLiteral("address"),
  });

  dspin_min_ = new QDoubleSpinBox(ext_group_);
  dspin_min_->setRange(-1e9, 1e9);
  dspin_min_->setDecimals(6);
  dspin_min_->setSpecialValueText(QStringLiteral("(预留)"));
  dspin_min_->setReadOnly(true);

  dspin_max_ = new QDoubleSpinBox(ext_group_);
  dspin_max_->setRange(-1e9, 1e9);
  dspin_max_->setDecimals(6);
  dspin_max_->setSpecialValueText(QStringLiteral("(预留)"));
  dspin_max_->setReadOnly(true);

  edit_value_text_ = new QLineEdit(ext_group_);
  edit_link_to_ = new QLineEdit(ext_group_);

  ext_form->addRow(QStringLiteral("系统名"), edit_system_);
  ext_form->addRow(QStringLiteral("组名"), combo_group_);
  ext_form->addRow(QStringLiteral("最小值"), dspin_min_);
  ext_form->addRow(QStringLiteral("最大值"), dspin_max_);
  ext_form->addRow(QStringLiteral("值文本列表"), edit_value_text_);
  ext_form->addRow(QStringLiteral("链接"), edit_link_to_);
  form_layout_->addRow(ext_group_);

  // -- Assemble --
  scroll->setWidget(form_widget_);
  outer_layout->addWidget(header);
  outer_layout->addWidget(scroll, 1);

  // Start in the "empty" state
  clearForm();
}

// ===========================================================================
// Helper: disconnect all previously-connected node editing signals
// ===========================================================================

void IcdPropertyPanel::clearNodeConnections() {
  for (const auto& conn : node_connections_) {
    disconnect(conn);
  }
  node_connections_.clear();
}

// ===========================================================================
// Clear the entire form to default / empty state
// ===========================================================================

void IcdPropertyPanel::clearForm() {
  clearNodeConnections();
  current_node_ = nullptr;
  current_frame_ = nullptr;

  // Reset all widget values to defaults
  edit_name_->clear();
  edit_desc_->clear();

  spin_offset_->setValue(0);
  spin_start_bit_->setValue(0);
  spin_bit_width_->setValue(1);
  combo_type_->setCurrentIndex(
      combo_type_->findText(QStringLiteral("unknown")));
  combo_tag_->setCurrentIndex(0);

  check_scaled_->setChecked(false);
  dspin_scale_a_->setValue(0.0);
  dspin_scale_b_->setValue(0.0);
  edit_unit_->clear();
  edit_scale_formula_->clear();
  edit_scale_convertor_->clear();

  edit_system_->clear();
  combo_group_->setCurrentText(QString());
  dspin_min_->setValue(0.0);
  dspin_max_->setValue(0.0);
  edit_value_text_->clear();
  edit_link_to_->clear();

  spin_frame_id_->setValue(0);
  combo_frame_type_->setCurrentIndex(0);
  combo_byte_order_->setCurrentIndex(0);

  // Hide all groups; the caller (showNode / showFrame) re-shows what
  // is appropriate.
  basic_group_->hide();
  frame_group_->hide();
  node_group_->hide();
  scale_group_->hide();
  ext_group_->hide();
}

// ===========================================================================
// Public API: populate form from an icd::Node  (with editing support)
// ===========================================================================

void IcdPropertyPanel::showNode(icd::Node& node) {
  clearForm();
  current_node_ = &node;

  // ---- Populate basic info ----
  edit_name_->setText(QString::fromStdString(std::string(node.name())));
  edit_desc_->setText(QString::fromStdString(std::string(node.description())));

  // ---- Populate node properties ----
  spin_offset_->setValue(node.offset());
  spin_start_bit_->setValue(node.bit_offset());
  spin_bit_width_->setValue(node.bit_width());

  {
    const int idx = combo_type_->findText(QString::fromLatin1(valueTypeName(node.value_type())));
    if (idx >= 0) combo_type_->setCurrentIndex(idx);
  }
  {
    const int idx = combo_tag_->findText(QString::fromLatin1(tagName(node.tag())));
    if (idx >= 0) combo_tag_->setCurrentIndex(idx);
  }

  // ---- Populate scale section ----
  const auto& attrs = node.attrs();
  check_scaled_->setChecked(attrs.is_scaled);
  dspin_scale_a_->setValue(static_cast<double>(attrs.scale_a.value_or(0.0f)));
  dspin_scale_b_->setValue(static_cast<double>(attrs.scale_b.value_or(0.0f)));
  edit_unit_->setText(QString::fromStdString(attrs.unit));
  edit_scale_formula_->setText(QString::fromStdString(attrs.scale_formula));
  edit_scale_convertor_->setText(QString::fromStdString(attrs.scale_convertor));

  // ---- Populate extended properties ----
  edit_system_->setText(QString::fromStdString(attrs.system_name));
  combo_group_->setCurrentText(QString::fromStdString(attrs.group_name));
  dspin_min_->setValue(static_cast<double>(attrs.min.value_or(0.0f)));
  dspin_max_->setValue(static_cast<double>(attrs.max.value_or(0.0f)));
  edit_value_text_->setText(QString::fromStdString(attrs.value_text_list));
  edit_link_to_->setText(QString::fromStdString(attrs.link_to));

  // ---- Show the relevant groups ----
  basic_group_->show();
  node_group_->show();
  scale_group_->show();
  ext_group_->show();
  // frame_group_ stays hidden

  // ---- Wire editing signals (disconnected by clearForm above) ----
  auto cn = [this](auto* sender, auto signal, auto&& lambda) {
    node_connections_.append(
        connect(sender, signal, this, std::forward<decltype(lambda)>(lambda)));
  };

  // Name
  cn(edit_name_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      current_node_
          ->setName(edit_name_->text().toStdString());
      emit nodeModified();
    }
  });
  // Description
  cn(edit_desc_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      current_node_
          ->setDescription(edit_desc_->text().toStdString());
      emit nodeModified();
    }
  });

  // Offset
  cn(spin_offset_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
    if (current_node_) {
      current_node_->setOffset(val);
      emit nodeModified();
    }
  });
  // Start bit
  cn(spin_start_bit_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
    if (current_node_) {
      current_node_->setBitOffset(val);
      emit nodeModified();
    }
  });
  // Bit width
  cn(spin_bit_width_, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
    if (current_node_) {
      current_node_->setBitWidth(val);
      emit nodeModified();
    }
  });

  // Value type combo
  cn(combo_type_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int /*idx*/) {
    if (current_node_) {
      current_node_
          ->setValueType(valueTypeFromName(combo_type_->currentText().toStdString()));
      emit nodeModified();
    }
  });
  // Tag combo
  cn(combo_tag_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int /*idx*/) {
    if (current_node_) {
      current_node_
          ->setTag(tagFromName(combo_tag_->currentText().toStdString()));
      emit nodeModified();
    }
  });

  // Scaled checkbox
  cn(check_scaled_, &QCheckBox::toggled, [this](bool checked) {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.is_scaled = checked;
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });
  // Scale A
  cn(dspin_scale_a_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
     [this](double val) {
       if (current_node_) {
         auto a = current_node_->attrs();
         a.scale_a = static_cast<float>(val);
         current_node_->setAttrs(std::move(a));
         emit nodeModified();
       }
     });
  // Scale B
  cn(dspin_scale_b_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
     [this](double val) {
       if (current_node_) {
         auto a = current_node_->attrs();
         a.scale_b = static_cast<float>(val);
         current_node_->setAttrs(std::move(a));
         emit nodeModified();
       }
     });

  // Unit
  cn(edit_unit_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.unit = edit_unit_->text().toStdString();
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });
  // Scale formula
  cn(edit_scale_formula_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.scale_formula = edit_scale_formula_->text().toStdString();
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });
  // Scale convertor
  cn(edit_scale_convertor_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.scale_convertor = edit_scale_convertor_->text().toStdString();
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });

  // System name
  cn(edit_system_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.system_name = edit_system_->text().toStdString();
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });
  // Group name (editable QComboBox: use editingFinished to avoid per-char trigger)
  cn(combo_group_->lineEdit(), &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.group_name = combo_group_->currentText().toStdString();
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });
  // Min/Max are reserved fields — read-only, no signal connection
  // Value text list
  cn(edit_value_text_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.value_text_list = edit_value_text_->text().toStdString();
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });
  // Link to
  cn(edit_link_to_, &QLineEdit::editingFinished, [this]() {
    if (current_node_) {
      auto a = current_node_->attrs();
      a.link_to = edit_link_to_->text().toStdString();
      current_node_->setAttrs(std::move(a));
      emit nodeModified();
    }
  });
}

// ===========================================================================
// Public API: populate form from an icd::Frame  (read-only display for now)
// ===========================================================================

void IcdPropertyPanel::showFrame(icd::Frame& frame) {
  clearForm();
  current_frame_ = &frame;

  // ---- Populate basic info ----
  edit_name_->setText(QString::fromStdString(std::string(frame.name())));
  edit_desc_->setText(QString::fromStdString(std::string(frame.description())));

  // ---- Populate frame-specific fields ----
  spin_frame_id_->setValue(frame.id());
  spin_frame_id_->setReadOnly(true);  // ID change requires repo re-index; keep read-only

  combo_frame_type_->setCurrentIndex(frameTypeIndex(frame.type()));
  combo_byte_order_->setCurrentIndex(byteOrderIndex(frame.order()));

  // ---- Show only frame-relevant groups ----
  basic_group_->show();
  frame_group_->show();

  // ---- Wire editing signals ----
  clearNodeConnections();

  auto cn = [this](auto* sender, auto signal, auto&& lambda) {
    node_connections_.append(
        connect(sender, signal, this, std::forward<decltype(lambda)>(lambda)));
  };

  cn(combo_frame_type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
     [this](int idx) {
       if (current_frame_) {
         current_frame_->setType(frameTypeFromIndex(idx));
         emit frameModified();
       }
     });
  cn(combo_byte_order_, QOverload<int>::of(&QComboBox::currentIndexChanged),
     [this](int idx) {
       if (current_frame_) {
         current_frame_->setOrder(byteOrderFromIndex(idx));
         emit frameModified();
       }
     });
}

// ===========================================================================
// Public API: clear form to empty state
// ===========================================================================

void IcdPropertyPanel::clear() {
  clearForm();
}

}  // namespace etest::protocal
