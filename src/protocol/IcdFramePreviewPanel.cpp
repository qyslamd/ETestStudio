#include "IcdFramePreviewPanel.h"

#include "IcdFramePreview.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <string>

namespace etest::protocol {

IcdFramePreviewPanel::IcdFramePreviewPanel(QWidget* parent) : QFrame(parent) {
  initUi();
}

void IcdFramePreviewPanel::initUi() {
  setObjectName(QStringLiteral("icdFramePreviewPanel"));

  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  // 输入行：hex 输入框 + 解析按钮
  auto* input_row = new QHBoxLayout();
  input_row->setSpacing(6);
  hex_input_ = new QLineEdit(this);
  hex_input_->setObjectName(QStringLiteral("icdFramePreviewHexInput"));
  hex_input_->setPlaceholderText(
      QStringLiteral("输入十六进制字节，例如 01 02 0A 0B"));
  input_row->addWidget(hex_input_, 1);

  decode_btn_ = new QToolButton(this);
  decode_btn_->setText(QStringLiteral("解析"));
  decode_btn_->setObjectName(QStringLiteral("icdFramePreviewDecodeBtn"));
  input_row->addWidget(decode_btn_);
  outer->addLayout(input_row);

  // 状态行
  status_label_ = new QLabel(this);
  status_label_->setObjectName(QStringLiteral("icdFramePreviewStatus"));
  outer->addWidget(status_label_);

  // 结果列表
  result_list_ = new QListWidget(this);
  result_list_->setFrameShape(QFrame::NoFrame);
  result_list_->setObjectName(QStringLiteral("icdFramePreviewResult"));
  outer->addWidget(result_list_, 1);

  connect(decode_btn_, &QToolButton::clicked, this,
          &IcdFramePreviewPanel::runPreview);
  connect(hex_input_, &QLineEdit::returnPressed, this,
          &IcdFramePreviewPanel::runPreview);
  connect(result_list_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row < 0 || row >= row_nodes_.size()) {
      emit nodeActivated(nullptr);
      return;
    }
    emit nodeActivated(row_nodes_[row]);
  });

  clearResult();
}

void IcdFramePreviewPanel::clearResult() {
  result_list_->clear();
  row_nodes_.clear();
}

void IcdFramePreviewPanel::setFrame(const icd::Frame* frame) {
  frame_ = frame;
  clearResult();
  if (frame) {
    status_label_->setText(
        QStringLiteral("当前帧: %1")
            .arg(QString::fromStdString(std::string(frame->name()))));
    hex_input_->setEnabled(true);
    decode_btn_->setEnabled(true);
  } else {
    status_label_->setText(QStringLiteral("未加载帧"));
    hex_input_->setEnabled(false);
    decode_btn_->setEnabled(false);
  }
}

void IcdFramePreviewPanel::runPreview() {
  if (!frame_) {
    status_label_->setText(QStringLiteral("未加载帧"));
    return;
  }
  const auto bytes = parseHexBytes(hex_input_->text());
  if (!bytes) {
    status_label_->setText(
        QStringLiteral("十六进制格式无效：长度需为偶数且仅含 0-9a-f"));
    clearResult();
    return;
  }

  const auto rows = decodeFramePreviewWithNodes(*frame_, *bytes);
  clearResult();
  int failed = 0;
  const QString fail_marker = QStringLiteral("解码失败");
  for (const auto& row : rows) {
    result_list_->addItem(row.first);
    row_nodes_.append(row.second);
    if (row.first.contains(fail_marker)) {
      ++failed;
    }
  }
  const int ok = rows.size() - failed;
  if (failed == 0) {
    status_label_->setText(
        QStringLiteral("已解析 %1 个字段（全部成功）").arg(rows.size()));
  } else {
    status_label_->setText(
        QStringLiteral("已解析 %1 个字段（成功 %2，失败 %3）")
            .arg(rows.size())
            .arg(ok)
            .arg(failed));
  }
}

}  // namespace etest::protocol
