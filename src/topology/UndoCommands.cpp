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

// ═══════════════════════════════════════════════════════════════
//  ResizeItemCommand
// ═══════════════════════════════════════════════════════════════

ResizeItemCommand::ResizeItemCommand(TopologyDocument* doc, int index,
                                     Type type, const QSizeF& oldSize,
                                     const QSizeF& newSize,
                                     const QPointF& oldPos,
                                     const QPointF& newPos,
                                     QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(index), type_(type),
      old_size_(oldSize), new_size_(newSize), old_pos_(oldPos),
      new_pos_(newPos) {
  setText(type_ == Product ? QStringLiteral("调整 UUT 大小")
           : type_ == Monitor ? QStringLiteral("调整监听器大小")
                              : QStringLiteral("调整设备大小"));
}

void ResizeItemCommand::undo() {
  if (type_ == Product) {
    auto* prod = doc_->product(index_);
    if (prod) {
      prod->size = old_size_;
      prod->position = old_pos_;
    }
  } else if (type_ == Monitor) {
    auto* mon = doc_->monitor(index_);
    if (mon) {
      mon->size = old_size_;
      mon->position = old_pos_;
    }
  } else {
    auto* dev = doc_->device(index_);
    if (dev) {
      dev->size = old_size_;
      dev->position = old_pos_;
    }
  }
}

void ResizeItemCommand::redo() {
  if (type_ == Product) {
    auto* prod = doc_->product(index_);
    if (prod) {
      prod->size = new_size_;
      prod->position = new_pos_;
    }
  } else if (type_ == Monitor) {
    auto* mon = doc_->monitor(index_);
    if (mon) {
      mon->size = new_size_;
      mon->position = new_pos_;
    }
  } else {
    auto* dev = doc_->device(index_);
    if (dev) {
      dev->size = new_size_;
      dev->position = new_pos_;
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  AddMonitorCommand
// ═══════════════════════════════════════════════════════════════

AddMonitorCommand::AddMonitorCommand(TopologyDocument* doc,
                                     const TopologyMonitor& monitor,
                                     QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), monitor_(monitor) {
  setText(QStringLiteral("添加监听器"));
}

void AddMonitorCommand::undo() {
  if (index_ >= 0) {
    doc_->removeMonitor(index_);
  }
}

void AddMonitorCommand::redo() {
  index_ = doc_->addMonitor(monitor_);
}

// ═══════════════════════════════════════════════════════════════
//  RemoveMonitorCommand
// ═══════════════════════════════════════════════════════════════

RemoveMonitorCommand::RemoveMonitorCommand(TopologyDocument* doc,
                                           int monitorIndex,
                                           QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(monitorIndex) {
  const auto* mon = doc_->monitor(monitorIndex);
  if (mon) {
    monitor_ = *mon;
  }
  setText(QStringLiteral("删除监听器"));
}

void RemoveMonitorCommand::undo() {
  doc_->addMonitor(monitor_);
}

void RemoveMonitorCommand::redo() {
  doc_->removeMonitor(index_);
}

// ═══════════════════════════════════════════════════════════════
//  RemoveDevicePortCommand
// ═══════════════════════════════════════════════════════════════

RemoveDevicePortCommand::RemoveDevicePortCommand(TopologyDocument* doc,
                                                 int deviceIndex, int portIndex,
                                                 QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), device_index_(deviceIndex),
      port_index_(portIndex) {
  const auto* dev = doc_->device(deviceIndex);
  if (dev && portIndex >= 0 && portIndex < dev->ports.size()) {
    port_ = dev->ports[portIndex];
  }
  setText(QStringLiteral("删除设备端口"));
}

void RemoveDevicePortCommand::undo() {
  doc_->addDevicePort(device_index_, port_);
}

void RemoveDevicePortCommand::redo() {
  doc_->removeDevicePort(device_index_, port_index_);
}

// ═══════════════════════════════════════════════════════════════
//  TapConnectionCommand
// ═══════════════════════════════════════════════════════════════

TapConnectionCommand::TapConnectionCommand(TopologyDocument* doc,
                                           int monitorIndex,
                                           const TopologyMonitorTap& tap,
                                           QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), monitor_index_(monitorIndex),
      tap_(tap) {
  setText(QStringLiteral("挂载连线"));
}

void TapConnectionCommand::undo() {
  const auto* mon = doc_->monitor(monitor_index_);
  if (mon) {
    for (int i = mon->taps.size() - 1; i >= 0; --i) {
      if (mon->taps[i].productName == tap_.productName &&
          mon->taps[i].portName == tap_.portName &&
          mon->taps[i].deviceName == tap_.deviceName &&
          mon->taps[i].devicePort == tap_.devicePort) {
        doc_->removeTap(monitor_index_, i);
        break;
      }
    }
  }
}

void TapConnectionCommand::redo() {
  doc_->addTap(monitor_index_, tap_);
}

// ═══════════════════════════════════════════════════════════════
//  UnTapConnectionCommand
// ═══════════════════════════════════════════════════════════════

UnTapConnectionCommand::UnTapConnectionCommand(TopologyDocument* doc,
                                               int monitorIndex, int tapIndex,
                                               QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), monitor_index_(monitorIndex),
      tap_index_(tapIndex) {
  const auto* mon = doc_->monitor(monitorIndex);
  if (mon && tapIndex >= 0 && tapIndex < mon->taps.size()) {
    tap_ = mon->taps[tapIndex];
  }
  setText(QStringLiteral("解除挂载"));
}

void UnTapConnectionCommand::undo() {
  doc_->addTap(monitor_index_, tap_);
}

void UnTapConnectionCommand::redo() {
  doc_->removeTap(monitor_index_, tap_index_);
}

}  // namespace etest::topology
