#include "UndoCommands.h"
#include "TopologyDocument.h"

#include <algorithm>

namespace etest::topology {

// ═══════════════════════════════════════════════════════════════
//  SetDevicePortFramesCommand (M3)
// ═══════════════════════════════════════════════════════════════

SetDevicePortFramesCommand::SetDevicePortFramesCommand(
    TopologyDocument* doc, int deviceIndex, int portIndex,
    const QStringList& newFrames, QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), device_index_(deviceIndex),
      port_index_(portIndex), new_frames_(newFrames) {
  old_frames_ = doc_->devicePortFrames(deviceIndex, portIndex);
  setText(QStringLiteral("绑定 ICD 帧"));
}

void SetDevicePortFramesCommand::undo() {
  doc_->setDevicePortFrames(device_index_, port_index_, old_frames_);
}

void SetDevicePortFramesCommand::redo() {
  doc_->setDevicePortFrames(device_index_, port_index_, new_frames_);
}

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
            {c->productName, c->portName, c->deviceName, c->devicePort, c->style});
      }
    }
  }
  setText(QStringLiteral("删除 UUT"));
}

void RemoveProductCommand::undo() {
  doc_->insertProduct(index_, product_);
  for (const auto& ce : saved_connections_) {
    TopologyConnection conn;
    conn.productName = ce.productName;
    conn.portName = ce.portName;
    conn.deviceName = ce.deviceName;
    conn.devicePort = ce.devicePort;
    conn.style = ce.style;
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
            {c->productName, c->portName, c->deviceName, c->devicePort, c->style});
      }
    }
  }
  setText(QStringLiteral("删除设备"));
}

void RemoveDeviceCommand::undo() {
  doc_->insertDevice(index_, device_);
  for (const auto& ce : saved_connections_) {
    TopologyConnection conn;
    conn.productName = ce.productName;
    conn.portName = ce.portName;
    conn.deviceName = ce.deviceName;
    conn.devicePort = ce.devicePort;
    conn.style = ce.style;
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
    // 级联清理：找到所有 connectionId 匹配的 monitor
    for (int mi = 0; mi < doc_->monitorCount(); ++mi) {
      const auto* mon = doc_->monitor(mi);
      if (!mon || mon->connectionId.isEmpty()) continue;
      if (mon->connectionId == c->id) {
        saved_monitors_.append({mi, *mon});
      }
    }
  }
  setText(QStringLiteral("删除连线"));
}

void RemoveConnectionCommand::undo() {
  doc_->insertConnection(index_, conn_);
  for (const auto& sm : saved_monitors_) {
    doc_->insertMonitor(sm.monitorIndex, sm.monitor);
  }
}

void RemoveConnectionCommand::redo() {
  // 从高 index 到低删除，防索引偏移
  std::sort(saved_monitors_.begin(), saved_monitors_.end(),
            [](const SavedMonitor& a, const SavedMonitor& b) {
              return a.monitorIndex > b.monitorIndex;
            });
  for (const auto& sm : saved_monitors_) {
    doc_->removeMonitor(sm.monitorIndex);
  }
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
//  MoveMonitorCommand
// ═══════════════════════════════════════════════════════════════

MoveMonitorCommand::MoveMonitorCommand(TopologyDocument* doc, int monitorIndex,
                                       const QPointF& oldPos,
                                       const QPointF& newPos,
                                       QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), index_(monitorIndex), old_pos_(oldPos),
      new_pos_(newPos) {
  setText(QStringLiteral("移动监听器"));
}

void MoveMonitorCommand::undo() {
  auto* mon = doc_->monitor(index_);
  if (mon) {
    mon->position = old_pos_;
  }
}

void MoveMonitorCommand::redo() {
  auto* mon = doc_->monitor(index_);
  if (mon) {
    mon->position = new_pos_;
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
//  Style commands
// ═══════════════════════════════════════════════════════════════

SetProductPortStyleCommand::SetProductPortStyleCommand(
    TopologyDocument* doc, int productIndex, int portIndex, PortStyle newStyle,
    QUndoCommand* parent)
    : QUndoCommand(parent),
      doc_(doc),
      product_index_(productIndex),
      port_index_(portIndex),
      old_style_(static_cast<int>(PortStyle::Circle)),
      new_style_(static_cast<int>(newStyle)) {
  if (auto* product = doc_->product(product_index_)) {
    if (port_index_ >= 0 && port_index_ < product->ports.size())
      old_style_ = product->ports[port_index_].portStyle;
  }
  setText(QStringLiteral("修改 UUT 端口样式"));
}

void SetProductPortStyleCommand::undo() {
  if (auto* product = doc_->product(product_index_)) {
    if (port_index_ >= 0 && port_index_ < product->ports.size())
      product->ports[port_index_].portStyle = old_style_;
  }
}

void SetProductPortStyleCommand::redo() {
  if (auto* product = doc_->product(product_index_)) {
    if (port_index_ >= 0 && port_index_ < product->ports.size())
      product->ports[port_index_].portStyle = new_style_;
  }
}

SetDevicePortStyleCommand::SetDevicePortStyleCommand(
    TopologyDocument* doc, int deviceIndex, int portIndex, PortStyle newStyle,
    QUndoCommand* parent)
    : QUndoCommand(parent),
      doc_(doc),
      device_index_(deviceIndex),
      port_index_(portIndex),
      old_style_(static_cast<int>(PortStyle::Circle)),
      new_style_(static_cast<int>(newStyle)) {
  if (auto* device = doc_->device(device_index_)) {
    if (port_index_ >= 0 && port_index_ < device->ports.size())
      old_style_ = device->ports[port_index_].portStyle;
  }
  setText(QStringLiteral("修改设备端口样式"));
}

void SetDevicePortStyleCommand::undo() {
  if (auto* device = doc_->device(device_index_)) {
    if (port_index_ >= 0 && port_index_ < device->ports.size())
      device->ports[port_index_].portStyle = old_style_;
  }
}

void SetDevicePortStyleCommand::redo() {
  if (auto* device = doc_->device(device_index_)) {
    if (port_index_ >= 0 && port_index_ < device->ports.size())
      device->ports[port_index_].portStyle = new_style_;
  }
}

SetConnectionStyleCommand::SetConnectionStyleCommand(
    TopologyDocument* doc, int connectionIndex, PathStyle newStyle,
    QUndoCommand* parent)
    : QUndoCommand(parent),
      doc_(doc),
      connection_index_(connectionIndex),
      old_style_(PathStyle::Bezier),
      new_style_(newStyle) {
  if (auto* connection = doc_->connection(connection_index_))
    old_style_ = connection->style;
  setText(QStringLiteral("修改连线样式"));
}

void SetConnectionStyleCommand::undo() {
  if (auto* connection = doc_->connection(connection_index_))
    connection->style = old_style_;
}

void SetConnectionStyleCommand::redo() {
  if (auto* connection = doc_->connection(connection_index_))
    connection->style = new_style_;
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
  doc_->insertMonitor(index_, monitor_);
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
  doc_->insertDevicePort(device_index_, port_index_, port_);
}

void RemoveDevicePortCommand::redo() {
  doc_->removeDevicePort(device_index_, port_index_);
}

// ═══════════════════════════════════════════════════════════════
//  AddProductPortCommand
// ═══════════════════════════════════════════════════════════════

AddProductPortCommand::AddProductPortCommand(TopologyDocument* doc,
                                             int productIndex,
                                             const TopologyPort& port,
                                             QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), product_index_(productIndex),
      port_(port) {
  setText(QStringLiteral("添加 UUT 端口"));
}

void AddProductPortCommand::undo() {
  if (port_index_ >= 0) {
    doc_->removeProductPort(product_index_, port_index_);
  }
}

void AddProductPortCommand::redo() {
  doc_->addProductPort(product_index_, port_);
  // Record the actual index where the port was inserted
  const auto* prod = doc_->product(product_index_);
  if (prod) {
    port_index_ = prod->ports.size() - 1;
  }
}

// ═══════════════════════════════════════════════════════════════
//  RemoveProductPortCommand
// ═══════════════════════════════════════════════════════════════

RemoveProductPortCommand::RemoveProductPortCommand(TopologyDocument* doc,
                                                   int productIndex,
                                                   int portIndex,
                                                   QUndoCommand* parent)
    : QUndoCommand(parent), doc_(doc), product_index_(productIndex),
      port_index_(portIndex) {
  const auto* prod = doc_->product(productIndex);
  if (prod && portIndex >= 0 && portIndex < prod->ports.size()) {
    port_ = prod->ports[portIndex];
    // 级联清理：找到引用此端口的连线
    for (int i = 0; i < doc_->connectionCount(); ++i) {
      const auto* c = doc_->connection(i);
      if (c->productName == prod->name && c->portName == port_.name) {
        saved_connections_.append(
            {c->productName, c->portName, c->deviceName, c->devicePort, c->style});
      }
    }
  }
  setText(QStringLiteral("删除 UUT 端口"));
}

void RemoveProductPortCommand::undo() {
  doc_->insertProductPort(product_index_, port_index_, port_);
  // 恢复被级联清理的连线
  for (const auto& ce : saved_connections_) {
    TopologyConnection conn;
    conn.productName = ce.productName;
    conn.portName = ce.portName;
    conn.deviceName = ce.deviceName;
    conn.devicePort = ce.devicePort;
    conn.style = ce.style;
    doc_->addConnection(conn);
  }
}

void RemoveProductPortCommand::redo() {
  auto* prod = doc_->product(product_index_);
  if (!prod)
    return;

  // 先移除引用此端口的所有连线
  for (int i = doc_->connectionCount() - 1; i >= 0; --i) {
    const auto* c = doc_->connection(i);
    if (c && c->portName == port_.name && c->productName == prod->name) {
      doc_->removeConnection(i);
    }
  }
  doc_->removeProductPort(product_index_, port_index_);
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
  doc_->insertTap(monitor_index_, tap_index_, tap_);
}

void UnTapConnectionCommand::redo() {
  doc_->removeTap(monitor_index_, tap_index_);
}

}  // namespace etest::topology
