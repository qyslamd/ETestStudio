#include "IcdNodeTreeWidget.h"

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <icd/repository.hpp>

#include <cstdint>

namespace etest::protocal {
namespace {

// ---------------------------------------------------------------------------
// Pointer storage helpers for QVariant
// ---------------------------------------------------------------------------
template <typename T>
static QVariant ptrToVariant(const T* ptr) {
    return QVariant::fromValue(reinterpret_cast<quintptr>(ptr));
}

template <typename T>
static const T* variantToPtr(const QVariant& v) {
    return reinterpret_cast<const T*>(
        static_cast<uintptr_t>(v.value<quintptr>()));
}

// ---------------------------------------------------------------------------
// ValueType to display string
// ---------------------------------------------------------------------------
static std::string valueTypeToString(icd::ValueType vt) {
    switch (vt) {
    case icd::ValueType::boolean:
        return "bool";
    case icd::ValueType::byte_:
        return "uint8";
    case icd::ValueType::bytes:
        return "bytes";
    case icd::ValueType::word:
        return "uint16";
    case icd::ValueType::shortint:
        return "int16";
    case icd::ValueType::smallint:
        return "int16";
    case icd::ValueType::longword:
        return "uint32";
    case icd::ValueType::integer:
        return "int32";
    case icd::ValueType::ulong_:
        return "uint64";
    case icd::ValueType::single:
        return "float";
    case icd::ValueType::double_:
        return "double";
    case icd::ValueType::string_:
        return "string";
    case icd::ValueType::unknown:
        return "unknown";
    }
    return "unknown";
}

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
enum { PtrRole = Qt::UserRole + 1 };

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

    auto* header = new QLabel(QStringLiteral("信号列表"), this);
    header->setObjectName(QStringLiteral("sectionHeader"));

    tree_view_ = new QTreeView(this);
    tree_view_->setHeaderHidden(true);
    tree_view_->setAnimated(true);
    tree_view_->setIndentation(16);
    tree_view_->setExpandsOnDoubleClick(true);
    tree_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    model_ = new QStandardItemModel(this);

    layout->addWidget(header);
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
// Load frames and nodes from an icd::Repository
// ---------------------------------------------------------------------------
void IcdNodeTreeWidget::loadFromRepository(const icd::Repository& repo) {
    model_->clear();

    for (const auto& frame_ptr : repo.frames()) {
        model_->appendRow(createFrameItem(*frame_ptr));
    }

    tree_view_->setModel(model_);
    tree_view_->expandAll();

    // Disconnect previous selection connection before reconnecting
    if (selection_conn_) {
        disconnect(selection_conn_);
    }

    // Connect selection model — valid only after setModel()
    selection_conn_ = connect(
        tree_view_->selectionModel(),
        &QItemSelectionModel::currentChanged,
        this,
        [this](const QModelIndex& current, const QModelIndex& /*previous*/) {
            if (!current.isValid())
                return;
            auto* item = model_->itemFromIndex(current);
            if (!item)
                return;

            if (!item->parent()) {
                // Top-level item → frame
                if (auto* frame =
                        variantToPtr<icd::Frame>(item->data(PtrRole))) {
                    emit frameSelected(frame);
                }
            } else {
                // Child item → node
                if (auto* node =
                        variantToPtr<icd::Node>(item->data(PtrRole))) {
                    emit nodeSelected(node);
                }
            }
        });
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

    // Root nodes
    for (const auto& root_ptr : frame.roots()) {
        item->appendRow(createNodeItem(*root_ptr));
    }

    return item;
}

// ---------------------------------------------------------------------------
// Create a tree item for an icd::Node (recursive)
// ---------------------------------------------------------------------------
QStandardItem* IcdNodeTreeWidget::createNodeItem(const icd::Node& node) {
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
        item->appendRow(createNodeItem(*child_ptr));
    }

    return item;
}

}  // namespace etest::protocal
