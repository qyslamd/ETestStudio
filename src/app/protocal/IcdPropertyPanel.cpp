#include "IcdPropertyPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace etest::protocal {

IcdPropertyPanel::IcdPropertyPanel(QWidget* parent) : QWidget(parent) {
  initUi();
}

void IcdPropertyPanel::initUi() {
  auto* outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);

  auto* header = new QLabel(QStringLiteral("信号属性"), this);
  header->setStyleSheet("font-weight: bold; padding: 4px 8px;");

  auto* scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  form_widget_ = new QWidget(this);
  auto* form_layout = new QFormLayout(form_widget_);
  form_layout->setContentsMargins(8, 8, 8, 8);
  form_layout->setSpacing(6);
  form_layout->setLabelAlignment(Qt::AlignRight);

  auto add_line = [&](const QString& label, const QString& placeholder) {
    auto* edit = new QLineEdit(placeholder, form_widget_);
    edit->setReadOnly(true);
    form_layout->addRow(label, edit);
  };

  auto add_spin = [&](const QString& label, int value, int min, int max) {
    auto* spin = new QSpinBox(form_widget_);
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setReadOnly(true);
    form_layout->addRow(label, spin);
  };

  auto add_dspin = [&](const QString& label, double value) {
    auto* spin = new QDoubleSpinBox(form_widget_);
    spin->setRange(-1e9, 1e9);
    spin->setDecimals(8);
    spin->setValue(value);
    spin->setReadOnly(true);
    form_layout->addRow(label, spin);
  };

  auto add_combo = [&](const QString& label, const QStringList& items,
                       int index) {
    auto* combo = new QComboBox(form_widget_);
    combo->addItems(items);
    combo->setCurrentIndex(index);
    combo->setEnabled(false);
    form_layout->addRow(label, combo);
  };

  // -- Basic properties --
  auto* basic_group = new QGroupBox(QStringLiteral("基本信息"), form_widget_);
  auto* basic_form = new QFormLayout(basic_group);
  add_line(QStringLiteral("Name"), QStringLiteral("GNSS_Latitude"));
  add_spin(QStringLiteral("Offset"), 1, 0, 255);
  add_spin(QStringLiteral("StartBit"), 0, 0, 7);
  add_spin(QStringLiteral("BitWidth"), 21, 1, 64);
  add_combo(QStringLiteral("Type"),
            {QStringLiteral("byte"), QStringLiteral("int"),
             QStringLiteral("uint"), QStringLiteral("float"),
             QStringLiteral("double"), QStringLiteral("string"),
             QStringLiteral("bytes"), QStringLiteral("dword")},
            2);
  add_combo(QStringLiteral("Tag"),
            {QStringLiteral("none"), QStringLiteral("head"),
             QStringLiteral("length"), QStringLiteral("count"),
             QStringLiteral("sum"), QStringLiteral("xor"),
             QStringLiteral("signal_in_value")},
            0);
  form_layout->addRow(basic_group);

  // -- Scale properties --
  auto* scale_group = new QGroupBox(QStringLiteral("缩放"), form_widget_);
  auto* scale_form = new QFormLayout(scale_group);
  auto* scaled_cb = new QCheckBox(QStringLiteral("启用缩放"), form_widget_);
  scaled_cb->setChecked(true);
  scaled_cb->setEnabled(false);
  scale_form->addRow(scaled_cb);
  add_dspin(QStringLiteral("ScaleA"), 0.00017166);
  add_dspin(QStringLiteral("ScaleB"), 0.0);
  add_line(QStringLiteral("Unit"), QStringLiteral("°"));
  add_line(QStringLiteral("预览"), QStringLiteral("原始值 524288 = 物理值 90.0°"));
  form_layout->addRow(scale_group);

  // -- Extended properties --
  auto* ext_group = new QGroupBox(QStringLiteral("扩展属性"), form_widget_);
  auto* ext_form = new QFormLayout(ext_group);
  add_line(QStringLiteral("SystemName"), QStringLiteral("ARINC429总线通讯模块"));
  add_line(QStringLiteral("GroupName"), QStringLiteral("ISI-01#A429_IN1(110)"));
  add_line(QStringLiteral("Description"), QStringLiteral("GNSS Latitude"));
  add_dspin(QStringLiteral("Min"), -90.0);
  add_dspin(QStringLiteral("Max"), 90.0);
  add_line(QStringLiteral("ValueTextList"), QStringLiteral("110"));
  add_combo(QStringLiteral("LinkTo"),
            {QStringLiteral("6272T_00"), QStringLiteral("6272T_01"),
             QStringLiteral("6272T_02"), QStringLiteral("6272T_03")},
            0);
  form_layout->addRow(ext_group);

  scroll->setWidget(form_widget_);

  outer_layout->addWidget(header);
  outer_layout->addWidget(scroll, 1);
}

}  // namespace etest::protocal
