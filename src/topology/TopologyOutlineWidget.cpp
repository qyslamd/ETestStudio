#include "TopologyOutlineWidget.h"
#include "TopologyDocument.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace etest::topology {

TopologyOutlineWidget::TopologyOutlineWidget(QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto* title = new QLabel(QStringLiteral("导航大纲"), this);
  title->setObjectName(QStringLiteral("topologySectionHeader"));
  layout->addWidget(title);

  filter_input_ = new QLineEdit(this);
  filter_input_->setPlaceholderText(QStringLiteral("搜索..."));
  filter_input_->setClearButtonEnabled(true);
  layout->addWidget(filter_input_);

  tree_ = new QTreeWidget(this);
  tree_->setHeaderHidden(true);
  tree_->setRootIsDecorated(true);
  tree_->setAnimated(true);
  tree_->setIndentation(16);
  tree_->header()->setStretchLastSection(true);
  tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(tree_);

  connect(filter_input_, &QLineEdit::textChanged, this,
          &TopologyOutlineWidget::onFilterTextChanged);
  connect(tree_, &QTreeWidget::itemClicked, this,
          &TopologyOutlineWidget::onTreeItemClicked);
}

void TopologyOutlineWidget::rebuildTree(TopologyDocument* doc) {
  tree_->clear();
  if (!doc)
    return;

  updating_selection_ = true;

  auto* uutCat = addCategoryItem(
      QStringLiteral("UUTs (%1)").arg(doc->productCount()));
  for (int i = 0; i < doc->productCount(); ++i)
    addUutItem(i, doc, uutCat);

  auto* devCat = addCategoryItem(
      QStringLiteral("Devices (%1)").arg(doc->deviceCount()));
  for (int i = 0; i < doc->deviceCount(); ++i)
    addDeviceItem(i, doc, devCat);

  auto* connCat = addCategoryItem(
      QStringLiteral("Connections (%1)").arg(doc->connectionCount()));
  for (int i = 0; i < doc->connectionCount(); ++i)
    addConnectionItem(i, doc, connCat);

  tree_->expandAll();
  updating_selection_ = false;
}

QTreeWidgetItem* TopologyOutlineWidget::addCategoryItem(
    const QString& label) {
  auto* item = new QTreeWidgetItem(tree_);
  item->setText(0, label);
  item->setData(0, kRoleTag, static_cast<int>(ItemTag::Category));
  QFont f = item->font(0);
  f.setBold(true);
  item->setFont(0, f);
  item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
  return item;
}

void TopologyOutlineWidget::addUutItem(int index, TopologyDocument* doc,
                                        QTreeWidgetItem* parent) {
  const auto* prod = doc->product(index);
  if (!prod)
    return;

  auto* item = new QTreeWidgetItem(parent);
  item->setText(0, prod->name);
  item->setData(0, kRoleTag, static_cast<int>(ItemTag::Uut));
  item->setData(0, kRoleMainIdx, index);
  item->setData(0, kRoleSubIdx, -1);

  for (int pi = 0; pi < prod->ports.size(); ++pi) {
    auto* portItem = new QTreeWidgetItem(item);
    portItem->setText(0, prod->ports[pi].name);
    portItem->setData(0, kRoleTag, static_cast<int>(ItemTag::Port));
    portItem->setData(0, kRoleMainIdx, index);
    portItem->setData(0, kRoleSubIdx, pi);
  }
}

void TopologyOutlineWidget::addDeviceItem(int index, TopologyDocument* doc,
                                           QTreeWidgetItem* parent) {
  const auto* dev = doc->device(index);
  if (!dev)
    return;

  auto* item = new QTreeWidgetItem(parent);
  item->setText(0, dev->name);
  item->setData(0, kRoleTag, static_cast<int>(ItemTag::Device));
  item->setData(0, kRoleMainIdx, index);
  item->setData(0, kRoleSubIdx, -1);

  for (int pi = 0; pi < dev->ports.size(); ++pi) {
    auto* portItem = new QTreeWidgetItem(item);
    portItem->setText(0, dev->ports[pi].name);
    portItem->setData(0, kRoleTag, static_cast<int>(ItemTag::DevicePort));
    portItem->setData(0, kRoleMainIdx, index);
    portItem->setData(0, kRoleSubIdx, pi);
  }
}

void TopologyOutlineWidget::addConnectionItem(int index,
                                               TopologyDocument* doc,
                                               QTreeWidgetItem* parent) {
  const auto* conn = doc->connection(index);
  if (!conn)
    return;

  auto* item = new QTreeWidgetItem(parent);
  item->setText(0, QStringLiteral("%1:%2 -> %3:%4")
                       .arg(conn->productName, conn->portName,
                            conn->deviceName, conn->devicePort));
  item->setData(0, kRoleTag, static_cast<int>(ItemTag::Connection));
  item->setData(0, kRoleMainIdx, index);
  item->setData(0, kRoleSubIdx, -1);
}

void TopologyOutlineWidget::onFilterTextChanged(const QString& text) {
  for (int i = 0; i < tree_->topLevelItemCount(); ++i)
    applyFilter(tree_->topLevelItem(i), text);
}

bool TopologyOutlineWidget::applyFilter(QTreeWidgetItem* item,
                                         const QString& filter) {
  if (filter.isEmpty()) {
    item->setHidden(false);
    for (int i = 0; i < item->childCount(); ++i)
      item->child(i)->setHidden(false);
    // keep category items expanded
    auto tag = static_cast<ItemTag>(item->data(0, kRoleTag).toInt());
    if (tag == ItemTag::Category)
      item->setExpanded(true);
    return true;
  }

  bool selfMatch =
      item->text(0).contains(filter, Qt::CaseInsensitive);
  bool childMatch = false;
  for (int i = 0; i < item->childCount(); ++i) {
    if (applyFilter(item->child(i), filter))
      childMatch = true;
  }

  bool visible = selfMatch || childMatch;
  item->setHidden(!visible);

  if (visible && childMatch)
    item->setExpanded(true);

  return visible;
}

void TopologyOutlineWidget::onTreeItemClicked(QTreeWidgetItem* item,
                                               int /*column*/) {
  if (updating_selection_)
    return;

  auto tag = static_cast<ItemTag>(item->data(0, kRoleTag).toInt());
  if (tag == ItemTag::Category)
    return;

  int mainIdx = item->data(0, kRoleMainIdx).toInt();
  int subIdx = item->data(0, kRoleSubIdx).toInt();
  emit navigateRequested(static_cast<int>(tag), mainIdx, subIdx);
}

void TopologyOutlineWidget::selectForItem(int itemType, int mainIndex,
                                           int subIndex) {
  updating_selection_ = true;

  tree_->clearSelection();

  auto targetTag = static_cast<ItemTag>(itemType);
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* cat = tree_->topLevelItem(i);
    if (static_cast<ItemTag>(cat->data(0, kRoleTag).toInt()) !=
        ItemTag::Category)
      continue;

    for (int j = 0; j < cat->childCount(); ++j) {
      auto* child = cat->child(j);
      if (static_cast<ItemTag>(child->data(0, kRoleTag).toInt()) !=
          targetTag)
        continue;
      if (child->data(0, kRoleMainIdx).toInt() != mainIndex)
        continue;

      if (subIndex < 0) {
        child->setSelected(true);
        tree_->scrollToItem(child);
        updating_selection_ = false;
        return;
      }

      // For ports, find the specific port child
      for (int k = 0; k < child->childCount(); ++k) {
        auto* portChild = child->child(k);
        if (portChild->data(0, kRoleSubIdx).toInt() == subIndex) {
          portChild->setSelected(true);
          tree_->scrollToItem(portChild);
          updating_selection_ = false;
          return;
        }
      }
    }
  }

  updating_selection_ = false;
}

void TopologyOutlineWidget::clearSelection() {
  updating_selection_ = true;
  tree_->clearSelection();
  updating_selection_ = false;
}

}  // namespace etest::topology
