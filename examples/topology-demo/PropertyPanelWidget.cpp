#include "PropertyPanelWidget.h"
#include "TopologyDocument.h"
#include "topology_items.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>

namespace topology {

PropertyPanelWidget::PropertyPanelWidget(TopologyDocument* doc,
                                         QWidget* parent)
    : QWidget(parent), doc_(doc) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    stack_ = new QStackedWidget(this);
    layout->addWidget(stack_);

    buildEmptyPage();
    buildUutPage();
    buildPortPage();
    buildDevicePage();
    buildConnectionPage();
    buildDevicePortPage();

    stack_->setCurrentIndex(PageEmpty);
}

void PropertyPanelWidget::showPropertiesFor(QGraphicsItem* item) {
    // Save pending device edits when leaving device page
    if (editing_device_index_ >= 0) {
        applyDeviceProperties(editing_device_index_);
        applyDevicePorts(editing_device_index_);
    }

    editing_uut_index_ = -1;
    editing_port_product_ = -1;
    editing_port_index_ = -1;
    editing_device_index_ = -1;
    editing_device_port_device_ = -1;
    editing_device_port_index_ = -1;

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
            port_direction_combo_->setCurrentIndex(
                static_cast<int>(p.direction));
            port_direction_combo_->blockSignals(false);
            port_function_combo_->blockSignals(true);
            port_function_combo_->setCurrentIndex(
                static_cast<int>(p.functionType));
            port_function_combo_->blockSignals(false);
        }
        stack_->setCurrentIndex(PagePort);
        return;
    }

    if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
        editing_device_index_ = dev->deviceIndex();
        auto* d = doc_->device(editing_device_index_);
        if (d) {
            device_name_edit_->blockSignals(true);
            device_name_edit_->setText(d->name);
            device_name_edit_->blockSignals(false);
            device_type_edit_->setText(d->deviceType);

            device_props_table_->setRowCount(d->properties.size());
            for (int r = 0; r < d->properties.size(); ++r) {
                device_props_table_->setItem(r, 0,
                    new QTableWidgetItem(d->properties[r].first));
                device_props_table_->setItem(r, 1,
                    new QTableWidgetItem(d->properties[r].second));
            }

            // Load device ports into table
            device_port_table_->setRowCount(d->ports.size());
            for (int r = 0; r < d->ports.size(); ++r) {
                device_port_table_->setItem(r, 0,
                    new QTableWidgetItem(d->ports[r].name));
                auto* dirCombo = new QComboBox();
                dirCombo->addItem(QStringLiteral("Input"));
                dirCombo->addItem(QStringLiteral("Output"));
                dirCombo->addItem(QStringLiteral("Bidirectional"));
                dirCombo->setCurrentIndex(
                    static_cast<int>(d->ports[r].direction));
                connect(dirCombo, &QComboBox::currentTextChanged, this,
                        [this, r](const QString&) { onDevicePortDirectionChanged(r); });
                device_port_table_->setCellWidget(r, 1, dirCombo);
                auto* funcCombo = new QComboBox();
                for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft) {
                    funcCombo->addItem(
                        functionTypeToString(static_cast<FunctionType>(ft)));
                }
                funcCombo->setCurrentIndex(
                    static_cast<int>(d->ports[r].functionType));
                connect(funcCombo, &QComboBox::currentTextChanged, this,
                        [this, r](const QString&) { onDevicePortFunctionTypeChanged(r); });
                device_port_table_->setCellWidget(r, 2, funcCombo);
            }
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
            devport_direction_combo_->setCurrentIndex(
                static_cast<int>(dp.direction));
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
                prod ? QStringLiteral("%1 / %2")
                           .arg(prod->name,
                                prod->ports[src->portIndex()].name)
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

    stack_->setCurrentIndex(PageEmpty);
}

void PropertyPanelWidget::clearPanel() {
    stack_->setCurrentIndex(PageEmpty);
}

// ── Page builders ──────────────────────────────────────────────

void PropertyPanelWidget::buildEmptyPage() {
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    auto* lbl = new QLabel(QStringLiteral("未选中任何元素"), w);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QStringLiteral("color: #888;"));
    lay->addWidget(lbl);
    stack_->addWidget(w);
}

void PropertyPanelWidget::buildUutPage() {
    auto* w = new QWidget(this);
    auto* lay = new QFormLayout(w);

    uut_name_edit_ = new QLineEdit(w);
    connect(uut_name_edit_, &QLineEdit::editingFinished, this,
            &PropertyPanelWidget::onUutNameChanged);
    lay->addRow(QStringLiteral("名称"), uut_name_edit_);

    stack_->addWidget(w);
}

void PropertyPanelWidget::buildPortPage() {
    auto* w = new QWidget(this);
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

    device_port_table_ = new QTableWidget(0, 3, w);
    device_port_table_->setHorizontalHeaderLabels(
        {QStringLiteral("名称"), QStringLiteral("方向"), QStringLiteral("功能类型")});
    device_port_table_->horizontalHeader()->setStretchLastSection(true);
    device_port_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    portLay->addWidget(device_port_table_);

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

// ── Slots ──────────────────────────────────────────────────────

void PropertyPanelWidget::onUutNameChanged() {
    auto* prod = doc_->product(editing_uut_index_);
    if (prod) {
        prod->name = uut_name_edit_->text();
        emit documentChanged();
    }
}

void PropertyPanelWidget::onPortNameChanged() {
    auto* prod = doc_->product(editing_port_product_);
    if (prod && editing_port_index_ < prod->ports.size()) {
        prod->ports[editing_port_index_].name = port_name_edit_->text();
        emit documentChanged();
    }
}

void PropertyPanelWidget::onPortDirectionChanged() {
    auto* prod = doc_->product(editing_port_product_);
    if (prod && editing_port_index_ < prod->ports.size()) {
        prod->ports[editing_port_index_].direction =
            static_cast<TopologyPort::Direction>(
                port_direction_combo_->currentIndex());
        emit documentChanged();
    }
}

void PropertyPanelWidget::onPortAllowedTypesChanged() {
    auto* prod = doc_->product(editing_port_product_);
    if (prod && editing_port_index_ < prod->ports.size()) {
        QString text = port_allowed_types_edit_->currentText().trimmed();
        prod->ports[editing_port_index_].allowedDeviceTypes =
            text.isEmpty() ? QStringList()
                           : text.split(QStringLiteral(", "),
                                        QString::SkipEmptyParts);
        emit documentChanged();
    }
}

void PropertyPanelWidget::onPortFunctionTypeChanged() {
    auto* prod = doc_->product(editing_port_product_);
    if (prod && editing_port_index_ < prod->ports.size()) {
        prod->ports[editing_port_index_].functionType =
            static_cast<FunctionType>(
                port_function_combo_->currentIndex());
        emit documentChanged();
    }
}

void PropertyPanelWidget::onDeviceNameChanged() {
    auto* dev = doc_->device(editing_device_index_);
    if (dev) {
        dev->name = device_name_edit_->text();
        emit documentChanged();
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
}

void PropertyPanelWidget::onRemovePropertyRow() {
    int row = device_props_table_->currentRow();
    if (row >= 0) {
        device_props_table_->removeRow(row);
    }
}

void PropertyPanelWidget::applyDeviceProperties(int deviceIndex) {
    auto* dev = doc_->device(deviceIndex);
    if (!dev) return;

    dev->properties.clear();
    for (int r = 0; r < device_props_table_->rowCount(); ++r) {
        auto* keyItem = device_props_table_->item(r, 0);
        auto* valItem = device_props_table_->item(r, 1);
        if (keyItem && valItem && !keyItem->text().isEmpty()) {
            dev->properties.append({keyItem->text(), valItem->text()});
        }
    }
}

void PropertyPanelWidget::applyDevicePorts(int deviceIndex) {
    auto* dev = doc_->device(deviceIndex);
    if (!dev) return;

    dev->ports.clear();
    for (int r = 0; r < device_port_table_->rowCount(); ++r) {
        auto* nameItem = device_port_table_->item(r, 0);
        if (!nameItem || nameItem->text().isEmpty()) continue;
        auto* dirCombo = qobject_cast<QComboBox*>(
            device_port_table_->cellWidget(r, 1));
        auto* funcCombo = qobject_cast<QComboBox*>(
            device_port_table_->cellWidget(r, 2));
        TopologyDevicePort dp;
        dp.name = nameItem->text();
        dp.direction = static_cast<TopologyPort::Direction>(
            dirCombo ? dirCombo->currentIndex() : 1);
        dp.functionType = static_cast<FunctionType>(
            funcCombo ? funcCombo->currentIndex() : 0);
        dev->ports.append(dp);
    }
}

// ── Device port slots ─────────────────────────────────────────

void PropertyPanelWidget::onAddDevicePortRow() {
    int row = device_port_table_->rowCount();
    device_port_table_->insertRow(row);
    device_port_table_->setItem(row, 0, new QTableWidgetItem(QString()));
    auto* dirCombo = new QComboBox();
    dirCombo->addItem(QStringLiteral("Input"));
    dirCombo->addItem(QStringLiteral("Output"));
    dirCombo->addItem(QStringLiteral("Bidirectional"));
    connect(dirCombo, &QComboBox::currentTextChanged, this,
            [this, row](const QString&) { onDevicePortDirectionChanged(row); });
    device_port_table_->setCellWidget(row, 1, dirCombo);
    auto* funcCombo = new QComboBox();
    for (int ft = 0; ft <= static_cast<int>(FunctionType::CUSTOM); ++ft) {
        funcCombo->addItem(
            functionTypeToString(static_cast<FunctionType>(ft)));
    }
    connect(funcCombo, &QComboBox::currentTextChanged, this,
            [this, row](const QString&) { onDevicePortFunctionTypeChanged(row); });
    device_port_table_->setCellWidget(row, 2, funcCombo);
}

void PropertyPanelWidget::onRemoveDevicePortRow() {
    int row = device_port_table_->currentRow();
    if (row >= 0) {
        device_port_table_->removeRow(row);
    }
}

void PropertyPanelWidget::onDevicePortFunctionTypeChanged(int row) {
    if (editing_device_index_ < 0) return;
    Q_UNUSED(row);
    emit documentChanged();
}

void PropertyPanelWidget::onDevicePortDirectionChanged(int row) {
    if (editing_device_index_ < 0) return;
    Q_UNUSED(row);
    emit documentChanged();
}

void PropertyPanelWidget::onDevicePortNameChanged() {
    auto* dev = doc_->device(editing_device_port_device_);
    if (dev && editing_device_port_index_ < dev->ports.size()) {
        dev->ports[editing_device_port_index_].name = devport_name_edit_->text();
        emit documentChanged();
    }
}

void PropertyPanelWidget::onDevicePortFunctionTypeChanged() {
    auto* dev = doc_->device(editing_device_port_device_);
    if (dev && editing_device_port_index_ < dev->ports.size()) {
        dev->ports[editing_device_port_index_].functionType =
            static_cast<FunctionType>(devport_function_combo_->currentIndex());
        emit documentChanged();
    }
}

void PropertyPanelWidget::onDevicePortDirectionChanged() {
    auto* dev = doc_->device(editing_device_port_device_);
    if (dev && editing_device_port_index_ < dev->ports.size()) {
        dev->ports[editing_device_port_index_].direction =
            static_cast<TopologyPort::Direction>(
                devport_direction_combo_->currentIndex());
        emit documentChanged();
    }
}

}  // namespace topology
