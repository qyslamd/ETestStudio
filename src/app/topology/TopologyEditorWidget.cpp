#include "TopologyEditorWidget.h"
#include "DeviceTemplateManager.h"
#include "PropertyPanelWidget.h"
#include "TopologyDocument.h"
#include "TopologyJsonSerializer.h"
#include "TopologyScene.h"
#include "TopologyView.h"
#include "topology_items.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

namespace etest::topology {

TopologyEditorWidget::TopologyEditorWidget(QWidget* parent) : QWidget(parent) {
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
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ── Toolbar frame ──
  auto* toolbarFrame = new QFrame(this);
  auto* toolbarLayout = new QHBoxLayout(toolbarFrame);
  toolbarLayout->setContentsMargins(4, 4, 4, 4);

  // File menu button
  auto* fileBtn = new QToolButton(toolbarFrame);
  fileBtn->setText(QStringLiteral("文件"));
  fileBtn->setPopupMode(QToolButton::InstantPopup);
  auto* fileMenu = new QMenu(fileBtn);

  auto* newAct = fileMenu->addAction(QStringLiteral("新建(&N)"));
  newAct->setShortcut(QKeySequence::New);
  connect(newAct, &QAction::triggered, this, &TopologyEditorWidget::onNewFile);

  auto* openAct = fileMenu->addAction(QStringLiteral("打开(&O)..."));
  openAct->setShortcut(QKeySequence::Open);
  connect(openAct, &QAction::triggered, this,
          &TopologyEditorWidget::onOpenFile);

  auto* saveAct = fileMenu->addAction(QStringLiteral("保存(&S)"));
  saveAct->setShortcut(QKeySequence::Save);
  connect(saveAct, &QAction::triggered, this,
          &TopologyEditorWidget::onSaveFile);

  auto* saveAsAct = fileMenu->addAction(QStringLiteral("另存为(&A)..."));
  saveAsAct->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_S));
  connect(saveAsAct, &QAction::triggered, this,
          &TopologyEditorWidget::onSaveAsFile);

  fileMenu->addSeparator();

  auto* exitAct = fileMenu->addAction(QStringLiteral("退出(&X)"));
  exitAct->setShortcut(QKeySequence::Quit);
  connect(exitAct, &QAction::triggered, this, &QWidget::close);

  fileBtn->setMenu(fileMenu);
  toolbarLayout->addWidget(fileBtn);

  toolbarLayout->addWidget(new QLabel(QStringLiteral("  |  "), toolbarFrame));

  // Edit buttons
  add_uut_action_ = new QAction(QStringLiteral("+ UUT"), this);
  add_uut_action_->setToolTip(QStringLiteral("添加被测产品"));
  auto* addUutBtn = new QToolButton(toolbarFrame);
  addUutBtn->setDefaultAction(add_uut_action_);
  toolbarLayout->addWidget(addUutBtn);

  add_device_action_ = new QAction(QStringLiteral("+ 设备"), this);
  add_device_action_->setToolTip(QStringLiteral("添加激励设备"));
  auto* addDeviceBtn = new QToolButton(toolbarFrame);
  addDeviceBtn->setDefaultAction(add_device_action_);
  toolbarLayout->addWidget(addDeviceBtn);

  toolbarLayout->addWidget(new QLabel(QStringLiteral("  |  "), toolbarFrame));

  zoom_in_action_ = new QAction(QStringLiteral("放大"), this);
  auto* zoomInBtn = new QToolButton(toolbarFrame);
  zoomInBtn->setDefaultAction(zoom_in_action_);
  toolbarLayout->addWidget(zoomInBtn);

  zoom_out_action_ = new QAction(QStringLiteral("缩小"), this);
  auto* zoomOutBtn = new QToolButton(toolbarFrame);
  zoomOutBtn->setDefaultAction(zoom_out_action_);
  toolbarLayout->addWidget(zoomOutBtn);

  zoom_reset_action_ = new QAction(QStringLiteral("重置"), this);
  auto* zoomResetBtn = new QToolButton(toolbarFrame);
  zoomResetBtn->setDefaultAction(zoom_reset_action_);
  toolbarLayout->addWidget(zoomResetBtn);

  toolbarLayout->addStretch();
  mainLayout->addWidget(toolbarFrame);

  // ── Central splitter ──
  splitter_ = new QSplitter(Qt::Horizontal, this);
  splitter_->addWidget(view_);
  splitter_->addWidget(property_panel_);
  splitter_->setStretchFactor(0, 4);
  splitter_->setStretchFactor(1, 1);
  mainLayout->addWidget(splitter_, 1);

  // ── Status frame ──
  auto* statusFrame = new QFrame(this);
  statusFrame->setFrameShape(QFrame::StyledPanel);
  auto* statusLayout = new QHBoxLayout(statusFrame);
  statusLayout->setContentsMargins(8, 2, 8, 2);
  status_label_ = new QLabel(QStringLiteral("就绪"), statusFrame);
  statusLayout->addWidget(status_label_);
  statusLayout->addStretch();
  mainLayout->addWidget(statusFrame);
}

void TopologyEditorWidget::initSignals() {
  connect(add_uut_action_, &QAction::triggered, this, [this]() { onAddUut(); });
  connect(add_device_action_, &QAction::triggered, this,
          [this]() { onAddDevice(); });

  connect(zoom_in_action_, &QAction::triggered, view_, &TopologyView::zoomIn);
  connect(zoom_out_action_, &QAction::triggered, view_, &TopologyView::zoomOut);
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

  // Delete key → delete selected item
  auto* delShortcut = new QShortcut(QKeySequence::Delete, this);
  connect(delShortcut, &QShortcut::activated, this, [this]() {
    auto selected = scene_->selectedItems();
    if (!selected.isEmpty()) {
      onDeleteItem(selected.first());
    }
  });
}

void TopologyEditorWidget::buildDefaultDocument() {
  // Product 1
  TopologyProduct prod1;
  prod1.name = QStringLiteral("ISI-01");
  prod1.position = QPointF(450, 120);
  prod1.ports.append({QStringLiteral("A429_CH1"),
                      TopologyPort::Bidirectional,
                      {QStringLiteral("A429")}});
  prod1.ports.append({QStringLiteral("A429_CH2"),
                      TopologyPort::Bidirectional,
                      {QStringLiteral("A429")}});
  prod1.ports.append({QStringLiteral("A429_CH3"),
                      TopologyPort::Bidirectional,
                      {QStringLiteral("A429")}});
  prod1.ports.append({QStringLiteral("离散量"),
                      TopologyPort::Input,
                      {QStringLiteral("DISCRETE")}});
  doc_->addProduct(prod1);

  // Product 2
  TopologyProduct prod2;
  prod2.name = QStringLiteral("ISI-02");
  prod2.position = QPointF(450, 320);
  prod2.ports.append({QStringLiteral("A429_CH1"),
                      TopologyPort::Bidirectional,
                      {QStringLiteral("A429")}});
  doc_->addProduct(prod2);

  // Device 1
  TopologyDevice dev1;
  dev1.name = QStringLiteral("6272T_00");
  dev1.deviceType = QStringLiteral("EPH6272T");
  dev1.position = QPointF(50, 80);
  dev1.properties = {{QStringLiteral("bus"), QStringLiteral("PXI6")},
                     {QStringLiteral("slot"), QStringLiteral("2")}};
  dev1.ports.append(
      {QStringLiteral("ch0"), TopologyPort::Bidirectional, FunctionType::A429});
  dev1.ports.append(
      {QStringLiteral("ch1"), TopologyPort::Bidirectional, FunctionType::A429});
  doc_->addDevice(dev1);

  // Device 2
  TopologyDevice dev2;
  dev2.name = QStringLiteral("6272T_01");
  dev2.deviceType = QStringLiteral("EPH6272T");
  dev2.position = QPointF(50, 200);
  dev2.properties = {{QStringLiteral("bus"), QStringLiteral("PXI6")},
                     {QStringLiteral("slot"), QStringLiteral("3")}};
  dev2.ports.append(
      {QStringLiteral("ch0"), TopologyPort::Bidirectional, FunctionType::A429});
  dev2.ports.append(
      {QStringLiteral("ch1"), TopologyPort::Bidirectional, FunctionType::A429});
  doc_->addDevice(dev2);

  // Device 3
  TopologyDevice dev3;
  dev3.name = QStringLiteral("EPH5121A_00");
  dev3.deviceType = QStringLiteral("EPH5121A");
  dev3.position = QPointF(50, 350);
  dev3.properties = {{QStringLiteral("bus"), QStringLiteral("PXI6")},
                     {QStringLiteral("slot"), QStringLiteral("5")}};
  dev3.ports.append({QStringLiteral("ch0"), TopologyPort::Bidirectional,
                     FunctionType::DISCRETE});
  doc_->addDevice(dev3);

  scene_->loadFromDocument();
}

// ── Slots ──────────────────────────────────────────────────────

void TopologyEditorWidget::onAddUut(const QPointF& scenePos) {
  int n = doc_->productCount() + 1;
  TopologyProduct prod;
  prod.name = QStringLiteral("UUT-%1").arg(n, 2, 10, QChar('0'));
  prod.position = (scenePos.isNull()) ? QPointF(450, 100 + n * 80) : scenePos;
  prod.ports.append({QStringLiteral("Port_IN1"),
                     TopologyPort::Input,
                     {QStringLiteral("A429")}});
  prod.ports.append({QStringLiteral("Port_OUT1"),
                     TopologyPort::Output,
                     {QStringLiteral("A429")}});

  int idx = doc_->addProduct(prod);
  scene_->addProductItem(idx, prod.position);
  status_label_->setText(QStringLiteral("已添加 UUT: %1").arg(prod.name));
}

void TopologyEditorWidget::onAddDevice(const QPointF& scenePos) {
  int n = doc_->deviceCount() + 1;
  TopologyDevice dev;
  dev.name = QStringLiteral("Device-%1").arg(n, 2, 10, QChar('0'));
  dev.deviceType = QStringLiteral("EPH6272T");
  dev.position = (scenePos.isNull()) ? QPointF(50, 100 + n * 80) : scenePos;

  int idx = doc_->addDevice(dev);
  scene_->addDeviceItem(idx, dev.position);
  status_label_->setText(QStringLiteral("已添加设备: %1").arg(dev.name));
}

void TopologyEditorWidget::onDeleteItem(QGraphicsItem* item) {
  if (!item)
    return;

  bool removed = false;

  if (auto* uut = qgraphicsitem_cast<UutItem*>(item)) {
    const auto* prod = doc_->product(uut->productIndex());
    if (prod) {
      for (int i = doc_->connectionCount() - 1; i >= 0; --i) {
        if (doc_->connection(i)->productName == prod->name) {
          doc_->removeConnection(i);
        }
      }
    }
    doc_->removeProduct(uut->productIndex());
    removed = true;
    status_label_->setText(QStringLiteral("已删除 UUT"));
  } else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
    const auto* d = doc_->device(dev->deviceIndex());
    if (d) {
      for (int i = doc_->connectionCount() - 1; i >= 0; --i) {
        if (doc_->connection(i)->deviceName == d->name) {
          doc_->removeConnection(i);
        }
      }
    }
    doc_->removeDevice(dev->deviceIndex());
    removed = true;
    status_label_->setText(QStringLiteral("已删除设备"));
  } else if (auto* devPort = qgraphicsitem_cast<DevicePortItem*>(item)) {
    doc_->removeDevicePort(devPort->deviceIndex(), devPort->portIndex());
    removed = true;
    status_label_->setText(QStringLiteral("已删除设备端口"));
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
              c->deviceName == dev->name &&
              c->devicePort == conn->devicePort()) {
            doc_->removeConnection(i);
            break;
          }
        }
      }
    }
    removed = true;
    status_label_->setText(QStringLiteral("已删除连线"));
  }

  if (removed) {
    scene_->loadFromDocument();
  }
}

void TopologyEditorWidget::onSaveTemplate(QGraphicsItem* item) {
  auto* dev = qgraphicsitem_cast<DeviceItem*>(item);
  if (!dev)
    return;

  QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("保存设备模板"), QString(),
      QStringLiteral("设备模板 (*.dvt)"));
  if (path.isEmpty())
    return;

  if (DeviceTemplateManager::saveTemplate(doc_, dev->deviceIndex(), path)) {
    status_label_->setText(QStringLiteral("模板已保存: %1").arg(path));
  } else {
    QMessageBox::warning(this, QStringLiteral("错误"),
                         QStringLiteral("保存模板失败"));
  }
}

void TopologyEditorWidget::onNewFile() {
  doc_->clear();
  scene_->loadFromDocument();
  current_file_.clear();
  emit editorTitleChanged(QStringLiteral("拓扑编辑器 - [未命名]"));
  status_label_->setText(QStringLiteral("新建文件"));
}

void TopologyEditorWidget::onOpenFile() {
  QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("打开拓扑文件"), QString(),
      QStringLiteral("拓扑文件 (*.json);;所有文件 (*)"));
  if (path.isEmpty())
    return;

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
  emit editorTitleChanged(QStringLiteral("拓扑编辑器 - %1").arg(path));
  status_label_->setText(QStringLiteral("已打开: %1").arg(path));
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

  status_label_->setText(QStringLiteral("已保存: %1").arg(current_file_));
}

void TopologyEditorWidget::onSaveAsFile() {
  QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("保存拓扑文件"), QString(),
      QStringLiteral("拓扑文件 (*.json);;所有文件 (*)"));
  if (path.isEmpty())
    return;

  // Ensure .json extension
  if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
    path += QStringLiteral(".json");
  }

  current_file_ = path;
  onSaveFile();
  emit editorTitleChanged(QStringLiteral("拓扑编辑器 - %1").arg(path));
}

void TopologyEditorWidget::onSelectionChanged(QGraphicsItem* item) {
  if (item) {
    property_panel_->showPropertiesFor(item);
  } else {
    property_panel_->clearPanel();
  }
}

void TopologyEditorWidget::onDocumentChanged() {
  // Save selection identity before reload
  int selType = -1, selIdx1 = -1, selIdx2 = -1;
  auto selItems = scene_->selectedItems();
  if (!selItems.isEmpty()) {
    auto* item = selItems.first();
    if (auto* uut = qgraphicsitem_cast<UutItem*>(item)) {
      selType = 0;
      selIdx1 = uut->productIndex();
    } else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
      selType = 1;
      selIdx1 = dev->deviceIndex();
    } else if (auto* p = qgraphicsitem_cast<PortItem*>(item)) {
      selType = 2;
      selIdx1 = p->productIndex();
      selIdx2 = p->portIndex();
    } else if (auto* dp = qgraphicsitem_cast<DevicePortItem*>(item)) {
      selType = 3;
      selIdx1 = dp->deviceIndex();
      selIdx2 = dp->portIndex();
    }
  }

  scene_->syncPositionsToDocument();
  scene_->loadFromDocument();

  // Restore selection on newly created items
  QGraphicsItem* newItem = nullptr;
  if (selType == 0) {
    auto* uut = scene_->findUutItem(selIdx1);
    if (uut) {
      uut->setSelected(true);
      newItem = uut;
    }
  } else if (selType == 1) {
    auto* dev = scene_->findDeviceItem(selIdx1);
    if (dev) {
      dev->setSelected(true);
      newItem = dev;
    }
  } else if (selType == 2) {
    auto* uut = scene_->findUutItem(selIdx1);
    if (uut) {
      auto* port = uut->portItem(selIdx2);
      if (port) {
        port->setSelected(true);
        newItem = port;
      }
    }
  } else if (selType == 3) {
    auto* dev = scene_->findDeviceItem(selIdx1);
    if (dev) {
      auto* dp = dev->devicePortItem(selIdx2);
      if (dp) {
        dp->setSelected(true);
        newItem = dp;
      }
    }
  }

  if (newItem) {
    property_panel_->showPropertiesFor(newItem);
  } else {
    property_panel_->clearPanel();
  }
}

}  // namespace etest::topology
