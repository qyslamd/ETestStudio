#include "TopologyEditorWidget.h"
#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
#include <QSplitter>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include "DevicePaletteWidget.h"
#include "DeviceTemplateManager.h"
#include "PropertyPanelWidget.h"
#include "TopologyTheme.h"
#include "TopologyDocument.h"
#include "TopologyJsonSerializer.h"
#include "TopologyScene.h"
#include "TopologyView.h"
#include "UndoCommands.h"
#include "topology_items.h"

namespace etest::topology {

TopologyEditorWidget::TopologyEditorWidget(QWidget* parent) : QFrame(parent) {
  setAutoFillBackground(true);

  doc_ = new TopologyDocument(this);
  scene_ = new TopologyScene(doc_, this);
  view_ = new TopologyView(scene_, this);
  property_panel_ = new PropertyPanelWidget(doc_, this);
  property_panel_->setAutoFillBackground(true);

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
  if (file.write(jdoc.toJson(QJsonDocument::Indented)) == -1) {
    file.close();
    return false;
  }
  file.close();

  doc_->undoStack()->setClean();
  // 显式通知修改状态更新，确保信号链可靠传递
  emit modificationChanged(false);
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
  toolbarFrame->setObjectName(QStringLiteral("topologyToolbar"));
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

  // ── 排列 (Align) ──
  auto* alignBtn = new QToolButton(toolbarFrame);
  alignBtn->setText(QStringLiteral("排列"));
  alignBtn->setPopupMode(QToolButton::InstantPopup);
  auto* alignMenu = new QMenu(alignBtn);
  align_left_action_ = alignMenu->addAction(QStringLiteral("左对齐"));
  align_hcenter_action_ = alignMenu->addAction(QStringLiteral("水平居中"));
  align_right_action_ = alignMenu->addAction(QStringLiteral("右对齐"));
  alignMenu->addSeparator();
  align_top_action_ = alignMenu->addAction(QStringLiteral("顶端对齐"));
  align_vcenter_action_ = alignMenu->addAction(QStringLiteral("垂直居中"));
  align_bottom_action_ = alignMenu->addAction(QStringLiteral("底端对齐"));
  alignBtn->setMenu(alignMenu);
  toolbarLayout->addWidget(alignBtn);

  // ── 位置 (Distribute) ──
  auto* distributeBtn = new QToolButton(toolbarFrame);
  distributeBtn->setText(QStringLiteral("位置"));
  distributeBtn->setPopupMode(QToolButton::InstantPopup);
  auto* distributeMenu = new QMenu(distributeBtn);
  distribute_horizontal_action_ = distributeMenu->addAction(QStringLiteral("横向分布"));
  distribute_vertical_action_ = distributeMenu->addAction(QStringLiteral("纵向分布"));
  distributeBtn->setMenu(distributeMenu);
  toolbarLayout->addWidget(distributeBtn);

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

  export_image_action_ = new QAction(QStringLiteral("导出图片"), this);
  export_image_action_->setToolTip(QStringLiteral("导出拓扑图为 PNG"));
  auto* exportBtn = new QToolButton(toolbarFrame);
  exportBtn->setDefaultAction(export_image_action_);
  toolbarLayout->addWidget(exportBtn);

  toolbarLayout->addWidget(new QLabel(QStringLiteral("  |  "), toolbarFrame));

  undo_action_ = new QAction(QStringLiteral("撤销"), this);
  // 主窗口通过 IEditor 接口统一管理撤销/重做快捷键
  undo_action_->setEnabled(false);
  auto* undoBtn = new QToolButton(toolbarFrame);
  undoBtn->setDefaultAction(undo_action_);
  toolbarLayout->addWidget(undoBtn);

  redo_action_ = new QAction(QStringLiteral("重做"), this);
  redo_action_->setEnabled(false);
  auto* redoBtn = new QToolButton(toolbarFrame);
  redoBtn->setDefaultAction(redo_action_);
  toolbarLayout->addWidget(redoBtn);

  toolbarLayout->addStretch();
  mainLayout->addWidget(toolbarFrame);

  device_palette_ = new DevicePaletteWidget(this);

  splitter_ = new QSplitter(Qt::Horizontal, this);
  splitter_->addWidget(device_palette_);
  splitter_->addWidget(view_);
  splitter_->addWidget(property_panel_);
  splitter_->setStretchFactor(0, 0);  // palette — fixed width
  splitter_->setStretchFactor(1, 4);  // view — stretch
  splitter_->setStretchFactor(2, 1);  // property panel — stretch less
  mainLayout->addWidget(splitter_, 1);

  auto* statusFrame = new QFrame(this);
  statusFrame->setObjectName(QStringLiteral("topologyStatusBar"));
  auto* statusLayout = new QHBoxLayout(statusFrame);
  statusLayout->setContentsMargins(8, 2, 8, 2);
  status_label_ = new QLabel(QStringLiteral("缩放: 100%"), statusFrame);
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

  connect(export_image_action_, &QAction::triggered, this,
          &TopologyEditorWidget::onExportImage);

  connect(view_, &TopologyView::zoomChanged, this,
          [this](qreal zoom) {
            status_label_->setText(
                QStringLiteral("缩放: %1%").arg(static_cast<int>(zoom * 100)));
          });

  connect(undo_action_, &QAction::triggered, this,
          &TopologyEditorWidget::onUndo);
  connect(redo_action_, &QAction::triggered, this,
          &TopologyEditorWidget::onRedo);

  // ── Align / Distribute ──
  connect(align_left_action_, &QAction::triggered, this, [this]() { doAlign(0); });
  connect(align_hcenter_action_, &QAction::triggered, this, [this]() { doAlign(1); });
  connect(align_right_action_, &QAction::triggered, this, [this]() { doAlign(2); });
  connect(align_top_action_, &QAction::triggered, this, [this]() { doAlign(3); });
  connect(align_vcenter_action_, &QAction::triggered, this, [this]() { doAlign(4); });
  connect(align_bottom_action_, &QAction::triggered, this, [this]() { doAlign(5); });
  connect(distribute_horizontal_action_, &QAction::triggered, this,
          [this]() { doDistribute(0); });
  connect(distribute_vertical_action_, &QAction::triggered, this,
          [this]() { doDistribute(1); });
  connect(scene_, &QGraphicsScene::selectionChanged, this,
          &TopologyEditorWidget::updateAlignDistributeActions);

  connect(view_, &TopologyView::addUutRequested, this,
          &TopologyEditorWidget::onAddUut);
  connect(view_, &TopologyView::addDeviceRequested, this,
          &TopologyEditorWidget::onAddDevice);
  connect(view_, &TopologyView::deleteItemRequested, this,
          &TopologyEditorWidget::onDeleteItem);
  connect(view_, &TopologyView::saveTemplateRequested, this,
          &TopologyEditorWidget::onSaveTemplate);
  connect(view_, &TopologyView::addDeviceFromTemplateRequested, this,
          &TopologyEditorWidget::onAddDeviceFromTemplate);

  connect(scene_, &TopologyScene::itemSelected, this,
          &TopologyEditorWidget::onSelectionChanged);

  connect(scene_, &TopologyScene::deviceDropped, this,
          &TopologyEditorWidget::onDropDevice);

  connect(property_panel_, &PropertyPanelWidget::documentChanged, this,
          &TopologyEditorWidget::onDocumentChanged);

  auto* undoStack = doc_->undoStack();
  connect(undoStack, &QUndoStack::indexChanged, this,
          [this]() { rebuildSceneAndRestoreSelection(); });
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

  auto* copyShortcut = new QShortcut(QKeySequence::Copy, this);
  connect(copyShortcut, &QShortcut::activated, this,
          &TopologyEditorWidget::onCopy);

  auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this);
  connect(pasteShortcut, &QShortcut::activated, this,
          &TopologyEditorWidget::onPaste);

  updateAlignDistributeActions();
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

void TopologyEditorWidget::onDropDevice(const QString& deviceType,
                                        int channelCount,
                                        int direction,
                                        int functionType,
                                        const QPointF& scenePos) {
  int n = doc_->deviceCount() + 1;
  TopologyDevice dev;
  dev.deviceType = deviceType;
  dev.name =
      QStringLiteral("%1_%2").arg(deviceType).arg(n, 2, 10, QChar('0'));
  dev.position = scenePos;

  for (int i = 0; i < channelCount; ++i) {
    TopologyDevicePort dp;
    dp.name = QStringLiteral("ch%1").arg(i);
    dp.direction = static_cast<TopologyPort::Direction>(direction);
    dp.functionType = static_cast<FunctionType>(functionType);
    dev.ports.append(dp);
  }

  auto* cmd = new AddDeviceCommand(doc_, dev);
  doc_->undoStack()->push(cmd);
  status_label_->setText(
      QStringLiteral("已拖放添加设备: %1").arg(dev.name));
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
            doc_->undoStack()->push(new RemoveConnectionCommand(doc_, i));
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
  // 属性面板可能blockSignals导致cleanChanged丢失，此处显式同步修改状态
  emit modificationChanged(!doc_->undoStack()->isClean());
}

void TopologyEditorWidget::onUndo() {
  doc_->undoStack()->undo();
}

void TopologyEditorWidget::onRedo() {
  doc_->undoStack()->redo();
}

// ── IEditor undo/redo ───────────────────────────────────────

bool TopologyEditorWidget::canUndo() const {
  return doc_->undoStack()->canUndo();
}

bool TopologyEditorWidget::canRedo() const {
  return doc_->undoStack()->canRedo();
}

void TopologyEditorWidget::undo() {
  doc_->undoStack()->undo();
}

void TopologyEditorWidget::redo() {
  doc_->undoStack()->redo();
}

// ── Copy / Paste ────────────────────────────────────────────

static const char kClipboardMime[] = "application/x-ietopology-items";

void TopologyEditorWidget::onCopy() {
  QJsonObject root;
  QJsonArray prodsArr, devsArr;
  auto selected = scene_->selectedItems();

  for (auto* item : selected) {
    if (auto* uut = qgraphicsitem_cast<UutItem*>(item)) {
      const auto* prod = doc_->product(uut->productIndex());
      if (!prod)
        continue;
      QJsonObject obj;
      obj["name"] = prod->name;
      obj["positionX"] = prod->position.x();
      obj["positionY"] = prod->position.y();
      QJsonArray portsArr;
      for (const auto& port : prod->ports) {
        QJsonObject pObj;
        pObj["name"] = port.name;
        pObj["direction"] = directionToString(port.direction);
        QJsonArray typesArr;
        for (const auto& t : port.allowedDeviceTypes)
          typesArr.append(t);
        pObj["allowedDeviceTypes"] = typesArr;
        pObj["functionType"] = functionTypeToString(port.functionType);
        portsArr.append(pObj);
      }
      obj["ports"] = portsArr;
      prodsArr.append(obj);
    } else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(item)) {
      const auto* d = doc_->device(dev->deviceIndex());
      if (!d)
        continue;
      QJsonObject obj;
      obj["name"] = d->name;
      obj["deviceType"] = d->deviceType;
      obj["positionX"] = d->position.x();
      obj["positionY"] = d->position.y();
      QJsonArray propsArr;
      for (const auto& prop : d->properties) {
        QJsonObject propObj;
        propObj["key"] = prop.first;
        propObj["value"] = prop.second;
        propsArr.append(propObj);
      }
      obj["properties"] = propsArr;
      QJsonArray portsArr;
      for (const auto& dp : d->ports) {
        QJsonObject dpObj;
        dpObj["name"] = dp.name;
        dpObj["direction"] = directionToString(dp.direction);
        dpObj["functionType"] = functionTypeToString(dp.functionType);
        portsArr.append(dpObj);
      }
      obj["ports"] = portsArr;
      devsArr.append(obj);
    }
  }

  if (prodsArr.isEmpty() && devsArr.isEmpty())
    return;

  root["products"] = prodsArr;
  root["devices"] = devsArr;
  QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);

  auto* clip = QApplication::clipboard();
  auto* mime = new QMimeData();
  mime->setData(QLatin1String(kClipboardMime), data);
  clip->setMimeData(mime);
  status_label_->setText(QStringLiteral("已复制 %1 个 UUT, %2 个设备")
                             .arg(prodsArr.size())
                             .arg(devsArr.size()));
}

void TopologyEditorWidget::onPaste() {
  auto* clip = QApplication::clipboard();
  auto* mimeData = clip->mimeData();
  if (!mimeData || !mimeData->hasFormat(QLatin1String(kClipboardMime)))
    return;

  QJsonDocument jdoc =
      QJsonDocument::fromJson(mimeData->data(QLatin1String(kClipboardMime)));
  if (!jdoc.isObject())
    return;

  QJsonObject root = jdoc.object();
  QJsonArray prodsArr = root["products"].toArray();
  QJsonArray devsArr = root["devices"].toArray();

  scene_->clearSelection();

  // Paste products with offset and unique names
  for (const auto& val : prodsArr) {
    QJsonObject obj = val.toObject();
    TopologyProduct prod;
    prod.name = obj["name"].toString();
    prod.position = QPointF(obj["positionX"].toDouble() + 30,
                            obj["positionY"].toDouble() + 30);

    // Generate unique name if conflict
    if (doc_->findProductIndex(prod.name) >= 0) {
      int suffix = 1;
      QString base = prod.name;
      while (doc_->findProductIndex(
                 QStringLiteral("%1_copy%2").arg(base).arg(suffix)) >= 0)
        ++suffix;
      prod.name = QStringLiteral("%1_copy%2").arg(base).arg(suffix);
    }

    QJsonArray portsArr = obj["ports"].toArray();
    for (const auto& pVal : portsArr) {
      QJsonObject pObj = pVal.toObject();
      TopologyPort port;
      port.name = pObj["name"].toString();
      port.direction = stringToDirection(
          pObj["direction"].toString(QStringLiteral("output")));
      for (const auto& t : pObj["allowedDeviceTypes"].toArray())
        port.allowedDeviceTypes.append(t.toString());
      port.functionType = stringToFunctionType(pObj["functionType"].toString());
      prod.ports.append(port);
    }

    auto* cmd = new AddProductCommand(doc_, prod);
    doc_->undoStack()->push(cmd);
  }

  // Paste devices with offset and unique names
  for (const auto& val : devsArr) {
    QJsonObject obj = val.toObject();
    TopologyDevice dev;
    dev.name = obj["name"].toString();
    dev.deviceType = obj["deviceType"].toString();
    dev.position = QPointF(obj["positionX"].toDouble() + 30,
                           obj["positionY"].toDouble() + 30);

    // Generate unique name if conflict
    if (doc_->findDeviceIndex(dev.name) >= 0) {
      int suffix = 1;
      QString base = dev.name;
      while (doc_->findDeviceIndex(
                 QStringLiteral("%1_copy%2").arg(base).arg(suffix)) >= 0)
        ++suffix;
      dev.name = QStringLiteral("%1_copy%2").arg(base).arg(suffix);
    }

    QJsonArray propsArr = obj["properties"].toArray();
    for (const auto& propVal : propsArr) {
      QJsonObject propObj = propVal.toObject();
      dev.properties.append(
          {propObj["key"].toString(), propObj["value"].toString()});
    }

    QJsonArray portsArr = obj["ports"].toArray();
    for (const auto& dpVal : portsArr) {
      QJsonObject dpObj = dpVal.toObject();
      TopologyDevicePort dp;
      dp.name = dpObj["name"].toString();
      dp.direction = stringToDirection(
          dpObj["direction"].toString(QStringLiteral("output")));
      dp.functionType = stringToFunctionType(dpObj["functionType"].toString());
      dev.ports.append(dp);
    }

    auto* cmd = new AddDeviceCommand(doc_, dev);
    doc_->undoStack()->push(cmd);
  }

  status_label_->setText(QStringLiteral("已粘贴 %1 个 UUT, %2 个设备")
                             .arg(prodsArr.size())
                             .arg(devsArr.size()));
}

// ── Export Image ──────────────────────────────────────────────

void TopologyEditorWidget::onExportImage() {
  QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出拓扑图"), QString(),
      QStringLiteral("PNG 图片 (*.png);;所有文件 (*)"));
  if (path.isEmpty())
    return;

  if (!path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
    path += QStringLiteral(".png");

  QRectF sceneRect = scene_->sceneRect();
  if (sceneRect.isEmpty())
    return;

  // Add padding
  sceneRect.adjust(-30, -30, 30, 30);

  QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
  image.fill(topologyColors().sceneBackground);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  scene_->render(&painter, QRectF(), sceneRect);
  painter.end();

  if (image.save(path, "PNG")) {
    status_label_->setText(QStringLiteral("拓扑图已导出: %1").arg(path));
  } else {
    QMessageBox::warning(this, QStringLiteral("错误"),
                         QStringLiteral("导出图片失败"));
  }
}

// ── Add Device From Template ──────────────────────────────────

void TopologyEditorWidget::onAddDeviceFromTemplate(const QPointF& scenePos) {
  QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("从模板添加设备"), QString(),
      QStringLiteral("设备模板 (*.dvt);;所有文件 (*)"));
  if (path.isEmpty())
    return;

  QJsonObject deviceType;
  QJsonArray properties, portsArr;
  if (!DeviceTemplateManager::loadTemplate(path, deviceType, properties,
                                           portsArr)) {
    QMessageBox::warning(
        this, QStringLiteral("错误"),
        QStringLiteral("加载模板失败: %1").arg(DeviceTemplateManager::lastError()));
    return;
  }

  int n = doc_->deviceCount() + 1;
  TopologyDevice dev;
  dev.deviceType = deviceType["deviceType"].toString();
  dev.name = QStringLiteral("%1_%2").arg(dev.deviceType).arg(n, 2, 10, QChar('0'));
  dev.position = scenePos;

  // Load properties
  for (const auto& propVal : properties) {
    QJsonObject propObj = propVal.toObject();
    dev.properties.append(
        {propObj["key"].toString(), propObj["value"].toString()});
  }

  // Load ports if the template has them; otherwise add a default port
  if (!portsArr.isEmpty()) {
    for (const auto& pv : portsArr) {
      QJsonObject pObj = pv.toObject();
      TopologyDevicePort dp;
      dp.name = pObj["name"].toString();
      dp.direction =
          stringToDirection(pObj["direction"].toString(QStringLiteral("output")));
      dp.functionType =
          stringToFunctionType(pObj["functionType"].toString());
      dev.ports.append(dp);
    }
  } else {
    dev.ports.append({QStringLiteral("default"), TopologyPort::Bidirectional,
                      FunctionType::CUSTOM});
  }

  auto* cmd = new AddDeviceCommand(doc_, dev);
  doc_->undoStack()->push(cmd);
  status_label_->setText(QStringLiteral("已从模板添加设备: %1").arg(dev.name));
}

// ── Align / Distribute ──────────────────────────────────────────

void TopologyEditorWidget::updateAlignDistributeActions() {
  int movable = 0;
  for (auto* sel : scene_->selectedItems()) {
    if (qgraphicsitem_cast<UutItem*>(sel) ||
        qgraphicsitem_cast<DeviceItem*>(sel)) {
      ++movable;
    }
  }
  bool en = (movable >= 2);
  align_left_action_->setEnabled(en);
  align_hcenter_action_->setEnabled(en);
  align_right_action_->setEnabled(en);
  align_top_action_->setEnabled(en);
  align_vcenter_action_->setEnabled(en);
  align_bottom_action_->setEnabled(en);
  distribute_horizontal_action_->setEnabled(en);
  distribute_vertical_action_->setEnabled(en);
}

void TopologyEditorWidget::doAlign(int alignType) {
  // Collect selected UUT/Device items
  struct Entry {
    QGraphicsItem* item;
    int index;
    bool isProduct;
    QPointF oldPos;
  };
  QVector<Entry> entries;
  for (auto* sel : scene_->selectedItems()) {
    if (auto* uut = qgraphicsitem_cast<UutItem*>(sel))
      entries.append({sel, uut->productIndex(), true, sel->pos()});
    else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(sel))
      entries.append({sel, dev->deviceIndex(), false, sel->pos()});
  }
  if (entries.size() < 2)
    return;

  // Compute union scene bounding rect of all selected items
  QRectF total = entries[0].item->sceneBoundingRect();
  for (int i = 1; i < entries.size(); ++i)
    total = total.united(entries[i].item->sceneBoundingRect());

  // Move items on scene first, then create undo commands
  for (const auto& e : entries) {
    QRectF r = e.item->sceneBoundingRect();
    qreal dx = 0, dy = 0;
    switch (alignType) {
      case 0: dx = total.left() - r.left(); break;                 // 左对齐
      case 1: dx = total.center().x() - r.center().x(); break;     // 水平居中
      case 2: dx = total.right() - r.right(); break;               // 右对齐
      case 3: dy = total.top() - r.top(); break;                   // 顶端对齐
      case 4: dy = total.center().y() - r.center().y(); break;     // 垂直居中
      case 5: dy = total.bottom() - r.bottom(); break;             // 底端对齐
    }
    if (dx != 0 || dy != 0)
      e.item->moveBy(dx, dy);
  }
  scene_->onItemMoved();

  // Create macro command
  auto* macro = new QUndoCommand;
  switch (alignType) {
    case 0: macro->setText(QStringLiteral("左对齐")); break;
    case 1: macro->setText(QStringLiteral("水平居中")); break;
    case 2: macro->setText(QStringLiteral("右对齐")); break;
    case 3: macro->setText(QStringLiteral("顶端对齐")); break;
    case 4: macro->setText(QStringLiteral("垂直居中")); break;
    case 5: macro->setText(QStringLiteral("底端对齐")); break;
  }

  for (const auto& e : entries) {
    QPointF newPos = e.item->pos();
    if (newPos != e.oldPos) {
      if (e.isProduct)
        new MoveProductCommand(doc_, e.index, e.oldPos, newPos, macro);
      else
        new MoveDeviceCommand(doc_, e.index, e.oldPos, newPos, macro);
    }
  }

  if (macro->childCount() > 0) {
    doc_->undoStack()->push(macro);
    status_label_->setText(
        QStringLiteral("排列: %1").arg(macro->text()));
  } else {
    delete macro;
  }
}

void TopologyEditorWidget::doDistribute(int distType) {
  // Collect selected UUT/Device items
  struct Entry {
    QGraphicsItem* item;
    int index;
    bool isProduct;
    QPointF oldPos;
  };
  QVector<Entry> entries;
  for (auto* sel : scene_->selectedItems()) {
    if (auto* uut = qgraphicsitem_cast<UutItem*>(sel))
      entries.append({sel, uut->productIndex(), true, sel->pos()});
    else if (auto* dev = qgraphicsitem_cast<DeviceItem*>(sel))
      entries.append({sel, dev->deviceIndex(), false, sel->pos()});
  }
  if (entries.size() < 2)
    return;

  if (distType == 0) {
    // 横向分布: sort by horizontal center, space evenly
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                return a.item->sceneBoundingRect().center().x() <
                       b.item->sceneBoundingRect().center().x();
              });
    qreal first = entries[0].item->sceneBoundingRect().center().x();
    qreal last = entries[entries.size() - 1].item->sceneBoundingRect().center().x();
    qreal step = (last - first) / (entries.size() - 1);

    for (int i = 0; i < entries.size(); ++i) {
      qreal target = first + step * i;
      qreal cur = entries[i].item->sceneBoundingRect().center().x();
      if (!qFuzzyCompare(target, cur))
        entries[i].item->moveBy(target - cur, 0);
    }
  } else {
    // 纵向分布: sort by vertical center, space evenly
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                return a.item->sceneBoundingRect().center().y() <
                       b.item->sceneBoundingRect().center().y();
              });
    qreal first = entries[0].item->sceneBoundingRect().center().y();
    qreal last = entries[entries.size() - 1].item->sceneBoundingRect().center().y();
    qreal step = (last - first) / (entries.size() - 1);

    for (int i = 0; i < entries.size(); ++i) {
      qreal target = first + step * i;
      qreal cur = entries[i].item->sceneBoundingRect().center().y();
      if (!qFuzzyCompare(target, cur))
        entries[i].item->moveBy(0, target - cur);
    }
  }

  scene_->onItemMoved();

  // Create macro command
  auto* macro = new QUndoCommand;
  macro->setText(distType == 0 ? QStringLiteral("横向分布")
                               : QStringLiteral("纵向分布"));

  for (const auto& e : entries) {
    QPointF newPos = e.item->pos();
    if (newPos != e.oldPos) {
      if (e.isProduct)
        new MoveProductCommand(doc_, e.index, e.oldPos, newPos, macro);
      else
        new MoveDeviceCommand(doc_, e.index, e.oldPos, newPos, macro);
    }
  }

  if (macro->childCount() > 0) {
    doc_->undoStack()->push(macro);
    status_label_->setText(distType == 0 ? QStringLiteral("横向分布")
                                         : QStringLiteral("纵向分布"));
  } else {
    delete macro;
  }
}

}  // namespace etest::topology
