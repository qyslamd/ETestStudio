#pragma once

#include <QWidget>
#include <QVector>

#include <icd/node.hpp>
#include <icd/frame.hpp>

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QFormLayout;
class QGroupBox;

namespace etest::protocal {

class IcdPropertyPanel : public QWidget {
  Q_OBJECT
 public:
  explicit IcdPropertyPanel(QWidget* parent = nullptr);

  void showNode(icd::Node& node);
  void showFrame(const icd::Frame& frame);
  void clear();

 signals:
  void nodeModified();

 private:
  void initUi();
  void clearForm();
  void clearNodeConnections();

  QWidget* form_widget_ = nullptr;
  QFormLayout* form_layout_ = nullptr;

  // Group boxes for section visibility
  QGroupBox* basic_group_ = nullptr;
  QGroupBox* frame_group_ = nullptr;
  QGroupBox* node_group_ = nullptr;
  QGroupBox* scale_group_ = nullptr;
  QGroupBox* ext_group_ = nullptr;

  // Editor widgets for node/frame properties
  QLineEdit* edit_name_ = nullptr;
  QLineEdit* edit_desc_ = nullptr;
  QSpinBox* spin_offset_ = nullptr;
  QSpinBox* spin_start_bit_ = nullptr;
  QSpinBox* spin_bit_width_ = nullptr;
  QComboBox* combo_type_ = nullptr;
  QComboBox* combo_tag_ = nullptr;
  QCheckBox* check_scaled_ = nullptr;
  QDoubleSpinBox* dspin_scale_a_ = nullptr;
  QDoubleSpinBox* dspin_scale_b_ = nullptr;
  QLineEdit* edit_unit_ = nullptr;
  QLineEdit* edit_system_ = nullptr;
  QLineEdit* edit_group_ = nullptr;
  QLineEdit* edit_value_text_ = nullptr;
  QLineEdit* edit_link_to_ = nullptr;
  QDoubleSpinBox* dspin_min_ = nullptr;
  QDoubleSpinBox* dspin_max_ = nullptr;
  QLineEdit* edit_scale_formula_ = nullptr;
  QLineEdit* edit_scale_convertor_ = nullptr;

  // Frame-specific widgets
  QSpinBox* spin_frame_id_ = nullptr;
  QComboBox* combo_frame_type_ = nullptr;
  QComboBox* combo_byte_order_ = nullptr;

  icd::Node* current_node_ = nullptr;
  QVector<QMetaObject::Connection> node_connections_;
};

}  // namespace etest::protocal
