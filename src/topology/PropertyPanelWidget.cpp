#include "PropertyPanelWidget.h"
#include "ComboBoxDelegate.h"
#include "DevicePortBindingDialog.h"
#include "TopologyDocument.h"
#include "UndoCommands.h"
#include "topology_items.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "logger/Logger.h"

namespace etest::topology {

PropertyPanelWidget::PropertyPanelWidget(TopologyDocument* doc, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  setObjectName(QStringLiteral("topologyPropertyPanel"));
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  stack_ = new QStackedWidget(this);
  stack_->setObjectName(QStringLiteral("topologyPropertyStack"));
  layout->addWidget(stack_);

  buildEmptyPage();
  buildUutPage();
  buildPortPage();
  buildDevicePage();
  buildConnectionPage();
  buildDevicePortPage();

  stack_->setCurrentIndex(PageEmpty);

  // Ctrl+Z/Y 同步刷新已绑定帧列表
  if (auto* cmdStack = doc_->undoStack()) {
    connect(cmdStack, &QUndoStack::indexChanged, this,
            &PropertyPanelWidget::refreshDevicePortFrames);
  }
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
    }
  }

  // Save pending UUT port edits when leaving UUT page
  if (editing_uut_index_ >= 0 && uut_dirty_) {
    applyUutPorts(editing_uut_index_);
    uut_dirty_ = false;
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
      // 基本信息
      uut_name_edit_->blockSignals(true);
      uut_name_edit_->setText(prod->name);
      uut_name_edit_->blockSignals(false);

      // 端口列表 — 保存当前状态供 undo，加载到表格
      uut_dirty_ = false;
      saved_uut_ports_ = prod->ports;
      const QStringList dirNames = {QStringLiteral("Input"),
                                    QStringLiteral("Output"),
                                    QStringLiteral("Bidirectional")};
      uut_port_table_->setUpdatesEnabled(false);
      uut_port_table_->blockSignals(true);
      uut_port_table_->setRowCount(prod->ports.size());
      for (int r = 0; r < prod->ports.size(); ++r) {
        const auto& p = prod->ports[r];
        uut_port_table_->setItem(r, 0, new QTableWidgetItem(p.name));
        uut_port_table_->setItem(
            r, 1,
            new QTableWidgetItem(dirNames[static_cast<int>(p.direction)]));
        uut_port_table_->setItem(
            r, 2, new QTableWidgetItem(functionTypeToString(p.functionType)));
      }
      uut_port_table_->blockSignals(false);
      uut_port_table_->resizeColumnsToContents();
      uut_port_table_->setUpdatesEnabled(true);
    }
    stack_->setCurrentIndex(PageUut);
    return;
  }

  if (auto* port = qgraphicsitem_cast<UutPortItem*>(item)) {
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
      port_style_combo_->blockSignals(true);
      port_style_combo_->setCurrentIndex(
          port_style_combo_->findData(static_cast<int>(p.portStyle)));
      port_style_combo_->blockSignals(false);
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
      devport_style_combo_->blockSignals(true);
      devport_style_combo_->setCurrentIndex(
          devport_style_combo_->findData(static_cast<int>(dp.portStyle)));
      devport_style_combo_->blockSignals(false);
    }
    refreshDevicePortFrames();
    stack_->setCurrentIndex(PageDevicePort);
    return;
  }

  if (auto* conn = qgraphicsitem_cast<ConnectionItem*>(item)) {
    editing_conn_index_ = conn->connectionIndex();
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
    conn_style_combo_->blockSignals(true);
    conn_style_combo_->setCurrentIndex(
        conn_style_combo_->findData(static_cast<int>(conn->style())));
    conn_style_combo_->blockSignals(false);

    // ── Monitor section ──
    const auto* c = doc_->connection(editing_conn_index_);
    int foundMonIdx = -1;
    if (c) {
      for (int mi = 0; mi < doc_->monitorCount(); ++mi) {
        const auto* m = doc_->monitor(mi);
        if (m && m->connectionId == c->id) {
          foundMonIdx = mi;
          break;
        }
      }
    }
    if (foundMonIdx >= 0) {
      editing_monitor_index_ = foundMonIdx;
      const auto* m = doc_->monitor(foundMonIdx);
      monitor_name_edit_->blockSignals(true);
      monitor_name_edit_->setText(m->name);
      monitor_name_edit_->blockSignals(false);

      QString connInfo = QStringLiteral("%1.%2 → %3.%4")
                             .arg(c->productName, c->portName,
                                  c->deviceName, c->devicePort);
      monitor_connection_label_->setText(connInfo);

      monitor_display_mode_combo_->blockSignals(true);
      int modeIdx = monitor_display_mode_combo_->findData(m->displayMode);
      if (modeIdx >= 0)
        monitor_display_mode_combo_->setCurrentIndex(modeIdx);
      monitor_display_mode_combo_->blockSignals(false);

      monitor_section_->setVisible(true);
    } else {
      editing_monitor_index_ = -1;
      monitor_section_->setVisible(false);
    }

    stack_->setCurrentIndex(PageConnection);
    return;
  }

  stack_->setCurrentIndex(PageEmpty);
}

void PropertyPanelWidget::clearPanel() {
  // Reset all editing indices to avoid stale-index risk when signals fire
  editing_uut_index_ = -1;
  editing_port_product_ = -1;
  editing_port_index_ = -1;
  editing_device_index_ = -1;
  editing_device_port_device_ = -1;
  editing_device_port_index_ = -1;
  editing_monitor_index_ = -1;
  stack_->setCurrentIndex(PageEmpty);
}

// ── Page builders ──────────────────────────────────────────────

void PropertyPanelWidget::buildEmptyPage() {
  auto* w = new QWidget(this);
  w->setObjectName("emptyPage");
  w->setAutoFillBackground(true);
  auto* lay = new QVBoxLayout(w);
  auto* lbl = new QLabel(QStringLiteral("未选中任何元素"), w);
  lbl->setAlignment(Qt::AlignCenter);
  lay->addWidget(lbl);
  stack_->addWidget(w);
}

void PropertyPanelWidget::buildUutPage() {
  auto* w = new QWidget(this);
  w->setObjectName("uutPage");
  w->setAutoFillBackground(true);
  auto* scrollLay = new QVBoxLayout(w);
  scrollLay->setContentsMargins(0, 0, 0, 0);

  // ── 基本信息 ──
  auto* infoGroup = new QGroupBox(QStringLiteral("基本信息"), w);
  auto* infoLay = new QFormLayout(infoGroup);
  uut_name_edit_ = new QLineEdit(w);
  connect(uut_name_edit_, &QLineEdit::editingFinished, this,
          &PropertyPanelWidget::onUutNameChanged);
  infoLay->addRow(QStringLiteral("名称"), uut_name_edit_);
  scrollLay->addWidget(infoGroup);

  // ── 端口列表 ──
  auto* portGroup = new QGroupBox(QStringLiteral("端口列表"), w);
  auto* portLay = new QVBoxLayout(portGroup);

  uut_port_table_ = new QTableWidget(0, 3, w);
  uut_port_table_->setHorizontalHeaderLabels({QStringLiteral("名称"),
                                              QStringLiteral("方向"),
                                              QStringLiteral("功能类型")});
  uut_port_table_->horizontalHeader()->setStretchLastSection(true);
  uut_port_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  uut_port_table_->setAlternatingRowColors(true);
  // 方向列和功能类型列使用 ComboBoxDelegate
  QStringList dirItems = {QStringLiteral("Input"), QStringLiteral("Output"),
                          QStringLiteral("Bidirectional")};
  QStringList funcItems;
  for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft)
    funcItems << functionTypeToString(static_cast<FunctionType>(ft));
  uut_port_table_->setItemDelegateForColumn(
      1, new ComboBoxDelegate(dirItems, this));
  uut_port_table_->setItemDelegateForColumn(
      2, new ComboBoxDelegate(funcItems, this));
  connect(uut_port_table_, &QTableWidget::itemChanged, this,
          [this]() { uut_dirty_ = true; });
  portLay->addWidget(uut_port_table_);

  auto* btnLay = new QHBoxLayout();
  uut_add_port_btn_ = new QPushButton(QStringLiteral("+"), w);
  connect(uut_add_port_btn_, &QPushButton::clicked, this,
          &PropertyPanelWidget::onUutAddPort);
  uut_remove_port_btn_ = new QPushButton(QStringLiteral("-"), w);
  connect(uut_remove_port_btn_, &QPushButton::clicked, this,
          &PropertyPanelWidget::onUutRemovePort);
  btnLay->addWidget(uut_add_port_btn_);
  btnLay->addWidget(uut_remove_port_btn_);
  btnLay->addStretch();
  portLay->addLayout(btnLay);

  scrollLay->addWidget(portGroup);
  scrollLay->addStretch();

  stack_->addWidget(w);
}

void PropertyPanelWidget::buildPortPage() {
  auto* w = new QWidget(this);
  w->setObjectName("portPage");
  w->setAutoFillBackground(true);
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

  port_style_combo_ = new QComboBox(w);
  port_style_combo_->addItem(QStringLiteral("圆形"),
                             static_cast<int>(PortStyle::Circle));
  port_style_combo_->addItem(QStringLiteral("三角形"),
                             static_cast<int>(PortStyle::Triangle));
  connect(port_style_combo_, &QComboBox::currentTextChanged, this,
          &PropertyPanelWidget::onPortStyleChanged);
  lay->addRow(QStringLiteral("端口样式"), port_style_combo_);

  stack_->addWidget(w);
}

void PropertyPanelWidget::buildDevicePage() {
  auto* w = new QWidget(this);
  w->setObjectName("devicePage");
  w->setAutoFillBackground(true);
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
  w->setAutoFillBackground(true);
  auto* lay = new QFormLayout(w);

  conn_source_label_ = new QLabel(QStringLiteral("-"), w);
  lay->addRow(QStringLiteral("源"), conn_source_label_);

  conn_target_label_ = new QLabel(QStringLiteral("-"), w);
  lay->addRow(QStringLiteral("目标"), conn_target_label_);

  conn_device_port_label_ = new QLabel(QStringLiteral("-"), w);
  lay->addRow(QStringLiteral("设备端口"), conn_device_port_label_);

  conn_style_combo_ = new QComboBox(w);
  conn_style_combo_->addItem(QStringLiteral("曲线"),
                             static_cast<int>(PathStyle::Bezier));
  conn_style_combo_->addItem(QStringLiteral("折线"),
                             static_cast<int>(PathStyle::Polyline));
  conn_style_combo_->addItem(QStringLiteral("直线"),
                             static_cast<int>(PathStyle::Straight));
  connect(conn_style_combo_, &QComboBox::currentTextChanged, this,
          &PropertyPanelWidget::onConnStyleChanged);
  lay->addRow(QStringLiteral("连线样式"), conn_style_combo_);

  // ── Monitor section (hidden when no monitor) ──
  monitor_section_ = new QWidget(w);
  monitor_section_->setVisible(false);
  auto* monLay = new QVBoxLayout(monitor_section_);
  monLay->setContentsMargins(0, 8, 0, 0);

  auto* sep = new QFrame(monitor_section_);
  sep->setFrameShape(QFrame::HLine);
  sep->setFrameShadow(QFrame::Sunken);
  monLay->addWidget(sep);

  auto* monForm = new QFormLayout();

  monitor_name_edit_ = new QLineEdit(monitor_section_);
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
  monForm->addRow(QStringLiteral("监听器名称"), monitor_name_edit_);

  // Connection info (read-only, resolved from connectionId)
  monitor_connection_label_ = new QLabel(QStringLiteral("-"), monitor_section_);
  monitor_connection_label_->setWordWrap(true);
  monForm->addRow(QStringLiteral("连线"), monitor_connection_label_);

  // Display mode at monitor level
  monitor_display_mode_combo_ = new QComboBox(monitor_section_);
  monitor_display_mode_combo_->addItem(QStringLiteral("波形"),
                                       QStringLiteral("waveform"));
  monitor_display_mode_combo_->addItem(QStringLiteral("LED"),
                                       QStringLiteral("led"));
  monitor_display_mode_combo_->addItem(QStringLiteral("仪表"),
                                       QStringLiteral("meter"));
  monitor_display_mode_combo_->addItem(QStringLiteral("帧数据"),
                                       QStringLiteral("frame"));
  connect(monitor_display_mode_combo_, &QComboBox::currentTextChanged,
          this, &PropertyPanelWidget::onMonitorDisplayModeChanged);
  monForm->addRow(QStringLiteral("显示模式"), monitor_display_mode_combo_);

  monLay->addLayout(monForm);

  monitor_delete_btn_ = new QPushButton(QStringLiteral("删除监听器"), monitor_section_);
  monitor_delete_btn_->setObjectName(QStringLiteral("monitorDeleteBtn"));
  connect(monitor_delete_btn_, &QPushButton::clicked,
          this, &PropertyPanelWidget::onMonitorDelete);
  monLay->addWidget(monitor_delete_btn_);

  lay->addRow(monitor_section_);

  stack_->addWidget(w);
}

void PropertyPanelWidget::onConnStyleChanged() {
  if (editing_conn_index_ < 0)
    return;
  auto style = static_cast<PathStyle>(conn_style_combo_->currentData().toInt());
  doc_->undoStack()->push(
      new SetConnectionStyleCommand(doc_, editing_conn_index_, style));
}

void PropertyPanelWidget::onPortStyleChanged() {
  if (editing_port_product_ < 0 || editing_port_index_ < 0)
    return;
  auto style = static_cast<PortStyle>(port_style_combo_->currentData().toInt());
  doc_->undoStack()->push(new SetProductPortStyleCommand(
      doc_, editing_port_product_, editing_port_index_, style));
}

void PropertyPanelWidget::onDevicePortStyleChanged() {
  if (editing_device_port_device_ < 0 || editing_device_port_index_ < 0)
    return;
  auto style =
      static_cast<PortStyle>(devport_style_combo_->currentData().toInt());
  doc_->undoStack()->push(new SetDevicePortStyleCommand(
      doc_, editing_device_port_device_, editing_device_port_index_, style));
}

// ── M3: 打开 ICD 帧绑定对话框 ──

void PropertyPanelWidget::onDevicePortBindFrames() {
  LOG_INFO("TOPOLOGY_UI", "绑定 ICD 帧");
  if (editing_device_port_device_ < 0 || editing_device_port_index_ < 0)
    return;

  DevicePortBindingDialog dlg(doc_, editing_device_port_device_,
                              editing_device_port_index_, available_icd_frames_,
                              this);
  if (dlg.exec() != QDialog::Accepted)
    return;

  QStringList newFrames = dlg.selectedFrames();
  doc_->undoStack()->push(
      new SetDevicePortFramesCommand(doc_, editing_device_port_device_,
                                     editing_device_port_index_, newFrames));
}

void PropertyPanelWidget::refreshDevicePortFrames() {
  if (!devport_frames_list_ || !devport_frames_stack_)
    return;
  if (editing_device_port_device_ < 0 || editing_device_port_index_ < 0) {
    devport_frames_stack_->setCurrentIndex(0);
    return;
  }
  const auto& ports = doc_->device(editing_device_port_device_)->ports;
  if (editing_device_port_index_ >= ports.size()) {
    devport_frames_stack_->setCurrentIndex(0);
    return;
  }
  const QStringList& frames = ports[editing_device_port_index_].boundFrameNames;
  devport_frames_list_->clear();
  if (frames.isEmpty()) {
    devport_frames_stack_->setCurrentIndex(0);
  } else {
    devport_frames_list_->addItems(frames);
    devport_frames_stack_->setCurrentIndex(1);
  }
}

void PropertyPanelWidget::buildDevicePortPage() {
  auto* w = new QWidget(this);
  w->setObjectName("devicePortPage");
  w->setAutoFillBackground(true);
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

  devport_style_combo_ = new QComboBox(w);
  devport_style_combo_->addItem(QStringLiteral("圆形"),
                                static_cast<int>(PortStyle::Circle));
  devport_style_combo_->addItem(QStringLiteral("三角形"),
                                static_cast<int>(PortStyle::Triangle));
  connect(devport_style_combo_, &QComboBox::currentTextChanged, this,
          &PropertyPanelWidget::onDevicePortStyleChanged);
  lay->addRow(QStringLiteral("端口样式"), devport_style_combo_);

  // ── M3: ICD 帧绑定按钮 ──
  devport_bind_frames_btn_ =
      new QPushButton(QStringLiteral("绑定 ICD 帧..."), w);
  lay->addRow(QString(), devport_bind_frames_btn_);
  connect(devport_bind_frames_btn_, &QPushButton::clicked, this,
          &PropertyPanelWidget::onDevicePortBindFrames);

  // ── 已绑定 ICD 帧列表 ──
  auto* framesGroup = new QGroupBox(QStringLiteral("已绑定 ICD 帧"), w);
  auto* framesLay = new QVBoxLayout(framesGroup);
  framesLay->setContentsMargins(4, 4, 4, 4);

  devport_frames_stack_ = new QStackedWidget(w);
  auto* emptyLabel = new QLabel(QStringLiteral("暂无绑定"), w);
  emptyLabel->setObjectName(QStringLiteral("devportEmptyLabel"));
  emptyLabel->setAlignment(Qt::AlignCenter);
  QPalette pal = emptyLabel->palette();
  pal.setColor(QPalette::WindowText, QColor(128, 128, 128));
  emptyLabel->setPalette(pal);
  devport_frames_stack_->addWidget(emptyLabel);  // index 0

  devport_frames_list_ = new QListWidget(w);
  devport_frames_list_->setFrameShape(QFrame::NoFrame);
  devport_frames_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  // devport_frames_list_->setAlternatingRowColors(true);
  devport_frames_list_->setMinimumHeight(60);
  devport_frames_stack_->addWidget(devport_frames_list_);  // index 1

  devport_frames_stack_->setCurrentIndex(0);
  framesLay->addWidget(devport_frames_stack_);
  lay->addRow(framesGroup);

  stack_->addWidget(w);
}


void PropertyPanelWidget::onMonitorDisplayModeChanged() {
  if (editing_monitor_index_ < 0)
    return;
  auto* mon = doc_->monitor(editing_monitor_index_);
  if (!mon)
    return;
  QString oldMode = mon->displayMode;
  QString newMode = monitor_display_mode_combo_->currentData().toString();
  if (oldMode == newMode)
    return;
  int idx = editing_monitor_index_;
  auto* cmd = new PropertyCommand(
      doc_,
      [doc = doc_, idx, oldMode]() {
        if (auto* m = doc->monitor(idx))
          m->displayMode = oldMode;
      },
      [doc = doc_, idx, newMode]() {
        if (auto* m = doc->monitor(idx))
          m->displayMode = newMode;
      },
      QStringLiteral("修改显示模式"));
  doc_->undoStack()->push(cmd);
}

void PropertyPanelWidget::onMonitorDelete() {
  if (editing_monitor_index_ < 0)
    return;
  int idx = editing_monitor_index_;
  doc_->undoStack()->push(new RemoveMonitorCommand(doc_, idx));
  editing_monitor_index_ = -1;
  monitor_section_->setVisible(false);
  emit documentChanged();
}

// ── Slots ──────────────────────────────────────────────────────

void PropertyPanelWidget::onUutNameChanged() {
  auto* prod = doc_->product(editing_uut_index_);
  if (prod) {
    QString oldName = prod->name;
    QString newName = uut_name_edit_->text();
    if (oldName == newName)
      return;
    int idx = editing_uut_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, idx, oldName]() { doc->renameProduct(idx, oldName); },
        [doc = doc_, idx, newName]() { doc->renameProduct(idx, newName); },
        QStringLiteral("修改 UUT 名称"));
    doc_->undoStack()->push(cmd);
  }
}

// ── UUT port table operations ──

void PropertyPanelWidget::onUutAddPort() {
  LOG_INFO("TOPOLOGY_UI", "添加 UUT 端口");
  auto* prod = doc_->product(editing_uut_index_);
  if (!prod)
    return;
  int n = prod->ports.size() + 1;
  TopologyPort port;
  port.name = QStringLiteral("Port_%1").arg(n, 2, 10, QChar('0'));
  port.direction = TopologyPort::Direction::Input;
  port.functionType = FunctionType::CUSTOM;
  auto* cmd = new AddProductPortCommand(doc_, editing_uut_index_, port);
  doc_->undoStack()->push(cmd);
  // 场景刷新后，重新加载表格
  prod = doc_->product(editing_uut_index_);
  if (prod) {
    const QStringList dirNames = {QStringLiteral("Input"),
                                  QStringLiteral("Output"),
                                  QStringLiteral("Bidirectional")};
    uut_port_table_->blockSignals(true);
    uut_port_table_->setRowCount(prod->ports.size());
    int r = prod->ports.size() - 1;
    const auto& p = prod->ports[r];
    uut_port_table_->setItem(r, 0, new QTableWidgetItem(p.name));
    uut_port_table_->setItem(
        r, 1, new QTableWidgetItem(dirNames[static_cast<int>(p.direction)]));
    uut_port_table_->setItem(
        r, 2, new QTableWidgetItem(functionTypeToString(p.functionType)));
    uut_port_table_->blockSignals(false);
    uut_port_table_->resizeColumnsToContents();
  }
}

void PropertyPanelWidget::onUutRemovePort() {
  LOG_INFO("TOPOLOGY_UI", "移除 UUT 端口");
  int row = uut_port_table_->currentRow();
  if (row < 0 || editing_uut_index_ < 0)
    return;
  auto* prod = doc_->product(editing_uut_index_);
  if (!prod || row >= prod->ports.size())
    return;
  auto* cmd = new RemoveProductPortCommand(doc_, editing_uut_index_, row);
  doc_->undoStack()->push(cmd);
  // 场景刷新后，重新加载表格
  prod = doc_->product(editing_uut_index_);
  if (prod) {
    const QStringList dirNames = {QStringLiteral("Input"),
                                  QStringLiteral("Output"),
                                  QStringLiteral("Bidirectional")};
    uut_port_table_->blockSignals(true);
    uut_port_table_->setRowCount(prod->ports.size());
    for (int r = 0; r < prod->ports.size(); ++r) {
      const auto& p = prod->ports[r];
      uut_port_table_->setItem(r, 0, new QTableWidgetItem(p.name));
      uut_port_table_->setItem(
          r, 1, new QTableWidgetItem(dirNames[static_cast<int>(p.direction)]));
      uut_port_table_->setItem(
          r, 2, new QTableWidgetItem(functionTypeToString(p.functionType)));
    }
    uut_port_table_->blockSignals(false);
    uut_port_table_->resizeColumnsToContents();
  }
}

void PropertyPanelWidget::applyUutPorts(int productIndex) {
  if (!uut_dirty_)
    return;
  auto* prod = doc_->product(productIndex);
  if (!prod)
    return;

  // Build new ports from table
  QVector<TopologyPort> newPorts;
  for (int r = 0; r < uut_port_table_->rowCount(); ++r) {
    auto* nameItem = uut_port_table_->item(r, 0);
    if (!nameItem || nameItem->text().isEmpty())
      continue;
    auto* dirItem = uut_port_table_->item(r, 1);
    auto* funcItem = uut_port_table_->item(r, 2);
    TopologyPort port;
    if (r < saved_uut_ports_.size())
      port = saved_uut_ports_[r];
    port.name = nameItem->text();
    port.direction =
        dirItem && dirItem->text() == QStringLiteral("Output")
            ? TopologyPort::Direction::Output
        : dirItem && dirItem->text() == QStringLiteral("Bidirectional")
            ? TopologyPort::Direction::Bidirectional
            : TopologyPort::Direction::Input;
    port.functionType = FunctionType::CUSTOM;
    if (funcItem) {
      for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft) {
        if (functionTypeToString(static_cast<FunctionType>(ft)) ==
            funcItem->text()) {
          port.functionType = static_cast<FunctionType>(ft);
          break;
        }
      }
    }
    newPorts.append(port);
  }

  // Compare with saved ports
  bool equal = newPorts.size() == saved_uut_ports_.size();
  if (equal) {
    for (int i = 0; i < newPorts.size(); ++i) {
      if (newPorts[i].name != saved_uut_ports_[i].name ||
          newPorts[i].direction != saved_uut_ports_[i].direction ||
          newPorts[i].functionType != saved_uut_ports_[i].functionType) {
        equal = false;
        break;
      }
    }
  }
  if (equal)
    return;

  auto oldPorts = saved_uut_ports_;
  int idx = productIndex;
  auto* cmd = new PropertyCommand(
      doc_,
      [doc = doc_, idx, oldPorts]() {
        if (auto* p = doc->product(idx))
          p->ports = oldPorts;
      },
      [doc = doc_, idx, newPorts]() {
        if (auto* p = doc->product(idx))
          p->ports = newPorts;
      },
      QStringLiteral("修改 UUT 端口"));
  doc_->undoStack()->push(cmd);
  uut_dirty_ = false;
}

void PropertyPanelWidget::onPortNameChanged() {
  auto* prod = doc_->product(editing_port_product_);
  if (prod && editing_port_index_ < prod->ports.size()) {
    QString oldName = prod->ports[editing_port_index_].name;
    QString newName = port_name_edit_->text();
    if (oldName == newName)
      return;
    int pIdx = editing_port_product_, poIdx = editing_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, pIdx, poIdx, oldName]() {
          doc->renameProductPort(pIdx, poIdx, oldName);
        },
        [doc = doc_, pIdx, poIdx, newName]() {
          doc->renameProductPort(pIdx, poIdx, newName);
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    auto newTypes = text.isEmpty()
                        ? QStringList()
                        : text.split(QStringLiteral(", "), Qt::SkipEmptyParts);
#else
    auto newTypes = text.isEmpty() ? QStringList()
                                   : text.split(QStringLiteral(", "),
                                                QString::SkipEmptyParts);
#endif
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
    if (oldName == newName)
      return;
    int idx = editing_device_index_;
    auto* cmd = new PropertyCommand(
        doc_, [doc = doc_, idx, oldName]() { doc->renameDevice(idx, oldName); },
        [doc = doc_, idx, newName]() { doc->renameDevice(idx, newName); },
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
  LOG_INFO("TOPOLOGY_UI", "添加属性");
  int row = device_props_table_->rowCount();
  device_props_table_->insertRow(row);
  device_props_table_->setItem(row, 0, new QTableWidgetItem(QString()));
  device_props_table_->setItem(row, 1, new QTableWidgetItem(QString()));
  device_dirty_ = true;
}

void PropertyPanelWidget::onRemovePropertyRow() {
  LOG_INFO("TOPOLOGY_UI", "移除属性");
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
    if (r < saved_device_ports_.size())
      dp = saved_device_ports_[r];
    dp.name = name;
    dp.direction = dirText == QStringLiteral("Bidirectional")
                       ? TopologyPort::Direction::Bidirectional
                   : dirText == QStringLiteral("Output")
                       ? TopologyPort::Direction::Output
                       : TopologyPort::Direction::Input;
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
  LOG_INFO("TOPOLOGY_UI", "添加设备端口");
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
  LOG_INFO("TOPOLOGY_UI", "移除设备端口");
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
    if (oldName == newName)
      return;
    int dIdx = editing_device_port_device_, pIdx = editing_device_port_index_;
    auto* cmd = new PropertyCommand(
        doc_,
        [doc = doc_, dIdx, pIdx, oldName]() {
          doc->renameDevicePort(dIdx, pIdx, oldName);
        },
        [doc = doc_, dIdx, pIdx, newName]() {
          doc->renameDevicePort(dIdx, pIdx, newName);
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
