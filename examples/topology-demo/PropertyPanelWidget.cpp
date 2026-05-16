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

    stack_->setCurrentIndex(PageEmpty);
}

void PropertyPanelWidget::showPropertiesFor(QGraphicsItem* item) {
    editing_uut_index_ = -1;
    editing_port_product_ = -1;
    editing_port_index_ = -1;
    editing_device_index_ = -1;

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
            port_name_edit_->blockSignals(true);
            port_name_edit_->setText(p.name);
            port_name_edit_->blockSignals(false);
            port_direction_combo_->blockSignals(true);
            port_direction_combo_->setCurrentIndex(
                p.direction == TopologyPort::Input ? 0 : 1);
            port_direction_combo_->blockSignals(false);
            port_allowed_types_edit_->setText(p.allowedDeviceTypes.join(", "));
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
        }
        stack_->setCurrentIndex(PageDevice);
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
    connect(port_direction_combo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PropertyPanelWidget::onPortDirectionChanged);
    lay->addRow(QStringLiteral("方向"), port_direction_combo_);

    port_allowed_types_edit_ = new QLineEdit(w);
    port_allowed_types_edit_->setReadOnly(true);
    port_allowed_types_edit_->setPlaceholderText(
        QStringLiteral("如: EPH6272T, EPH5272"));
    lay->addRow(QStringLiteral("允许设备类型"), port_allowed_types_edit_);

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

// ── Slots ──────────────────────────────────────────────────────

void PropertyPanelWidget::onUutNameChanged() {
    auto* prod = doc_->product(editing_uut_index_);
    if (prod) {
        prod->name = uut_name_edit_->text();
    }
}

void PropertyPanelWidget::onPortNameChanged() {
    auto* prod = doc_->product(editing_port_product_);
    if (prod && editing_port_index_ < prod->ports.size()) {
        prod->ports[editing_port_index_].name = port_name_edit_->text();
    }
}

void PropertyPanelWidget::onPortDirectionChanged() {
    auto* prod = doc_->product(editing_port_product_);
    if (prod && editing_port_index_ < prod->ports.size()) {
        prod->ports[editing_port_index_].direction =
            (port_direction_combo_->currentIndex() == 0)
                ? TopologyPort::Input
                : TopologyPort::Output;
    }
}

void PropertyPanelWidget::onDeviceNameChanged() {
    auto* dev = doc_->device(editing_device_index_);
    if (dev) {
        dev->name = device_name_edit_->text();
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

}  // namespace topology
