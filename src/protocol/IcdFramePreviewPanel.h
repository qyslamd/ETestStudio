#pragma once

#include <QWidget>

#include <icd/frame.hpp>

class QLabel;
class QLineEdit;
class QListWidget;
class QToolButton;

namespace etest::protocol {

// 帧报文预览面板：输入十六进制报文，按当前帧协议解析出各字段值。
// 属于帧级视图，与位布局视图、节点树、属性面板并列。
class IcdFramePreviewPanel : public QWidget {
  Q_OBJECT
 public:
  explicit IcdFramePreviewPanel(QWidget* parent = nullptr);

  // 设置当前用于解析的帧。传入 nullptr 时清空并禁用输入。
  void setFrame(const icd::Frame* frame);

 signals:
  // 选中某行字段时发出，携带该字段对应的 node 指针（可能为 nullptr）。
  void nodeActivated(const icd::Node* node);

 private:
  void initUi();
  void runPreview();
  void clearResult();

  QLineEdit* hex_input_ = nullptr;
  QToolButton* decode_btn_ = nullptr;
  QLabel* status_label_ = nullptr;
  QListWidget* result_list_ = nullptr;

  const icd::Frame* frame_ = nullptr;
  // 解析结果行对应的 node 指针，顺序与 result_list_ 行一致。
  QVector<const icd::Node*> row_nodes_;
};

}  // namespace etest::protocol
