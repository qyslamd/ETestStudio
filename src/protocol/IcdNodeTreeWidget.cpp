#include "IcdNodeTreeWidget.h"
#include "IcdProtocolUtils.h"

#include <QAction>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <icd/repository.hpp>

#include <cstdint>
#include <functional>

namespace etest::protocol {
using namespace utils;
namespace {

// ---------------------------------------------------------------------------
// Pointer storage helpers for QVariant
// ---------------------------------------------------------------------------
template <typename T>
static QVariant ptrToVariant(const T* ptr) {
  return QVariant::fromValue(reinterpret_cast<quintptr>(ptr));
}

template <typename T>
static T* variantToPtr(const QVariant& v) {
  return reinterpret_cast<T*>(static_cast<uintptr_t>(v.value<quintptr>()));
}

// ---------------------------------------------------------------------------
// ValueType to display string  (delegated to IcdProtocolUtils.h)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// FrameType to display string
// ---------------------------------------------------------------------------
static std::string frameTypeToString(icd::FrameType ft) {
  switch (ft) {
    case icd::FrameType::data:
      return "data";
    case icd::FrameType::cmd:
      return "cmd";
    case icd::FrameType::data_cmd:
      return "data_cmd";
  }
  return "unknown";
}

// ---------------------------------------------------------------------------
// ByteOrder to display string
// ---------------------------------------------------------------------------
static std::string byteOrderToString(icd::ByteOrder bo) {
  switch (bo) {
    case icd::ByteOrder::little_endian:
      return "little_endian";
    case icd::ByteOrder::big_endian:
      return "big_endian";
  }
  return "unknown";
}

// ---------------------------------------------------------------------------
// Custom data roles
// ---------------------------------------------------------------------------
enum { PtrRole = Qt::UserRole + 1, FrameIdRole = Qt::UserRole + 2 };

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
IcdNodeTreeWidget::IcdNodeTreeWidget(QWidget* parent) : QWidget(parent) {
  initUi();
}

// ---------------------------------------------------------------------------
// Initialise UI
// ---------------------------------------------------------------------------
void IcdNodeTreeWidget::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  filter_input_ = new QLineEdit(this);
  filter_input_->setPlaceholderText(QStringLiteral("搜索信号..."));
  filter_input_->setClearButtonEnabled(true);

  tree_view_ = new QTreeView(this);
  tree_view_->setHeaderHidden(true);
  tree_view_->setAnimated(true);
  tree_view_->setIndentation(16);
  tree_view_->setExpandsOnDoubleClick(true);
  tree_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);

  model_ = new QStandardItemModel(this);
  proxy_ = new QSortFilterProxyModel(this);
  proxy_->setSourceModel(model_);
  proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxy_->setRecursiveFilteringEnabled(true);
  proxy_->setFilterRole(Qt::DisplayRole);
  tree_view_->setModel(proxy_);

  connect(tree_view_, &QTreeView::customContextMenuRequested, this,
          &IcdNodeTreeWidget::onContextMenu);
  connect(filter_input_, &QLineEdit::textChanged, this,
          &IcdNodeTreeWidget::applyFilter);

  layout->addWidget(filter_input_);
  layout->addWidget(tree_view_, 1);
}

// ---------------------------------------------------------------------------
// Clear all items
// ---------------------------------------------------------------------------
void IcdNodeTreeWidget::clear() {
  model_->clear();
}

// ---------------------------------------------------------------------------
// Select a tree item by node pointer
// ---------------------------------------------------------------------------
void IcdNodeTreeWidget::selectNode(const icd::Node* node) {
  if (!node || !model_)
    return;

  std::function<void(QStandardItem*)> search = [&](QStandardItem* item) {
    for (int i = 0; i < item->rowCount(); ++i) {
      auto* child = item->child(i);
      if (auto* stored = variantToPtr<icd::Node>(child->data(PtrRole))) {
        if (stored == node) {
          QModelIndex srcIdx = model_->indexFromItem(child);
          QModelIndex proxyIdx = proxy_->mapFromSource(srcIdx);
          tree_view_->setCurrentIndex(proxyIdx);
          return;
        }
      }
      search(child);
    }
  };

  for (int i = 0; i < model_->rowCount(); ++i) {
    search(model_->item(i));
  }
}

// ---------------------------------------------------------------------------
// Reveal a tree item by node pointer (scroll to, no selection change)
// ---------------------------------------------------------------------------
void IcdNodeTreeWidget::revealNode(const icd::Node* node) {
  if (!node || !model_)
    return;

  std::function<void(QStandardItem*)> search = [&](QStandardItem* item) {
    for (int i = 0; i < item->rowCount(); ++i) {
      auto* child = item->child(i);
      if (auto* stored = variantToPtr<icd::Node>(child->data(PtrRole))) {
        if (stored == node) {
          QModelIndex srcIdx = model_->indexFromItem(child);
          QModelIndex proxyIdx = proxy_->mapFromSource(srcIdx);
          tree_view_->scrollTo(proxyIdx);
          return;
        }
      }
      search(child);
    }
  };

  for (int i = 0; i < model_->rowCount(); ++i) {
    search(model_->item(i));
  }
}

void IcdNodeTreeWidget::applyFilter(const QString& text) {
  proxy_->setFilterFixedString(text);
  if (!text.isEmpty()) {
    tree_view_->expandAll();
  } else {
    tree_view_->collapseAll();
  }
}

// ---------------------------------------------------------------------------
// Load frames and nodes from an icd::Repository
// ---------------------------------------------------------------------------
void IcdNodeTreeWidget::loadFromRepository(const icd::Repository& repo) {
  // Disconnect BEFORE clear to avoid dangling selectionModel
  if (selection_conn_) {
    disconnect(selection_conn_);
    selection_conn_ = {};
  }

  model_->clear();

  for (const auto& frame_ptr : repo.frames()) {
    model_->appendRow(createFrameItem(*frame_ptr));
  }

  tree_view_->expandAll();

  // Connect selection model (valid after setModel via proxy)
  QItemSelectionModel* selModel = tree_view_->selectionModel();
  if (selModel) {
    selection_conn_ = connect(
        selModel, &QItemSelectionModel::currentChanged, this,
        [this](const QModelIndex& current, const QModelIndex& /*previous*/) {
          if (!current.isValid())
            return;
          QModelIndex srcIdx = proxy_->mapToSource(current);
          auto* item = model_->itemFromIndex(srcIdx);
          if (!item)
            return;

          if (!item->parent()) {
            // Top-level item → frame
            if (auto* frame = variantToPtr<icd::Frame>(item->data(PtrRole))) {
              emit frameSelected(frame);
            }
          } else {
            // Child item → node
            if (auto* node = variantToPtr<icd::Node>(item->data(PtrRole))) {
              emit nodeSelected(node);
            }
          }
        });
  }
}

// ---------------------------------------------------------------------------
// Create a tree item for an icd::Frame
// ---------------------------------------------------------------------------
QStandardItem* IcdNodeTreeWidget::createFrameItem(const icd::Frame& frame) {
  auto display =
      QStringLiteral("%1 (ID: %2)")
          .arg(QString::fromUtf8(frame.name().data(),
                                 static_cast<int>(frame.name().size())),
               QString::number(frame.id()));

  auto* item = new QStandardItem(display);
  item->setEditable(false);
  item->setData(ptrToVariant(&frame), PtrRole);

  // Tooltip: id, type, byte order, description
  auto tooltip =
      QStringLiteral("ID: %1\nType: %2\nByteOrder: %3\n%4")
          .arg(QString::number(frame.id()),
               QString::fromStdString(frameTypeToString(frame.type())),
               QString::fromStdString(byteOrderToString(frame.order())),
               QString::fromUtf8(frame.description().data(),
                                 static_cast<int>(frame.description().size())));
  item->setToolTip(tooltip);

  // Store frame ID for pointer validation
  item->setData(frame.id(), FrameIdRole);

  // Root nodes
  for (const auto& root_ptr : frame.roots()) {
    item->appendRow(createNodeItem(*root_ptr, frame.id()));
  }

  return item;
}

// ---------------------------------------------------------------------------
// Create a tree item for an icd::Node (recursive)
// ---------------------------------------------------------------------------
QStandardItem* IcdNodeTreeWidget::createNodeItem(const icd::Node& node,
                                                 int frameId) {
  auto display =
      QStringLiteral("%1 (O:%2, B:%3~%4)")
          .arg(QString::fromUtf8(node.name().data(),
                                 static_cast<int>(node.name().size())),
               QString::number(node.offset()),
               QString::number(node.bit_offset()),
               QString::number(node.bit_offset() + node.bit_width() - 1));

  auto* item = new QStandardItem(display);
  item->setEditable(false);
  item->setData(ptrToVariant(&node), PtrRole);
  item->setData(frameId, FrameIdRole);

  // Tooltip: value type, bit width, description
  auto tooltip =
      QStringLiteral("Type: %1\nBitWidth: %2\n%3")
          .arg(QString::fromStdString(valueTypeToString(node.value_type())),
               QString::number(node.bit_width()),
               QString::fromUtf8(node.description().data(),
                                 static_cast<int>(node.description().size())));
  item->setToolTip(tooltip);

  // Children (recursive)
  for (const auto& child_ptr : node.children()) {
    item->appendRow(createNodeItem(*child_ptr, frameId));
  }

  return item;
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------
void IcdNodeTreeWidget::onContextMenu(const QPoint& pos) {
  QModelIndex index = tree_view_->indexAt(pos);
  QMenu menu;

  if (!index.isValid()) {
    // Clicked on empty space
    auto* action = menu.addAction(QStringLiteral("添加帧"));
    connect(action, &QAction::triggered, this,
            &IcdNodeTreeWidget::addFrameRequested);
  } else {
    auto* item = model_->itemFromIndex(index);
    if (!item)
      return;

    if (!item->parent()) {
      // Frame-level item
      auto* frame = variantToPtr<icd::Frame>(item->data(PtrRole));
      if (!frame)
        return;

      auto* addAction = menu.addAction(QStringLiteral("添加节点"));
      connect(addAction, &QAction::triggered, this,
              [this, frame]() { emit addNodeRequested(frame->id()); });

      menu.addSeparator();

      auto* delAction = menu.addAction(QStringLiteral("删除帧"));
      connect(delAction, &QAction::triggered, this,
              [this, frame]() { emit deleteFrameRequested(frame->id()); });
    } else {
      // Node-level item
      auto* node = variantToPtr<icd::Node>(item->data(PtrRole));
      if (!node)
        return;

      auto* parentItem = item->parent();
      auto* frame = variantToPtr<icd::Frame>(parentItem->data(PtrRole));
      if (!frame)
        return;

      auto* addChildAction = menu.addAction(QStringLiteral("添加子节点"));
      connect(addChildAction, &QAction::triggered, this, [this, frame, node]() {
        emit addNodeRequested(frame->id(), node);
      });

      menu.addSeparator();

      auto* delAction = menu.addAction(QStringLiteral("删除节点"));
      connect(delAction, &QAction::triggered, this, [this, frame, node]() {
        emit deleteNodeRequested(frame->id(), node);
      });
    }
  }

  menu.exec(tree_view_->viewport()->mapToGlobal(pos));
}

}  // namespace etest::protocol
