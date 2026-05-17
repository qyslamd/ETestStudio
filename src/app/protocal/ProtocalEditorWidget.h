#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QSplitter;

namespace etest::protocal {

class IcdNodeTreeWidget;
class IcdBitLayoutView;
class IcdPropertyPanel;

class ProtocalEditorWidget : public QWidget {
  Q_OBJECT
 public:
  explicit ProtocalEditorWidget(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();

  QSplitter* splitter_ = nullptr;
  IcdNodeTreeWidget* node_tree_ = nullptr;
  IcdBitLayoutView* bit_view_ = nullptr;
  IcdPropertyPanel* property_panel_ = nullptr;
  QLabel* status_label_ = nullptr;
  QLabel* frame_name_label_ = nullptr;
  QComboBox* frame_type_combo_ = nullptr;
  QComboBox* byte_order_combo_ = nullptr;
};

}  // namespace etest::protocal
