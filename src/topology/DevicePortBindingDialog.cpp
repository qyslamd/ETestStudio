#include "DevicePortBindingDialog.h"
#include "TopologyDocument.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace etest::topology {

DevicePortBindingDialog::DevicePortBindingDialog(
    TopologyDocument* doc, int deviceIndex, int portIndex,
    const QStringList& allFrames, QWidget* parent)
    : QDialog(parent), doc_(doc), device_index_(deviceIndex),
      port_index_(portIndex), all_frames_(allFrames) {
  setWindowTitle(QStringLiteral("绑定 ICD 帧"));
  setMinimumSize(500, 350);
  initUi();
  populateAvailableFrames(allFrames);
  populateBoundFrames();
}

void DevicePortBindingDialog::initUi() {
  auto* mainLayout = new QVBoxLayout(this);

  // ── 过滤栏 ──
  auto* filterRow = new QHBoxLayout;
  filterRow->addWidget(new QLabel(QStringLiteral("过滤:")));
  filter_combo_ = new QComboBox(this);
  filter_combo_->addItem(QStringLiteral("全部"));
  filter_combo_->addItem(QStringLiteral("A429"));
  filter_combo_->addItem(QStringLiteral("SERIAL"));
  filter_combo_->addItem(QStringLiteral("AD"));
  filter_combo_->addItem(QStringLiteral("DA"));
  filter_combo_->addItem(QStringLiteral("DISCRETE"));
  filter_combo_->addItem(QStringLiteral("MIL1553"));
  filter_combo_->setCurrentIndex(0);
  filterRow->addWidget(filter_combo_, 1);
  mainLayout->addLayout(filterRow);

  // ── 左右列表 ──
  auto* splitter = new QSplitter(Qt::Horizontal, this);

  auto* leftWidget = new QWidget(this);
  auto* leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(new QLabel(QStringLiteral("可用 ICD 帧:"), this));
  available_list_ = new QListWidget(this);
  available_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  leftLayout->addWidget(available_list_);
  splitter->addWidget(leftWidget);

  // ── 中间按钮 ──
  auto* btnWidget = new QWidget(this);
  auto* btnLayout = new QVBoxLayout(btnWidget);
  btnLayout->setAlignment(Qt::AlignCenter);
  add_btn_ = new QPushButton(QStringLiteral("→ 添加"), this);
  remove_btn_ = new QPushButton(QStringLiteral("← 移除"), this);
  btnLayout->addWidget(add_btn_);
  btnLayout->addWidget(remove_btn_);
  splitter->addWidget(btnWidget);

  auto* rightWidget = new QWidget(this);
  auto* rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->addWidget(new QLabel(QStringLiteral("已绑定帧:"), this));
  bound_list_ = new QListWidget(this);
  bound_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  rightLayout->addWidget(bound_list_);
  splitter->addWidget(rightWidget);

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  splitter->setStretchFactor(2, 1);
  mainLayout->addWidget(splitter, 1);

  // ── 按钮栏 ──
  auto* buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  mainLayout->addWidget(buttonBox);

  // ── 信号 ──
  connect(add_btn_, &QPushButton::clicked, this,
          &DevicePortBindingDialog::onAddFrames);
  connect(remove_btn_, &QPushButton::clicked, this,
          &DevicePortBindingDialog::onRemoveFrames);
  connect(filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DevicePortBindingDialog::onFilterChanged);
  connect(buttonBox, &QDialogButtonBox::accepted, this,
          &DevicePortBindingDialog::onAccept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DevicePortBindingDialog::populateAvailableFrames(
    const QStringList& allFrames) {
  available_list_->clear();
  int filterIdx = filter_combo_->currentIndex();
  for (const QString& frame : allFrames) {
    // 简单过滤：若 filterIdx > 0，只显示含过滤关键字的帧名
    if (filterIdx > 0) {
      QString keyword = filter_combo_->currentText();
      if (!frame.contains(keyword, Qt::CaseInsensitive)) continue;
    }
    available_list_->addItem(frame);
  }
}

void DevicePortBindingDialog::populateBoundFrames() {
  bound_list_->clear();
  QStringList bound = doc_->devicePortFrames(device_index_, port_index_);
  for (const QString& f : bound) {
    bound_list_->addItem(f);
  }
}

void DevicePortBindingDialog::onAddFrames() {
  QList<QListWidgetItem*> selected = available_list_->selectedItems();
  for (auto* item : selected) {
    // 已绑定的不加
    bool alreadyBound = false;
    for (int i = 0; i < bound_list_->count(); ++i) {
      if (bound_list_->item(i)->text() == item->text()) {
        alreadyBound = true;
        break;
      }
    }
    if (!alreadyBound) {
      bound_list_->addItem(item->text());
    }
  }
}

void DevicePortBindingDialog::onRemoveFrames() {
  QList<QListWidgetItem*> selected = bound_list_->selectedItems();
  for (auto* item : selected) {
    delete bound_list_->takeItem(bound_list_->row(item));
  }
}

void DevicePortBindingDialog::onFilterChanged(int /*index*/) {
  populateAvailableFrames(all_frames_);
}

void DevicePortBindingDialog::onAccept() {
  selected_frames_.clear();
  for (int i = 0; i < bound_list_->count(); ++i) {
    selected_frames_.append(bound_list_->item(i)->text());
  }
  accept();
}

}  // namespace etest::topology
