#include "TopologyEditorWidget.h"
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include "DeviceTemplateManager.h"
#include "PropertyPanelWidget.h"
#include "TopologyDocument.h"
#include "TopologyJsonSerializer.h"
#include "TopologyScene.h"
#include "TopologyView.h"
#include "UndoCommands.h"
#include "topology_items.h"

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

// ── IEditor interface ──────────────────────────────────────────

QString TopologyEditorWidget::displayName() const {
  if (current_file_.isEmpty()) {
    return QStringLiteral("硬件拓扑(未保存)");
  }
  return QFileInfo(current_file_).fileName();
}

bool TopologyEditorWidget::isModified() const {
  return doc_->isModified();
}

bool TopologyEditorWidget::save() {
  if (current_file_.isEmpty()) {
    return saveAs(QString());
  }
  scene_->syncPositionsToDocument();
  QJsonObject json = TopologyJsonSerializer::serialize(*doc_);
  QJsonDocument jdoc(json);

  QFile file(current_file_);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(jdoc.toJson(QJsonDocument::Indented));
  file.close();

  doc_->undoStack()->setClean();
  return true;
}

bool TopologyEditorWidget::saveAs(const QString& path) {
  QString savePath = path;
  if (savePath.isEmpty()) {
    savePath = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存拓扑文件"), QString(),
        QStringLiteral("拓扑文件 (*.etopo);;所有文件 (*)"));
    if (savePath.isEmpty())
      return false;
    if (!savePath.endsWith(QStringLiteral(".etopo"), Qt::CaseInsensitive)) {
      savePath += QStringLiteral(".etopo");
    }
  }

  QString oldId = editorId();
  current_file_ = savePath;
  bool ok = save();
  if (ok) {
    emit editorTitleChanged(QStringLiteral("拓扑编辑器 - %1").arg(savePath));
    emit editorIdChanged(oldId, editorId());
  } else {
    current_file_.clear();
  }
  return ok;
}

QString TopologyEditorWidget::filePath() const {
  return current_file_;
}

QString TopologyEditorWidget::editorId() const {
  if (current_file_.isEmpty()) {
    return QStringLiteral("editor://topology/new");
  }
  return current_file_;
}

QWidget* TopologyEditorWidget::widget() {
  return this;
}

QString TopologyEditorWidget::editorType() const {
  return QStringLiteral("topology");
}

QObject* TopologyEditorWidget::signalObject() {
  return this;
}

// ── Topology specific ──────────────────────────────────────────

TopologyDocument* TopologyEditorWidget::document() const {
  return doc_;
}

void TopologyEditorWidget::reloadScene() {
  scene_->loadFromDocument();
}

void TopologyEditorWidget::setEditorId(const QString& newId) {
  QString oldId = editorId();
  current_file_ = newId;
  if (oldId != editorId()) {
    emit editorIdChanged(oldId, editorId());
  }
}

// ── Constructor helpers ────────────────────────────────────────

void TopologyEditorWidget::initUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  auto* toolbarFrame = new QFrame(this);
  auto* toolbarLayout = new QHBoxLayout(toolbarFrame);
  toolbarLayout->setContentsMargins(4, 4, 4, 4);

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

  toolbarLayout->addWidget(new QLabel(QStringLiteral("  |  "), toolbarFrame));

  undo_action_ = new QAction(QStringLiteral("撤销"), this);
  undo_action_->setShortcut(QKeySequence::Undo);
  undo_action_->setEnabled(false);
  auto* undoBtn = new QToolButton(toolbarFrame);
  undoBtn->setDefaultAction(undo_action_);
  toolbarLayout->addWidget(undoBtn);

  redo_action_ = new QAction(QStringLiteral("重做"), this);
  redo_action_->setShortcut(QKeySequence::Redo);
  redo_action_->setEnabled(false);
  auto* redoBtn = new QToolButton(toolbarFrame);
  redoBtn->setDefaultAction(redo_action_);
  toolbarLayout->addWidget(redoBtn);

  toolbarLayout->addStretch();
  mainLayout->addWidget(toolbarFrame);

  splitter_ = new QSplitter(Qt::Horizontal, this);
  splitter_->addWidget(view_);
  splitter_->addWidget(property_panel_);
  splitter_->setStretchFactor(0, 4);
  splitter_->setStretchFactor(1, 1);
  mainLayout->addWidget(splitter_, 1);

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

  connect(undo_action_, &QAction::triggered, this,
          &TopologyEditorWidget::onUndo);
  connect(redo_action_, &QAction::triggered, this,
          &TopologyEditorWidget::onRedo);

  connect(view_, &TopologyView::addUutRequested, this,
          &TopologyEditorWidget::onAddUut);
  connect(view_, &TopologyView::addDeviceRequested, this,
          &TopologyEditorWidget::onAddDevice);
  connect(view_, &TopologyView::deleteItemRequested, this,
          &TopologyEditorWidget::onDeleteItem);
  connect(view_, &TopologyView::saveTemplateRequested, this,
          &TopologyEditorWidget::onSaveTemplate);

  connect(scene_, &TopologyScene::itemSelected, this,
          &TopologyEditorWidget::onSelectionChanged);

  connect(property_panel_, &PropertyPanelWidget::documentChanged, this,
          &TopologyEditorWidget::onDocumentChanged);

  auto* undoStack = doc_->undoStack();
  connect(undoStack, &QUndoStack::indexChanged, this, [this]() {
    rebuildSceneAndRestoreSelection();
  });
  connect(undoStack, &QUndoStack::canUndoChanged, undo_action_,
          &QAction::setEnabled);
  connect(undoStack, &QUndoStack::canRedoChanged, redo_action_,
          &QAction::setEnabled);
  connect(undoStack, &QUndoStack::cleanChanged, this,
          [this](bool clean) { emit modificationChanged(!clean); });

  auto* delShortcut = new QShortcut(QKeySequence::Delete, this);
  connect(delShortcut, &QShortcut::activated, this, [this]() {
    auto selected = scene_->selectedItems();
    if (!selected.isEmpty()) {
      onDeleteItem(selected.first());
    }
  });
}

void TopologyEditorWidget::buildDefaultDocument() {
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

  TopologyProduct prod2;
  prod2.name = QStringLiteral("ISI-02");
  prod2.position = QPointF(450, 320);
  prod2.ports.append({QStringLiteral("A429_CH1"),
                      TopologyPort::Bidirectional,
                      {QStringLiteral("A429")}});
  doc_->addProduct(prod2);

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
  doc_->undoStack()->clear();
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

  auto* cmd = new AddProductCommand(doc_, prod);
  doc_->undoStack()->push(cmd);
  status_label_->setText(QStringLiteral("已添加 UUT: %1").arg(prod.name));
}

void TopologyEditorWidget::onAddDevice(const QPointF& scenePos) {
  int n = doc_->deviceCount() + 1;
  TopologyDevice dev;
  dev.name = QStringLiteral("Device-%1").arg(n, 2, 10, QChar('0'));
  dev.deviceType = QStringLiteral("EPH6272T");
  dev.position = (scenePos.isNull()) ? QPointF(50, 100 + n * 80) : scenePos;

  auto* cmd = new AddDeviceCommand(doc_, dev);
  doc_->undoStack()->push(cmd);
  status_label_->setText(QStringLiteral("已添加设备: %1").arg(dev.name));
}

void TopologyEditorWidget::onDeleteItem(QGraphicsItem* item) {
  if (!item)
    return;

  if (auto* uut = qgraphicsitem_cast<UutItem*>(item)) {
    auto* cmd = new RemoveProductCommand(doc_, uut->productIndex());
    doc_->undoStack()->push(cmd);
    status_label_->setText(QStringLiteral("已删除 UUT"));
  } else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
    auto* cmd = new RemoveDeviceCommand(doc_, dev->deviceIndex());
    doc_->undoStack()->push(cmd);
    status_label_->setText(QStringLiteral("已删除设备"));
  } else if (auto* devPort = qgraphicsitem_cast<DevicePortItem*>(item)) {
    doc_->removeDevicePort(devPort->deviceIndex(), devPort->portIndex());
    onDocumentChanged();
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
            doc_->undoStack()->push(
                new RemoveConnectionCommand(doc_, i));
            break;
          }
        }
      }
    }
    status_label_->setText(QStringLiteral("已删除连线"));
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

void TopologyEditorWidget::onSelectionChanged(QGraphicsItem* item) {
  if (item) {
    property_panel_->showPropertiesFor(item);
  } else {
    property_panel_->clearPanel();
  }
}

void TopologyEditorWidget::rebuildSceneAndRestoreSelection() {
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

void TopologyEditorWidget::onDocumentChanged() {
  rebuildSceneAndRestoreSelection();
}

void TopologyEditorWidget::onUndo() {
  doc_->undoStack()->undo();
}

void TopologyEditorWidget::onRedo() {
  doc_->undoStack()->redo();
}

}  // namespace etest::topology
