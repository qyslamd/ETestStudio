#include "PropertyPanelWidget.h"
#include "ComboBoxDelegate.h"
#include "TopologyDocument.h"
#include "UndoCommands.h"
#include "topology_items.h"


#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace etest::topology {

PropertyPanelWidget::PropertyPanelWidget(TopologyDocument* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* title = new QLabel(QStringLiteral("属性面板"), this);
  title->setObjectName(QStringLiteral("topologySectionHeader"));
  layout->addWidget(title);

  stack_ = new QStackedWidget(this);
  layout->addWidget(stack_);

  buildEmptyPage();
  buildUutPage();
  buildPortPage();
  buildDevicePage();
  buildConnectionPage();
  buildDevicePortPage();
  buildMonitorPage();

  stack_->setCurrentIndex(PageEmpty);
}

void PropertyPanelWidget::showPropertiesFor(QGraphicsItem* item) {
  // Save pending device edits when leaving device page.
  // IMPORTANT: skip rebuild if clicking on the same device — this allows
  // drag-to-move to work without the scene being cleared mid-press.
  if (editing_device_index_ >= 0) {
    bool sameDevice = false;
    if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
      sameDevice = (dev->deviceIndex() == editing_device_index_);
    }
    if (!sameDevice) {
      int savedIdx = editing_device_index_;
      editing_device_index_ = -1;

      if (device_dirty_) {
        bool wasBlocked = doc_->undoStack()->blockSignals(true);
        applyDeviceProperties(savedIdx);
        applyDevicePorts(savedIdx);
        doc_->undoStack()->blockSignals(wasBlocked);

        emit documentChanged();
      }
      return;
    }
  }

  editing_uut_index_ = -1;
  editing_port_product_ = -1;
  editing_port_index_ = -1;
  editing_device_index_ = -1;
  editing_device_port_device_ = -1;
  editing_device_port_index_ = -1;
  editing_monitor_index_ = -1;

  if (!item) {
    stack_->setCurrentIndex(PageEmpty);
    return;
  }

  if (auto* uut = qgraphicsitem_cast<UutItem*>(item)) {
    editing_uut_index_ = uut->productIndex();
    auto* prod = doc_->product(editing_uut_index_);
    if (prod) {
      uut_name_edit_->blockSignals(true);
      uut_name_edit_->setText(prod->name);
      uut_name_edit_->blockSignals(false);
    }
    stack_->setCurrentIndex(PageUut);
    return;
  }

  if (auto* port = qgraphicsitem_cast<PortItem*>(item)) {
    editing_port_product_ = port->productIndex();
    editing_port_index_ = port->portIndex();
    auto* prod = doc_->product(editing_port_product_);
    if (prod && editing_port_index_ < prod->ports.size()) {
      const auto& p = prod->ports[editing_port_index_];

      // Refresh device types from document
      port_allowed_types_edit_->blockSignals(true);
      port_allowed_types_edit_->clear();
      QStringList knownTypes;
      for (int i = 0; i < doc_->deviceCount(); ++i) {
        auto* d = doc_->device(i);
        if (d && !d->deviceType.isEmpty())
          knownTypes.append(d->deviceType);
      }
      knownTypes.removeDuplicates();
      port_allowed_types_edit_->addItems(knownTypes);
      port_allowed_types_edit_->setCurrentText(
          p.allowedDeviceTypes.join(QStringLiteral(", ")));
      port_allowed_types_edit_->blockSignals(false);

      port_name_edit_->blockSignals(true);
      port_name_edit_->setText(p.name);
      port_name_edit_->blockSignals(false);
      port_direction_combo_->blockSignals(true);
      port_direction_combo_->setCurrentIndex(static_cast<int>(p.direction));
      port_direction_combo_->blockSignals(false);
      port_function_combo_->blockSignals(true);
      port_function_combo_->setCurrentIndex(static_cast<int>(p.functionType));
      port_function_combo_->blockSignals(false);
    }
    stack_->setCurrentIndex(PagePort);
    return;
  }

  if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
    editing_device_index_ = dev->deviceIndex();
    auto* d = doc_->device(editing_device_index_);
    if (d) {
      // Save current state for undo support
      saved_device_properties_ = d->properties;
      saved_device_ports_ = d->ports;
      device_dirty_ = false;

      device_name_edit_->blockSignals(true);
      device_name_edit_->setText(d->name);
      device_name_edit_->blockSignals(false);
      device_type_edit_->setText(d->deviceType);

      device_props_table_->setUpdatesEnabled(false);
      device_props_table_->blockSignals(true);
      device_props_table_->setRowCount(d->properties.size());
      for (int r = 0; r < d->properties.size(); ++r) {
        device_props_table_->setItem(
            r, 0, new QTableWidgetItem(d->properties[r].first));
        device_props_table_->setItem(
            r, 1, new QTableWidgetItem(d->properties[r].second));
      }
      device_props_table_->blockSignals(false);
      device_props_table_->setUpdatesEnabled(true);

      // Load device ports via model (no QComboBox creation)
      device_port_view_->setUpdatesEnabled(false);
      device_port_model_->blockSignals(true);
      const QStringList dirNames = {QStringLiteral("Input"),
                                    QStringLiteral("Output"),
                                    QStringLiteral("Bidirectional")};
      device_port_model_->removeRows(0, device_port_model_->rowCount());
      for (int r = 0; r < d->ports.size(); ++r) {
        device_port_model_->insertRow(r);
        device_port_model_->setData(device_port_model_->index(r, 0),
                                    d->ports[r].name);
        device_port_model_->setData(
            device_port_model_->index(r, 1),
            dirNames[static_cast<int>(d->ports[r].direction)]);
        device_port_model_->setData(
            device_port_model_->index(r, 2),
            functionTypeToString(d->ports[r].functionType));
      }
      device_port_model_->blockSignals(false);
      device_port_view_->resizeColumnsToContents();
      device_port_view_->setUpdatesEnabled(true);
    }
    stack_->setCurrentIndex(PageDevice);
    return;
  }

  if (auto* devPort = qgraphicsitem_cast<DevicePortItem*>(item)) {
    editing_device_port_device_ = devPort->deviceIndex();
    editing_device_port_index_ = devPort->portIndex();
    auto* dev = doc_->device(editing_device_port_device_);
    if (dev && editing_device_port_index_ < dev->ports.size()) {
      const auto& dp = dev->ports[editing_device_port_index_];
      devport_name_edit_->blockSignals(true);
      devport_name_edit_->setText(dp.name);
      devport_name_edit_->blockSignals(false);
      devport_direction_combo_->blockSignals(true);
      devport_direction_combo_->setCurrentIndex(static_cast<int>(dp.direction));
      devport_direction_combo_->blockSignals(false);
      devport_function_combo_->blockSignals(true);
      devport_function_combo_->setCurrentIndex(
          static_cast<int>(dp.functionType));
      devport_function_combo_->blockSignals(false);
    }
    stack_->setCurrentIndex(PageDevicePort);
    return;
  }

  if (auto* conn = qgraphicsitem_cast<ConnectionItem*>(item)) {
    auto* src = conn->sourcePort();
    auto* tgt = conn->targetDevice();
    if (src) {
      auto* prod = doc_->product(src->productIndex());
      conn_source_label_->setText(
          prod ? QStringLiteral("%1 / %2").arg(
                     prod->name, prod->ports[src->portIndex()].name)
               : QStringLiteral("?"));
    }
    if (tgt) {
      auto* d = doc_->device(tgt->deviceIndex());
      conn_target_label_->setText(
          d ? QStringLiteral("%1 (%2)").arg(d->name, d->deviceType)
            : QStringLiteral("?"));
    }
    conn_device_port_label_->setText(conn->devicePort());
    stack_->setCurrentIndex(PageConnection);
    return;
  }

  if (auto* mon = qgraphicsitem_cast<MonitorItem*>(item)) {
    editing_monitor_index_ = mon->monitorIndex();
    auto* m = doc_->monitor(editing_monitor_index_);
    if (m) {
      monitor_name_edit_->blockSignals(true);
      monitor_name_edit_->setText(m->name);
      monitor_name_edit_->blockSignals(false);
      monitor_type_label_->setText(m->deviceType);

      // Populate taps table
      monitor_taps_table_->setUpdatesEnabled(false);
      monitor_taps_table_->blockSignals(true);
      monitor_taps_table_->setRowCount(m->taps.size());
      for (int r = 0; r < m->taps.size(); ++r) {
        const auto& tap = m->taps[r];
        monitor_taps_table_->setItem(
            r, 0, new QTableWidgetItem(
                QStringLiteral("%1:%2").arg(tap.productName, tap.portName)));
        monitor_taps_table_->setItem(
            r, 1, new QTableWidgetItem(
                QStringLiteral("%1:%2").arg(tap.deviceName, tap.devicePort)));
      }
      monitor_taps_table_->blockSignals(false);
      monitor_taps_table_->setUpdatesEnabled(true);
    }
    stack_->setCurrentIndex(PageMonitor);
    return;
  }

  stack_->setCurrentIndex(PageEmpty);
}

void PropertyPanelWidget::clearPanel() {
  stack_->setCurrentIndex(PageEmpty);
}

// ── Page builders ──────────────────────────────────────────────

void PropertyPanelWidget::buildEmptyPage() {
  auto* w = new QWidget(this);
  w->setObjectName("emptyPage");
  auto* lay = new QVBoxLayout(w);
  auto* lbl = new QLabel(QStringLiteral("未选中任何元素"), w);
  lbl->setAlignment(Qt::AlignCenter);
  lay->addWidget(lbl);
  stack_->addWidget(w);
}

void PropertyPanelWidget::buildUutPage() {
  auto* w = new QWidget(this);
  w->setObjectName("uutPage");
  auto* lay = new QFormLayout(w);

  uut_name_edit_ = new QLineEdit(w);
  connect(uut_name_edit_, &QLineEdit::editingFinished, this,
          &PropertyPanelWidget::onUutNameChanged);
  lay->addRow(QStringLiteral("名称"), uut_name_edit_);

  stack_->addWidget(w);
}

void PropertyPanelWidget::buildPortPage() {
  auto* w = new QWidget(this);
  w->setObjectName("portPage");
  auto* lay = new QFormLayout(w);

  port_name_edit_ = new QLineEdit(w);
  connect(port_name_edit_, &QLineEdit::editingFinished, this,
          &PropertyPanelWidget::onPortNameChanged);
  lay->addRow(QStringLiteral("名称"), port_name_edit_);

  port_direction_combo_ = new QComboBox(w);
  port_direction_combo_->addItem(QStringLiteral("Input"));
  port_direction_combo_->addItem(QStringLiteral("Output"));
  port_direction_combo_->addItem(QStringLiteral("Bidirectional"));
  connect(port_direction_combo_, &QComboBox::currentTextChanged, this,
          [this](const QString&) { onPortDirectionChanged(); });
  lay->addRow(QStringLiteral("方向"), port_direction_combo_);

  port_allowed_types_edit_ = new QComboBox(w);
  port_allowed_types_edit_->setEditable(true);
  port_allowed_types_edit_->setInsertPolicy(QComboBox::NoInsert);
  port_allowed_types_edit_->lineEdit()->setPlaceholderText(
      QStringLiteral("如: EPH6272T, EPH5272"));
  connect(port_allowed_types_edit_, QOverload<int>::of(&QComboBox::activated),
          this, [this](int) { onPortAllowedTypesChanged(); });
  connect(port_allowed_types_edit_->lineEdit(), &QLineEdit::editingFinished,
          this, &PropertyPanelWidget::onPortAllowedTypesChanged);
  lay->addRow(QStringLiteral("允许设备类型"), port_allowed_types_edit_);

  port_function_combo_ = new QComboBox(w);
  for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft) {
    port_function_combo_->addItem(
        functionTypeToString(static_cast<FunctionType>(ft)));
  }
  connect(port_function_combo_, &QComboBox::currentTextChanged, this,
          [this](const QString&) { onPortFunctionTypeChanged(); });
  lay->addRow(QStringLiteral("功能类型"), port_function_combo_);

  stack_->addWidget(w);
}

void PropertyPanelWidget::buildDevicePage() {
  auto* w = new QWidget(this);
  w->setObjectName("devicePage");
  auto* lay = new QVBoxLayout(w);

  auto* form = new QFormLayout();
  device_name_edit_ = new QLineEdit(w);
  connect(device_name_edit_, &QLineEdit::editingFinished, this,
          &PropertyPanelWidget::onDeviceNameChanged);
  form->addRow(QStringLiteral("名称"), device_name_edit_);

  device_type_edit_ = new QLineEdit(w);
  device_type_edit_->setReadOnly(true);
  form->addRow(QStringLiteral("设备类型"), device_type_edit_);
  lay->addLayout(form);

  auto* propsGroup = new QGroupBox(QStringLiteral("属性"), w);
  auto* propsLay = new QVBoxLayout(propsGroup);

  device_props_table_ = new QTableWidget(0, 2, w);
  device_props_table_->setHorizontalHeaderLabels(
      {QStringLiteral("键"), QStringLiteral("值")});
  device_props_table_->horizontalHeader()->setStretchLastSection(true);
  device_props_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  connect(device_props_table_, &QTableWidget::itemChanged, this,
          [this]() { device_dirty_ = true; });
  propsLay->addWidget(device_props_table_);

  auto* btnLay = new QHBoxLayout();
  add_prop_btn_ = new QPushButton(QStringLiteral("+"), w);
  connect(add_prop_btn_, &QPushButton::clicked, this,
          &PropertyPanelWidget::onAddPropertyRow);
  remove_prop_btn_ = new QPushButton(QStringLiteral("-"), w);
  connect(remove_prop_btn_, &QPushButton::clicked, this,
          &PropertyPanelWidget::onRemovePropertyRow);
  btnLay->addWidget(add_prop_btn_);
  btnLay->addWidget(remove_prop_btn_);
  btnLay->addStretch();
  propsLay->addLayout(btnLay);

  lay->addWidget(propsGroup);

  auto* portGroup = new QGroupBox(QStringLiteral("设备端口"), w);
  auto* portLay = new QVBoxLayout(portGroup);

  device_port_view_ = new QTableView(w);
  device_port_view_->horizontalHeader()->setStretchLastSection(true);
  device_port_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  device_port_view_->setAlternatingRowColors(true);
  device_port_model_ = new QStandardItemModel(0, 3, this);
  device_port_model_->setHorizontalHeaderLabels({QStringLiteral("名称"),
                                                 QStringLiteral("方向"),
                                                 QStringLiteral("功能类型")});
  device_port_view_->setModel(device_port_model_);
  connect(device_port_model_, &QStandardItemModel::itemChanged, this,
          [this]() { device_dirty_ = true; });

  {
    QStringList funcItems;
    for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft)
      funcItems << functionTypeToString(static_cast<FunctionType>(ft));
    direction_delegate_ =
        new ComboBoxDelegate({QStringLiteral("Input"), QStringLiteral("Output"),
                              QStringLiteral("Bidirectional")},
                             this);
    function_delegate_ = new ComboBoxDelegate(funcItems, this);
  }
  device_port_view_->setItemDelegateForColumn(1, direction_delegate_);
  device_port_view_->setItemDelegateForColumn(2, function_delegate_);

  portLay->addWidget(device_port_view_);

  auto* portBtnLay = new QHBoxLayout();
  add_port_btn_ = new QPushButton(QStringLiteral("+"), w);
  connect(add_port_btn_, &QPushButton::clicked, this,
          &PropertyPanelWidget::onAddDevicePortRow);
  remove_port_btn_ = new QPushButton(QStringLiteral("-"), w);
  connect(remove_port_btn_, &QPushButton::clicked, this,
          &PropertyPanelWidget::onRemoveDevicePortRow);
  portBtnLay->addWidget(add_port_btn_);
  portBtnLay->addWidget(remove_port_btn_);
  portBtnLay->addStretch();
  portLay->addLayout(portBtnLay);

  lay->addWidget(portGroup);
  stack_->addWidget(w);
}

void PropertyPanelWidget::buildConnectionPage() {
  auto* w = new QWidget(this);
  w->setObjectName("connectionPage");
  auto* lay = new QFormLayout(w);

  conn_source_label_ = new QLabel(QStringLiteral("-"), w);
  lay->addRow(QStringLiteral("源"), conn_source_label_);

  conn_target_label_ = new QLabel(QStringLiteral("-"), w);
  lay->addRow(QStringLiteral("目标"), conn_target_label_);

  conn_device_port_label_ = new QLabel(QStringLiteral("-"), w);
  lay->addRow(QStringLiteral("设备端口"), conn_device_port_label_);

  stack_->addWidget(w);
}

void PropertyPanelWidget::buildDevicePortPage() {
  auto* w = new QWidget(this);
  w->setObjectName("devicePortPage");
  auto* lay = new QFormLayout(w);

  devport_name_edit_ = new QLineEdit(w);
  connect(devport_name_edit_, &QLineEdit::editingFinished, this,
          &PropertyPanelWidget::onDevicePortNameChanged);
  lay->addRow(QStringLiteral("端口名称"), devport_name_edit_);

  devport_direction_combo_ = new QComboBox(w);
  devport_direction_combo_->addItem(QStringLiteral("Input"));
  devport_direction_combo_->addItem(QStringLiteral("Output"));
  devport_direction_combo_->addItem(QStringLiteral("Bidirectional"));
  connect(devport_direction_combo_, &QComboBox::currentTextChanged, this,
          [this](const QString&) { onDevicePortDirectionChanged(); });
  lay->addRow(QStringLiteral("方向"), devport_direction_combo_);

  devport_function_combo_ = new QComboBox(w);
  for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft) {
    devport_function_combo_->addItem(
        functionTypeToString(static_cast<FunctionType>(ft)));
  }
  connect(devport_function_combo_, &QComboBox::currentTextChanged, this,
          [this](const QString&) { onDevicePortFunctionTypeChanged(); });
  lay->addRow(QStringLiteral("功能类型"), devport_function_combo_);

  stack_->addWidget(w);
}

void PropertyPanelWidget::buildMonitorPage() {
  auto* w = new QWidget(this);
  w->setObjectName("monitorPage");
  auto* lay = new QVBoxLayout(w);

  auto* form = new QFormLayout();
  monitor_name_edit_ = new QLineEdit(w);
  connect(monitor_name_edit_, &QLineEdit::editingFinished, this, [this]() {
    auto* mon = doc_->monitor(editing_monitor_index_);
    if (mon) {
      QString oldName = mon->name;
      QString newName = monitor_name_edit_->text();
      int idx = editing_monitor_index_;
      auto* cmd = new PropertyCommand(
          doc_,
          [doc = doc_, idx, oldName]() {
            if (auto* m = doc->monitor(idx))
              m->name = oldName;
          },
          [doc = doc_, idx, newName]() {
            if (auto* m = doc->monitor(idx))
              m->name = newName;
          },
          QStringLiteral("修改监听器名称"));
      doc_->undoStack()->push(cmd);
    }
  });
  form->addRow(QStringLiteral("名称"), monitor_name_edit_);

  monitor_type_label_ = new QLabel(w);
  form->addRow(QStringLiteral("设备类型"), monitor_type_label_);
  lay->addLayout(form);

  auto* tapsGroup = new QGroupBox(QStringLiteral("已挂载连线"), w);
  auto* tapsLay = new QVBoxLayout(tapsGroup);

  monitor_taps_table_ = new QTableWidget(0, 2, w);
  monitor_taps_table_->setHorizontalHeaderLabels(
      {QStringLiteral("源"), QStringLiteral("目标")});
  monitor_taps_table_->horizontalHeader()->setStretchLastSection(true);
  monitor_taps_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  monitor_taps_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  tapsLay->addWidget(monitor_taps_table_);

  lay->addWidget(tapsGroup);
  stack_->addWidget(w);
}

// ── Slots ──────────────────────────────────────────────────────

void PropertyPanelWidget::onUutNameChanged() {
  auto* prod = doc_->product(editing_uut_index_);
  if (prod) {
    QString oldName = prod->name;
    QString newName = uut_name_edit_->text();
    int idx = editing_uut_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, idx, oldName]() {
          if (auto* p = doc->product(idx))
            p->name = oldName;
        },
        [doc = doc_, idx, newName]() {
          if (auto* p = doc->product(idx))
            p->name = newName;
        },
        QStringLiteral("修改 UUT 名称"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onPortNameChanged() {
  auto* prod = doc_->product(editing_port_product_);
  if (prod && editing_port_index_ < prod->ports.size()) {
    QString oldName = prod->ports[editing_port_index_].name;
    QString newName = port_name_edit_->text();
    int pIdx = editing_port_product_, poIdx = editing_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, pIdx, poIdx, oldName]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].name = oldName;
        },
        [doc = doc_, pIdx, poIdx, newName]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].name = newName;
        },
        QStringLiteral("修改端口名称"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onPortDirectionChanged() {
  auto* prod = doc_->product(editing_port_product_);
  if (prod && editing_port_index_ < prod->ports.size()) {
    auto oldDir = prod->ports[editing_port_index_].direction;
    auto newDir = static_cast<TopologyPort::Direction>(
        port_direction_combo_->currentIndex());
    int pIdx = editing_port_product_, poIdx = editing_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, pIdx, poIdx, oldDir]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].direction = oldDir;
        },
        [doc = doc_, pIdx, poIdx, newDir]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].direction = newDir;
        },
        QStringLiteral("修改端口方向"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onPortAllowedTypesChanged() {
  auto* prod = doc_->product(editing_port_product_);
  if (prod && editing_port_index_ < prod->ports.size()) {
    auto oldTypes = prod->ports[editing_port_index_].allowedDeviceTypes;
    QString text = port_allowed_types_edit_->currentText().trimmed();
    auto newTypes = text.isEmpty() ? QStringList()
                                   : text.split(QStringLiteral(", "),
                                                QString::SkipEmptyParts);
    int pIdx = editing_port_product_, poIdx = editing_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, pIdx, poIdx, oldTypes]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].allowedDeviceTypes = oldTypes;
        },
        [doc = doc_, pIdx, poIdx, newTypes]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].allowedDeviceTypes = newTypes;
        },
        QStringLiteral("修改端口允许设备类型"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onPortFunctionTypeChanged() {
  auto* prod = doc_->product(editing_port_product_);
  if (prod && editing_port_index_ < prod->ports.size()) {
    auto oldFt = prod->ports[editing_port_index_].functionType;
    auto newFt =
        static_cast<FunctionType>(port_function_combo_->currentIndex());
    int pIdx = editing_port_product_, poIdx = editing_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, pIdx, poIdx, oldFt]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].functionType = oldFt;
        },
        [doc = doc_, pIdx, poIdx, newFt]() {
          if (auto* p = doc->product(pIdx))
            if (poIdx < p->ports.size())
              p->ports[poIdx].functionType = newFt;
        },
        QStringLiteral("修改端口功能类型"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onDeviceNameChanged() {
  auto* dev = doc_->device(editing_device_index_);
  if (dev) {
    QString oldName = dev->name;
    QString newName = device_name_edit_->text();
    int idx = editing_device_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, idx, oldName]() {
          if (auto* d = doc->device(idx))
            d->name = oldName;
        },
        [doc = doc_, idx, newName]() {
          if (auto* d = doc->device(idx))
            d->name = newName;
        },
        QStringLiteral("修改设备名称"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onDeviceTypeChanged() {
  auto* dev = doc_->device(editing_device_index_);
  if (dev) {
    dev->deviceType = device_type_edit_->text();
  }
}

void PropertyPanelWidget::onAddPropertyRow() {
  int row = device_props_table_->rowCount();
  device_props_table_->insertRow(row);
  device_props_table_->setItem(row, 0, new QTableWidgetItem(QString()));
  device_props_table_->setItem(row, 1, new QTableWidgetItem(QString()));
  device_dirty_ = true;
}

void PropertyPanelWidget::onRemovePropertyRow() {
  int row = device_props_table_->currentRow();
  if (row >= 0) {
    device_props_table_->removeRow(row);
    device_dirty_ = true;
  }
}

void PropertyPanelWidget::applyDeviceProperties(int deviceIndex) {
  auto* dev = doc_->device(deviceIndex);
  if (!dev || !device_dirty_)
    return;

  // Build new properties from table
  QVector<QPair<QString, QString>> newProps;
  for (int r = 0; r < device_props_table_->rowCount(); ++r) {
    auto* keyItem = device_props_table_->item(r, 0);
    auto* valItem = device_props_table_->item(r, 1);
    if (keyItem && valItem && !keyItem->text().isEmpty()) {
      newProps.append({keyItem->text(), valItem->text()});
    }
  }

  if (newProps != saved_device_properties_) {
    auto oldProps = saved_device_properties_;
    int idx = deviceIndex;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, idx, oldProps]() {
          if (auto* d = doc->device(idx))
            d->properties = oldProps;
        },
        [doc = doc_, idx, newProps]() {
          if (auto* d = doc->device(idx))
            d->properties = newProps;
        },
        QStringLiteral("修改设备属性"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::applyDevicePorts(int deviceIndex) {
  auto* dev = doc_->device(deviceIndex);
  if (!dev || !device_dirty_)
    return;

  // Build new ports from model
  QVector<TopologyDevicePort> newPorts;
  for (int r = 0; r < device_port_model_->rowCount(); ++r) {
    auto name =
        device_port_model_->index(r, 0).data(Qt::DisplayRole).toString();
    if (name.isEmpty())
      continue;
    auto dirText =
        device_port_model_->index(r, 1).data(Qt::DisplayRole).toString();
    auto funcText =
        device_port_model_->index(r, 2).data(Qt::DisplayRole).toString();
    TopologyDevicePort dp;
    dp.name = name;
    dp.direction = dirText == QStringLiteral("Bidirectional")
                       ? TopologyPort::Bidirectional
                   : dirText == QStringLiteral("Output") ? TopologyPort::Output
                                                         : TopologyPort::Input;
    dp.functionType = FunctionType::CUSTOM;
    for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft) {
      if (functionTypeToString(static_cast<FunctionType>(ft)) == funcText) {
        dp.functionType = static_cast<FunctionType>(ft);
        break;
      }
    }
    newPorts.append(dp);
  }

  // Compare with saved ports (no operator== on TopologyDevicePort)
  bool equal = newPorts.size() == saved_device_ports_.size();
  if (equal) {
    for (int i = 0; i < newPorts.size(); ++i) {
      if (newPorts[i].name != saved_device_ports_[i].name ||
          newPorts[i].direction != saved_device_ports_[i].direction ||
          newPorts[i].functionType != saved_device_ports_[i].functionType) {
        equal = false;
        break;
      }
    }
  }
  if (equal)
    return;

  auto oldPorts = saved_device_ports_;
  int idx = deviceIndex;
  auto* cmd = new PropertyCommand(
      doc_,
      [doc = doc_, idx, oldPorts]() {
        if (auto* d = doc->device(idx))
          d->ports = oldPorts;
      },
      [doc = doc_, idx, newPorts]() {
        if (auto* d = doc->device(idx))
          d->ports = newPorts;
      },
      QStringLiteral("修改设备端口"));
  doc_->undoStack()->push(cmd);
  device_dirty_ = false;
}

// ── Device port slots ─────────────────────────────────────────

void PropertyPanelWidget::onAddDevicePortRow() {
  int row = device_port_model_->rowCount();
  device_port_model_->insertRow(row);
  device_port_model_->setData(device_port_model_->index(row, 0), QString());
  device_port_model_->setData(device_port_model_->index(row, 1),
                              QStringLiteral("Input"));
  device_port_model_->setData(device_port_model_->index(row, 2),
                              functionTypeToString(FunctionType::A429));
  device_dirty_ = true;
}

void PropertyPanelWidget::onRemoveDevicePortRow() {
  int row = device_port_view_->currentIndex().row();
  if (row >= 0) {
    device_port_model_->removeRow(row);
    device_dirty_ = true;
  }
}

void PropertyPanelWidget::onDevicePortFunctionTypeChanged(int /*row*/) {
  device_dirty_ = true;
}

void PropertyPanelWidget::onDevicePortDirectionChanged(int /*row*/) {
  device_dirty_ = true;
}

void PropertyPanelWidget::onDevicePortNameChanged() {
  auto* dev = doc_->device(editing_device_port_device_);
  if (dev && editing_device_port_index_ < dev->ports.size()) {
    QString oldName = dev->ports[editing_device_port_index_].name;
    QString newName = devport_name_edit_->text();
    int dIdx = editing_device_port_device_, pIdx = editing_device_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, dIdx, pIdx, oldName]() {
          if (auto* d = doc->device(dIdx))
            if (pIdx < d->ports.size())
              d->ports[pIdx].name = oldName;
        },
        [doc = doc_, dIdx, pIdx, newName]() {
          if (auto* d = doc->device(dIdx))
            if (pIdx < d->ports.size())
              d->ports[pIdx].name = newName;
        },
        QStringLiteral("修改设备端口名称"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onDevicePortFunctionTypeChanged() {
  auto* dev = doc_->device(editing_device_port_device_);
  if (dev && editing_device_port_index_ < dev->ports.size()) {
    auto oldFt = dev->ports[editing_device_port_index_].functionType;
    auto newFt =
        static_cast<FunctionType>(devport_function_combo_->currentIndex());
    int dIdx = editing_device_port_device_, pIdx = editing_device_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, dIdx, pIdx, oldFt]() {
          if (auto* d = doc->device(dIdx))
            if (pIdx < d->ports.size())
              d->ports[pIdx].functionType = oldFt;
        },
        [doc = doc_, dIdx, pIdx, newFt]() {
          if (auto* d = doc->device(dIdx))
            if (pIdx < d->ports.size())
              d->ports[pIdx].functionType = newFt;
        },
        QStringLiteral("修改设备端口功能类型"));
    doc_->undoStack()->push(cmd);
  }
}

void PropertyPanelWidget::onDevicePortDirectionChanged() {
  auto* dev = doc_->device(editing_device_port_device_);
  if (dev && editing_device_port_index_ < dev->ports.size()) {
    auto oldDir = dev->ports[editing_device_port_index_].direction;
    auto newDir = static_cast<TopologyPort::Direction>(
        devport_direction_combo_->currentIndex());
    int dIdx = editing_device_port_device_, pIdx = editing_device_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, dIdx, pIdx, oldDir]() {
          if (auto* d = doc->device(dIdx))
            if (pIdx < d->ports.size())
              d->ports[pIdx].direction = oldDir;
        },
        [doc = doc_, dIdx, pIdx, newDir]() {
          if (auto* d = doc->device(dIdx))
            if (pIdx < d->ports.size())
              d->ports[pIdx].direction = newDir;
        },
        QStringLiteral("修改设备端口方向"));
    doc_->undoStack()->push(cmd);
  }
}

}  // namespace etest::topology
