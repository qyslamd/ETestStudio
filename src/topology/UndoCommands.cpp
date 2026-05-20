#include "UndoCommands.h"
#include "TopologyDocument.h"

namespace etest::topology {

// ═══════════════════════════════════════════════════════════════
//  AddProductCommand
// ═══════════════════════════════════════════════════════════════

AddProductCommand::AddProductCommand(TopologyDocument* doc,
                                     const TopologyProduct& product,
                                     QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), product_(product) {
  setText(QStringLiteral("添加 UUT"));
}

void AddProductCommand::undo() {
  if (index_ >= 0) {
    doc_->removeProduct(index_);
  }
}

void AddProductCommand::redo() {
  index_ = doc_->addProduct(product_);
}

// ═══════════════════════════════════════════════════════════════
//  RemoveProductCommand
// ═══════════════════════════════════════════════════════════════

RemoveProductCommand::RemoveProductCommand(TopologyDocument* doc,
                                           int productIndex,
                                           QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(productIndex) {
  const auto* prod = doc_->product(productIndex);
  if (prod) {
    product_ = *prod;
    for (int i = 0; i < doc_->connectionCount(); ++i) {
      const auto* c = doc_->connection(i);
      if (c->productName == prod->name) {
        saved_connections_.append(
            {c->productName, c->portName, c->deviceName, c->devicePort});
      }
    }
  }
  setText(QStringLiteral("删除 UUT"));
}

void RemoveProductCommand::undo() {
  doc_->addProduct(product_);
  for (const auto& ce : saved_connections_) {
    TopologyConnection conn;
    conn.productName = ce.productName;
    conn.portName = ce.portName;
    conn.deviceName = ce.deviceName;
    conn.devicePort = ce.devicePort;
    doc_->addConnection(conn);
  }
}

void RemoveProductCommand::redo() {
  for (int i = doc_->connectionCount() - 1; i >= 0; --i) {
    const auto* c = doc_->connection(i);
    if (c->productName == product_.name) {
      doc_->removeConnection(i);
    }
  }
  doc_->removeProduct(index_);
}

// ═══════════════════════════════════════════════════════════════
//  AddDeviceCommand
// ═══════════════════════════════════════════════════════════════

AddDeviceCommand::AddDeviceCommand(TopologyDocument* doc,
                                   const TopologyDevice& device,
                                   QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), device_(device) {
  setText(QStringLiteral("添加设备"));
}

void AddDeviceCommand::undo() {
  if (index_ >= 0) {
    doc_->removeDevice(index_);
  }
}

void AddDeviceCommand::redo() {
  index_ = doc_->addDevice(device_);
}

// ═══════════════════════════════════════════════════════════════
//  RemoveDeviceCommand
// ═══════════════════════════════════════════════════════════════

RemoveDeviceCommand::RemoveDeviceCommand(TopologyDocument* doc,
                                         int deviceIndex,
                                         QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(deviceIndex) {
  const auto* dev = doc_->device(deviceIndex);
  if (dev) {
    device_ = *dev;
    for (int i = 0; i < doc_->connectionCount(); ++i) {
      const auto* c = doc_->connection(i);
      if (c->deviceName == dev->name) {
        saved_connections_.append(
            {c->productName, c->portName, c->deviceName, c->devicePort});
      }
    }
  }
  setText(QStringLiteral("删除设备"));
}

void RemoveDeviceCommand::undo() {
  doc_->addDevice(device_);
  for (const auto& ce : saved_connections_) {
    TopologyConnection conn;
    conn.productName = ce.productName;
    conn.portName = ce.portName;
    conn.deviceName = ce.deviceName;
    conn.devicePort = ce.devicePort;
    doc_->addConnection(conn);
  }
}

void RemoveDeviceCommand::redo() {
  for (int i = doc_->connectionCount() - 1; i >= 0; --i) {
    const auto* c = doc_->connection(i);
    if (c->deviceName == device_.name) {
      doc_->removeConnection(i);
    }
  }
  doc_->removeDevice(index_);
}

// ═══════════════════════════════════════════════════════════════
//  AddConnectionCommand
// ═══════════════════════════════════════════════════════════════

AddConnectionCommand::AddConnectionCommand(TopologyDocument* doc,
                                           const TopologyConnection& conn,
                                           QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), conn_(conn) {
  setText(QStringLiteral("添加连线"));
}

void AddConnectionCommand::undo() {
  if (index_ >= 0) {
    doc_->removeConnection(index_);
  }
}

void AddConnectionCommand::redo() {
  index_ = doc_->addConnection(conn_);
}

// ═══════════════════════════════════════════════════════════════
//  RemoveConnectionCommand
// ═══════════════════════════════════════════════════════════════

RemoveConnectionCommand::RemoveConnectionCommand(TopologyDocument* doc,
                                                 int connectionIndex,
                                                 QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(connectionIndex) {
  const auto* c = doc_->connection(connectionIndex);
  if (c) {
    conn_ = *c;
  }
  setText(QStringLiteral("删除连线"));
}

void RemoveConnectionCommand::undo() {
  doc_->addConnection(conn_);
}

void RemoveConnectionCommand::redo() {
  doc_->removeConnection(index_);
}

// ═══════════════════════════════════════════════════════════════
//  MoveProductCommand
// ═══════════════════════════════════════════════════════════════

MoveProductCommand::MoveProductCommand(TopologyDocument* doc, int productIndex,
                                       const QPointF& oldPos,
                                       const QPointF& newPos,
                                       QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(productIndex), old_pos_(oldPos),
      new_pos_(newPos) {
  setText(QStringLiteral("移动 UUT"));
}

void MoveProductCommand::undo() {
  auto* prod = doc_->product(index_);
  if (prod) {
    prod->position = old_pos_;
  }
}

void MoveProductCommand::redo() {
  auto* prod = doc_->product(index_);
  if (prod) {
    prod->position = new_pos_;
  }
}

// ═══════════════════════════════════════════════════════════════
//  MoveDeviceCommand
// ═══════════════════════════════════════════════════════════════

MoveDeviceCommand::MoveDeviceCommand(TopologyDocument* doc, int deviceIndex,
                                     const QPointF& oldPos,
                                     const QPointF& newPos,
                                     QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(deviceIndex), old_pos_(oldPos),
      new_pos_(newPos) {
  setText(QStringLiteral("移动设备"));
}

void MoveDeviceCommand::undo() {
  auto* dev = doc_->device(index_);
  if (dev) {
    dev->position = old_pos_;
  }
}

void MoveDeviceCommand::redo() {
  auto* dev = doc_->device(index_);
  if (dev) {
    dev->position = new_pos_;
  }
}

// ═══════════════════════════════════════════════════════════════
//  PropertyCommand
// ═══════════════════════════════════════════════════════════════

PropertyCommand::PropertyCommand(TopologyDocument* doc, ApplyFn undoFn,
                                 ApplyFn redoFn, const QString& text,
                                 QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), undo_fn_(std::move(undoFn)),
      redo_fn_(std::move(redoFn)) {
  setText(text);
}

void PropertyCommand::undo() {
  if (undo_fn_)
    undo_fn_();
}

void PropertyCommand::redo() {
  if (redo_fn_)
    redo_fn_();
}

}  // namespace etest::topology
