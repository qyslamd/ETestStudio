#include "TopologyEditorWidget.h"
#include "TopologyDocument.h"
#include "TopologyScene.h"
#include "TopologyView.h"
#include "TopologyJsonSerializer.h"
#include "topology_items.h"
#include "DeviceTemplateManager.h"
#include "PropertyPanelWidget.h"

#include <QMenuBar>
#include <QToolBar>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QApplication>
#include <QStatusBar>

namespace topology {

TopologyEditorWidget::TopologyEditorWidget(QWidget* parent)
    : QMainWindow(parent) {
    doc_ = new TopologyDocument(this);
    scene_ = new TopologyScene(doc_, this);
    view_ = new TopologyView(scene_, this);
    property_panel_ = new PropertyPanelWidget(doc_, this);

    initUi();
    initSignals();
    buildDefaultDocument();
}

TopologyEditorWidget::~TopologyEditorWidget() {}

void TopologyEditorWidget::initUi() {
    setWindowTitle(QStringLiteral("拓扑编辑器 - topology-demo"));
    resize(1200, 800);

    // Menu bar
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

    auto* newAct = fileMenu->addAction(QStringLiteral("新建(&N)"));
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &TopologyEditorWidget::onNewFile);

    auto* openAct = fileMenu->addAction(QStringLiteral("打开(&O)..."));
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &TopologyEditorWidget::onOpenFile);

    auto* saveAct = fileMenu->addAction(QStringLiteral("保存(&S)"));
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &TopologyEditorWidget::onSaveFile);

    auto* saveAsAct = fileMenu->addAction(QStringLiteral("另存为(&A)..."));
    saveAsAct->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_S));
    connect(saveAsAct, &QAction::triggered, this,
            &TopologyEditorWidget::onSaveAsFile);

    fileMenu->addSeparator();

    auto* exitAct = fileMenu->addAction(QStringLiteral("退出(&X)"));
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Toolbar
    auto* toolbar = addToolBar(QStringLiteral("编辑"));
    toolbar->setMovable(false);

    add_uut_action_ = toolbar->addAction(QStringLiteral("+ UUT"));
    add_uut_action_->setToolTip(QStringLiteral("添加被测产品"));

    add_device_action_ = toolbar->addAction(QStringLiteral("+ 设备"));
    add_device_action_->setToolTip(QStringLiteral("添加激励设备"));

    toolbar->addSeparator();

    zoom_in_action_ = toolbar->addAction(QStringLiteral("放大"));
    zoom_out_action_ = toolbar->addAction(QStringLiteral("缩小"));
    zoom_reset_action_ = toolbar->addAction(QStringLiteral("重置"));

    // Central area: View + Property panel
    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->addWidget(view_);
    splitter_->addWidget(property_panel_);
    splitter_->setStretchFactor(0, 4);
    splitter_->setStretchFactor(1, 1);

    setCentralWidget(splitter_);

    statusBar()->showMessage(QStringLiteral("就绪"));
}

void TopologyEditorWidget::initSignals() {
    connect(add_uut_action_, &QAction::triggered, this,
            [this]() { onAddUut(); });
    connect(add_device_action_, &QAction::triggered, this,
            [this]() { onAddDevice(); });

    connect(zoom_in_action_, &QAction::triggered, view_,
            &TopologyView::zoomIn);
    connect(zoom_out_action_, &QAction::triggered, view_,
            &TopologyView::zoomOut);
    connect(zoom_reset_action_, &QAction::triggered, view_,
            &TopologyView::zoomReset);

    // Context menu from view
    connect(view_, &TopologyView::addUutRequested, this,
            &TopologyEditorWidget::onAddUut);
    connect(view_, &TopologyView::addDeviceRequested, this,
            &TopologyEditorWidget::onAddDevice);
    connect(view_, &TopologyView::deleteItemRequested, this,
            &TopologyEditorWidget::onDeleteItem);
    connect(view_, &TopologyView::saveTemplateRequested, this,
            &TopologyEditorWidget::onSaveTemplate);

    // Selection change → property panel
    connect(scene_, &TopologyScene::itemSelected, this,
            &TopologyEditorWidget::onSelectionChanged);

    // Document change → refresh scene
    connect(property_panel_, &PropertyPanelWidget::documentChanged, this,
            &TopologyEditorWidget::onDocumentChanged);
}

void TopologyEditorWidget::buildDefaultDocument() {
    // Product 1
    TopologyProduct prod1;
    prod1.name = QStringLiteral("ISI-01");
    prod1.position = QPointF(50, 120);
    prod1.ports.append(
        {QStringLiteral("A429_IN1"), TopologyPort::Input,
         {QStringLiteral("A429")}});
    prod1.ports.append(
        {QStringLiteral("A429_IN2"), TopologyPort::Input,
         {QStringLiteral("A429")}});
    prod1.ports.append(
        {QStringLiteral("A429_OUT"), TopologyPort::Output,
         {QStringLiteral("A429")}});
    prod1.ports.append(
        {QStringLiteral("离散量"), TopologyPort::Input,
         {QStringLiteral("DISCRETE")}});
    doc_->addProduct(prod1);

    // Product 2
    TopologyProduct prod2;
    prod2.name = QStringLiteral("ISI-02");
    prod2.position = QPointF(50, 320);
    prod2.ports.append(
        {QStringLiteral("A429_IN1"), TopologyPort::Input,
         {QStringLiteral("A429")}});
    doc_->addProduct(prod2);

    // Device 1
    TopologyDevice dev1;
    dev1.name = QStringLiteral("6272T_00");
    dev1.deviceType = QStringLiteral("EPH6272T");
    dev1.position = QPointF(450, 80);
    dev1.properties = {{QStringLiteral("bus"), QStringLiteral("PXI6")},
                       {QStringLiteral("slot"), QStringLiteral("2")}};
    dev1.ports.append({QStringLiteral("ch0"), FunctionType::A429});
    dev1.ports.append({QStringLiteral("ch1"), FunctionType::A429});
    doc_->addDevice(dev1);

    // Device 2
    TopologyDevice dev2;
    dev2.name = QStringLiteral("6272T_01");
    dev2.deviceType = QStringLiteral("EPH6272T");
    dev2.position = QPointF(450, 200);
    dev2.properties = {{QStringLiteral("bus"), QStringLiteral("PXI6")},
                       {QStringLiteral("slot"), QStringLiteral("3")}};
    dev2.ports.append({QStringLiteral("ch0"), FunctionType::A429});
    dev2.ports.append({QStringLiteral("ch1"), FunctionType::A429});
    doc_->addDevice(dev2);

    // Device 3
    TopologyDevice dev3;
    dev3.name = QStringLiteral("EPH5121A_00");
    dev3.deviceType = QStringLiteral("EPH5121A");
    dev3.position = QPointF(450, 350);
    dev3.properties = {{QStringLiteral("bus"), QStringLiteral("PXI6")},
                       {QStringLiteral("slot"), QStringLiteral("5")}};
    dev3.ports.append({QStringLiteral("ch0"), FunctionType::DISCRETE});
    doc_->addDevice(dev3);

    scene_->loadFromDocument();
}

// ── Slots ──────────────────────────────────────────────────────

void TopologyEditorWidget::onAddUut(const QPointF& scenePos) {
    int n = doc_->productCount() + 1;
    TopologyProduct prod;
    prod.name = QStringLiteral("UUT-%1").arg(n, 2, 10, QChar('0'));
    prod.position = (scenePos.isNull()) ? QPointF(50, 100 + n * 80) : scenePos;
    prod.ports.append(
        {QStringLiteral("Port_IN1"), TopologyPort::Input,
         {QStringLiteral("A429")}});
    prod.ports.append(
        {QStringLiteral("Port_OUT1"), TopologyPort::Output,
         {QStringLiteral("A429")}});

    int idx = doc_->addProduct(prod);
    scene_->addProductItem(idx, prod.position);
    statusBar()->showMessage(
        QStringLiteral("已添加 UUT: %1").arg(prod.name), 3000);
}

void TopologyEditorWidget::onAddDevice(const QPointF& scenePos) {
    int n = doc_->deviceCount() + 1;
    TopologyDevice dev;
    dev.name = QStringLiteral("Device-%1").arg(n, 2, 10, QChar('0'));
    dev.deviceType = QStringLiteral("EPH6272T");
    dev.position = (scenePos.isNull()) ? QPointF(400, 100 + n * 80) : scenePos;

    int idx = doc_->addDevice(dev);
    scene_->addDeviceItem(idx, dev.position);
    statusBar()->showMessage(
        QStringLiteral("已添加设备: %1").arg(dev.name), 3000);
}

void TopologyEditorWidget::onDeleteItem(QGraphicsItem* item) {
    if (!item) return;

    bool removed = false;

    if (auto* uut = qgraphicsitem_cast<UutItem*>(item)) {
        doc_->removeProduct(uut->productIndex());
        removed = true;
        statusBar()->showMessage(QStringLiteral("已删除 UUT"), 3000);
    } else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
        doc_->removeDevice(dev->deviceIndex());
        removed = true;
        statusBar()->showMessage(QStringLiteral("已删除设备"), 3000);
    } else if (auto* devPort = qgraphicsitem_cast<DevicePortItem*>(item)) {
        doc_->removeDevicePort(devPort->deviceIndex(), devPort->portIndex());
        removed = true;
        statusBar()->showMessage(QStringLiteral("已删除设备端口"), 3000);
    } else if (auto* conn = qgraphicsitem_cast<ConnectionItem*>(item)) {
        auto* src = conn->sourcePort();
        auto* tgt = conn->targetDevice();
        if (src && tgt) {
            auto* prod = doc_->product(src->productIndex());
            auto* dev = doc_->device(tgt->deviceIndex());
            if (prod && dev) {
                for (int i = 0; i < doc_->connectionCount(); ++i) {
                    const auto* c = doc_->connection(i);
                    if (c->productName == prod->name &&
                        c->portName == prod->ports[src->portIndex()].name &&
                        c->deviceName == dev->name) {
                        doc_->removeConnection(i);
                        break;
                    }
                }
            }
        }
        removed = true;
        statusBar()->showMessage(QStringLiteral("已删除连线"), 3000);
    }

    if (removed) {
        scene_->loadFromDocument();
    }
}

void TopologyEditorWidget::onSaveTemplate(QGraphicsItem* item) {
    auto* dev = qgraphicsitem_cast<DeviceItem*>(item);
    if (!dev) return;

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存设备模板"), QString(),
        QStringLiteral("设备模板 (*.dvt)"));
    if (path.isEmpty()) return;

    if (DeviceTemplateManager::saveTemplate(doc_, dev->deviceIndex(), path)) {
        statusBar()->showMessage(QStringLiteral("模板已保存: %1").arg(path),
                                 3000);
    } else {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("保存模板失败"));
    }
}

void TopologyEditorWidget::onNewFile() {
    doc_->clear();
    scene_->loadFromDocument();
    current_file_.clear();
    setWindowTitle(QStringLiteral("拓扑编辑器 - [未命名]"));
    statusBar()->showMessage(QStringLiteral("新建文件"), 3000);
}

void TopologyEditorWidget::onOpenFile() {
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开拓扑文件"), QString(),
        QStringLiteral("拓扑文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法打开文件"));
        return;
    }

    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, QStringLiteral("解析错误"), err.errorString());
        return;
    }

    if (!TopologyJsonSerializer::deserialize(jdoc.object(), doc_)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("数据格式错误"));
        return;
    }

    scene_->loadFromDocument();
    current_file_ = path;
    setWindowTitle(QStringLiteral("拓扑编辑器 - %1").arg(path));
    statusBar()->showMessage(QStringLiteral("已打开: %1").arg(path), 3000);
}

void TopologyEditorWidget::onSaveFile() {
    if (current_file_.isEmpty()) {
        onSaveAsFile();
        return;
    }

    scene_->syncPositionsToDocument();
    QJsonObject json = TopologyJsonSerializer::serialize(*doc_);
    QJsonDocument jdoc(json);

    QFile file(current_file_);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写入文件"));
        return;
    }
    file.write(jdoc.toJson(QJsonDocument::Indented));
    file.close();

    statusBar()->showMessage(
        QStringLiteral("已保存: %1").arg(current_file_), 3000);
}

void TopologyEditorWidget::onSaveAsFile() {
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存拓扑文件"), QString(),
        QStringLiteral("拓扑文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty()) return;

    // Ensure .json extension
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".json");
    }

    current_file_ = path;
    onSaveFile();
    setWindowTitle(QStringLiteral("拓扑编辑器 - %1").arg(path));
}

void TopologyEditorWidget::onSelectionChanged(QGraphicsItem* item) {
    if (item) {
        property_panel_->showPropertiesFor(item);
    } else {
        property_panel_->clearPanel();
    }
}

void TopologyEditorWidget::onDocumentChanged() {
    scene_->syncPositionsToDocument();
    scene_->loadFromDocument();
}

}  // namespace topology
