#include "SignalSelectionDialog.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <functional>
#include <QFormLayout>
#include <QPushButton>
#include <QStandardItem>
#include <QHeaderView>
#include <QLabel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include "core/SignalRegistry.h"
#include "icd/frame.hpp"
#include "icd/node.hpp"
#include "icd/repository.hpp"
#include "utils/SignalSyncHelper.h"

namespace etest::app {

SignalSelectionDialog::SignalSelectionDialog(
    const etest::core::SignalRegistry* registry,
    const icd::Repository* repository, QWidget* parent)
    : QDialog(parent), registry_(registry), repository_(repository) {
  setWindowTitle(QStringLiteral("选择信号"));
  setMinimumSize(520, 480);

  auto* mainLayout = new QVBoxLayout(this);

  // ── 选择区（表单） ──
  auto* form = new QFormLayout;

  device_combo_ = new QComboBox(this);
  form->addRow(QStringLiteral("设备:"), device_combo_);

  port_combo_ = new QComboBox(this);
  form->addRow(QStringLiteral("端口:"), port_combo_);

  frame_combo_ = new QComboBox(this);
  form->addRow(QStringLiteral("帧:"), frame_combo_);

  mainLayout->addLayout(form);

  // ── 信号树 ──
  node_model_ = new QStandardItemModel(this);
  node_model_->setHorizontalHeaderLabels({QStringLiteral("信号节点")});
  node_tree_ = new QTreeView(this);
  node_tree_->setModel(node_model_);
  node_tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  node_tree_->header()->setStretchLastSection(true);
  mainLayout->addWidget(node_tree_, 1);

  // ── 信息区 ──
  info_label_ = new QLabel(this);
  info_label_->setWordWrap(true);
  mainLayout->addWidget(info_label_);

  uuid_label_ = new QLabel(this);
  uuid_label_->setWordWrap(true);
  uuid_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  mainLayout->addWidget(uuid_label_);

  // ── 按钮 ──
  auto* buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
  mainLayout->addWidget(buttonBox);

  // ── 信号连接 ──
  connect(device_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SignalSelectionDialog::onDeviceChanged);
  connect(port_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SignalSelectionDialog::onPortChanged);
  connect(frame_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SignalSelectionDialog::onFrameChanged);
  connect(node_tree_->selectionModel(),
          &QItemSelectionModel::currentChanged, this,
          &SignalSelectionDialog::onNodeSelected);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  // ── 初始加载 ──
  populateDevices();
}

void SignalSelectionDialog::populateDevices() {
  device_combo_->clear();
  port_combo_->clear();
  frame_combo_->clear();
  node_model_->clear();
  info_label_->clear();
  uuid_label_->clear();

  QStringList ids = registry_->registeredDeviceIds();
  for (const QString& id : ids) {
    QString name = registry_->deviceName(id);
    // 显示 "设备名 (id)"，userData 存 id
    QString label = name.isEmpty() ? id : QStringLiteral("%1 (%2)").arg(name, id);
    device_combo_->addItem(label, id);
  }

  if (device_combo_->count() > 0) {
    device_combo_->setCurrentIndex(0);
  }
}

void SignalSelectionDialog::onDeviceChanged(int index) {
  port_combo_->clear();
  frame_combo_->clear();
  node_model_->clear();
  info_label_->clear();
  uuid_label_->clear();

  if (index < 0) return;
  QString deviceId = device_combo_->itemData(index).toString();
  populatePorts(deviceId);
}

void SignalSelectionDialog::populatePorts(const QString& deviceId) {
  // 从 registry 遍历端口绑定，收集该设备的端口
  QStringList ports;
  registry_->forEachPortBinding(
      [&](const QString& dId, const QString& portName,
          const QStringList& /*frameNames*/) {
        if (dId == deviceId && !ports.contains(portName)) {
          ports.append(portName);
        }
      });

  for (const QString& p : ports) {
    port_combo_->addItem(p);
  }
  if (port_combo_->count() > 0) {
    port_combo_->setCurrentIndex(0);
  }
}

void SignalSelectionDialog::onPortChanged(int index) {
  frame_combo_->clear();
  node_model_->clear();
  info_label_->clear();
  uuid_label_->clear();

  if (index < 0 || device_combo_->currentIndex() < 0) return;
  QString deviceId = device_combo_->currentData().toString();
  QString portName = port_combo_->currentText();
  populateFrames(deviceId, portName);
}

void SignalSelectionDialog::populateFrames(const QString& deviceId,
                                            const QString& portName) {
  registry_->forEachPortBinding(
      [&](const QString& dId, const QString& pName,
          const QStringList& frameNames) {
        if (dId == deviceId && pName == portName) {
          for (const QString& f : frameNames) {
            frame_combo_->addItem(f);
          }
        }
      });

  if (frame_combo_->count() > 0) {
    frame_combo_->setCurrentIndex(0);
  }
}

void SignalSelectionDialog::onFrameChanged(int index) {
  node_model_->clear();
  info_label_->clear();
  uuid_label_->clear();

  if (index < 0) return;
  QString frameName = frame_combo_->currentText();
  populateNodes(frameName);
}

void SignalSelectionDialog::populateNodes(const QString& frameName) {
  const auto* frame = repository_->find(frameName.toStdString());
  if (!frame) return;

  // 递归构建树：遍历 roots
  std::function<void(QStandardItem*, const icd::Node*)> addChildren;
  addChildren = [&](QStandardItem* parent, const icd::Node* node) {
    auto* item = new QStandardItem(
        QString::fromUtf8(node->name().data(),
                          static_cast<int>(node->name().size())));
    // 存 node 指针供选中时查询
    item->setData(reinterpret_cast<quintptr>(node), Qt::UserRole);
    parent->appendRow(item);

    for (const auto& child : node->children()) {
      addChildren(item, child.get());
    }
  };

  for (const auto& root : frame->roots()) {
    auto* rootItem = new QStandardItem(
        QString::fromUtf8(root->name().data(),
                          static_cast<int>(root->name().size())));
    rootItem->setData(reinterpret_cast<quintptr>(root.get()), Qt::UserRole);
    node_model_->appendRow(rootItem);
    for (const auto& child : root->children()) {
      addChildren(rootItem, child.get());
    }
    node_tree_->expand(rootItem->index());
  }

  node_tree_->expandAll();
}

void SignalSelectionDialog::onNodeSelected(const QModelIndex& index) {
  if (!index.isValid()) {
    current_uuid_.clear();
    updateInfoLabel();
    updateOkButton();
    return;
  }

  // 从选中节点向上构建 nodePath
  QStringList pathSegments;
  QModelIndex cur = index;
  while (cur.isValid()) {
    pathSegments.prepend(cur.data().toString());
    cur = cur.parent();
  }
  QString nodePath = pathSegments.join('/');

  // 获取 deviceId + portName + frameName
  if (device_combo_->currentIndex() < 0 ||
      port_combo_->currentIndex() < 0 ||
      frame_combo_->currentIndex() < 0) {
    return;
  }

  QString deviceId = device_combo_->currentData().toString();
  QString portName = port_combo_->currentText();
  QString frameName = frame_combo_->currentText();

  current_uuid_ = etest::core::SignalRegistry::computeUuid(
      deviceId, portName, frameName, nodePath);
  updateInfoLabel();
  updateOkButton();
}

void SignalSelectionDialog::updateInfoLabel() {
  if (current_uuid_.isEmpty()) {
    info_label_->setText(QString());
    return;
  }

  // 显示选中信号的元信息（取自选中节点）
  QModelIndex cur = node_tree_->currentIndex();
  if (!cur.isValid()) return;

  quintptr ptr = cur.data(Qt::UserRole).value<quintptr>();
  auto* node = reinterpret_cast<const icd::Node*>(ptr);
  if (!node) return;

  // 值类型 / 偏移 / 位宽（使用 icd::ValueType 可读名）
  auto vt = node->value_type();
  QString typeName;
  switch (vt) {
    case icd::ValueType::boolean:  typeName = QStringLiteral("boolean"); break;
    case icd::ValueType::byte_:    typeName = QStringLiteral("uint8");   break;
    case icd::ValueType::bytes:    typeName = QStringLiteral("bytes");   break;
    case icd::ValueType::word:     typeName = QStringLiteral("uint16");  break;
    case icd::ValueType::shortint: typeName = QStringLiteral("int16");   break;
    case icd::ValueType::smallint: typeName = QStringLiteral("smallint");break;
    case icd::ValueType::longword: typeName = QStringLiteral("uint32");  break;
    case icd::ValueType::integer:  typeName = QStringLiteral("int32");   break;
    case icd::ValueType::ulong_:   typeName = QStringLiteral("uint64");  break;
    case icd::ValueType::single:   typeName = QStringLiteral("float");   break;
    case icd::ValueType::double_:  typeName = QStringLiteral("double");  break;
    case icd::ValueType::string_:  typeName = QStringLiteral("string");  break;
    default:                       typeName = QStringLiteral("unknown"); break;
  }
  info_label_->setText(
      QStringLiteral("类型: %1 | Offset: %2 | Bit: %3~%4")
          .arg(typeName)
          .arg(node->offset())
          .arg(node->bit_offset())
          .arg(node->bit_offset() + node->bit_width() - 1));
}

void SignalSelectionDialog::updateOkButton() {
  auto* btn = findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
  if (btn) {
    bool valid = !current_uuid_.isEmpty();
    btn->setEnabled(valid);
    uuid_label_->setText(
        valid ? QStringLiteral("UUID: %1").arg(current_uuid_) : QString());
  }
}

}  // namespace etest::app
