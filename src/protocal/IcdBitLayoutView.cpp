#include "IcdBitLayoutView.h"

#include <QComboBox>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QScrollBar>
#include <QVBoxLayout>

#include <functional>
#include <icd/node.hpp>

#include "core/common/ThemeState.h"

namespace {

// ============================================================
// Group → Color mapping (semantic, not hash-based)
// ============================================================
struct GroupColor {
    QColor dark;
    QColor light;
};

static const QMap<QString, GroupColor> kGroupColors = {
    {QStringLiteral("header"),   {QColor( 74, 144, 217), QColor( 91, 160, 233)}},
    {QStringLiteral("payload"),  {QColor( 92, 184,  92), QColor(108, 200, 108)}},
    {QStringLiteral("checksum"), {QColor(217,  83,  79), QColor(233,  99,  95)}},
    {QStringLiteral("length"),   {QColor(240, 173,  78), QColor(255, 189,  94)}},
    {QStringLiteral("count"),    {QColor(142,  68, 173), QColor(158,  84, 189)}},
    {QStringLiteral("address"),  {QColor( 91, 192, 222), QColor(107, 208, 238)}},
};

static const GroupColor kDefaultGroupColor = {
    QColor(127, 140, 141), QColor(149, 165, 166)
};

// Map icd::Tag to a group name so tagged fields get meaningful colors
// even when group_name is not explicitly set.
static QString tagToGroupName(icd::Tag tag) {
    switch (tag) {
    case icd::Tag::head:    return QStringLiteral("header");
    case icd::Tag::length:  return QStringLiteral("length");
    case icd::Tag::count:   return QStringLiteral("count");
    case icd::Tag::sum:
    case icd::Tag::sum2:
    case icd::Tag::xor_:
    case icd::Tag::xor1:
    case icd::Tag::xor2:    return QStringLiteral("checksum");
    default:                return {};
    }
}

static QColor resolveGroupColor(const icd::Node& node, bool dark) {
    // 1. Explicit group_name takes priority
    QString group = QString::fromStdString(node.attrs().group_name);
    if (group.isEmpty()) {
        // 2. Fallback to tag-based grouping
        group = tagToGroupName(node.tag());
    }
    if (group.isEmpty()) {
        // 3. No group, no tag → gray fallback
        return dark ? kDefaultGroupColor.dark : kDefaultGroupColor.light;
    }
    auto it = kGroupColors.find(group);
    if (it != kGroupColors.end()) {
        return dark ? it->dark : it->light;
    }
    return dark ? kDefaultGroupColor.dark : kDefaultGroupColor.light;
}

}  // anonymous namespace

// ============================================================
// BitBlockItem
// ============================================================

namespace etest::protocal {

// ============================================================
// BitBlockItem
// ============================================================
BitBlockItem::BitBlockItem(const QString& name, int byte_offset,
                           int start_bit, int bit_width, const QColor& color,
                           int cell_size, QGraphicsItem* parent)
    : QGraphicsObject(parent),
      name_(name),
      byte_offset_(byte_offset),
      start_bit_(start_bit),
      bit_width_(bit_width),
      color_(color),
      cell_size_(cell_size) {
  setAcceptHoverEvents(true);
  setCursor(Qt::PointingHandCursor);
}

QRectF BitBlockItem::boundingRect() const {
  return QRectF(0, 0, bit_width_ * cell_size_, cell_size_);
}

void BitBlockItem::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem*, QWidget*) {
  QRectF r = boundingRect();

  QColor fill = color_;
  if (hovered_ || highlighted_) {
    fill = QColor(fill.red(), fill.green(), fill.blue(), 220);
  }
  painter->fillRect(r, fill);

  if (highlighted_) {
    painter->setPen(QPen(Qt::white, 3));
  } else if (hovered_) {
    painter->setPen(QPen(QColor(255, 255, 255, 200), 2));
  } else {
    painter->setPen(QPen(fill.darker(130), 1));
  }
  painter->drawRect(r);

  if (r.width() > 48) {
    painter->setPen(Qt::white);
    QFont f = painter->font();
    f.setPointSize(9);
    painter->setFont(f);
    painter->drawText(r.adjusted(4, 0, -4, 0),
                      Qt::AlignVCenter | Qt::AlignLeft, name_);
  }
}

void BitBlockItem::setHighlighted(bool on) {
  highlighted_ = on;
  update();
}

void BitBlockItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  if (!hovered_) {
    hovered_ = true;
    emit hovered(name_, true);
  }
  update();
}

void BitBlockItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  if (hovered_) {
    hovered_ = false;
    emit hovered(name_, false);
  }
  update();
}

void BitBlockItem::mousePressEvent(QGraphicsSceneMouseEvent*) {
  emit clicked(name_);
}

void BitBlockItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
  QMenu menu;
  QAction* act_edit = menu.addAction(QStringLiteral("编辑"));
  menu.addSeparator();
  QAction* act_add_before = menu.addAction(QStringLiteral("在前面插入"));
  QAction* act_add_after = menu.addAction(QStringLiteral("在后面插入"));
  menu.addSeparator();
  QAction* act_delete = menu.addAction(QStringLiteral("删除"));

  QAction* chosen = menu.exec(event->screenPos());
  if (chosen == act_edit) {
    emit clicked(name_);
  } else if (chosen == act_add_before) {
    emit contextMenuAction(name_, QStringLiteral("addBefore"));
  } else if (chosen == act_add_after) {
    emit contextMenuAction(name_, QStringLiteral("addAfter"));
  } else if (chosen == act_delete) {
    emit contextMenuAction(name_, QStringLiteral("delete"));
  }
}

// ============================================================
// IcdBitLayoutScene
// ============================================================
IcdBitLayoutScene::IcdBitLayoutScene(QObject* parent)
    : QGraphicsScene(parent) {
  setBackgroundBrush(core::common::isDarkTheme() ? QColor(30, 30, 30)
                                                  : QColor(248, 248, 248));
}

void IcdBitLayoutScene::setFrame(int length_bytes, int bits_per_row) {
  frame_length_ = length_bytes;
  bits_per_row_ = bits_per_row;

  int rows = (frame_length_ * 8 + bits_per_row_ - 1) / bits_per_row_;
  int total_w = margin_left_ + bits_per_row_ * cell_size_ + 10;
  int total_h = margin_top_ + rows * cell_size_ + 10;

  clear();
  setSceneRect(0, 0, total_w, total_h);
}

BitBlockItem* IcdBitLayoutScene::addBlock(const QString& name,
                                           int byte_offset, int start_bit,
                                           int bit_width,
                                           const QColor& color) {
  int global_start = byte_offset * 8 + start_bit;
  int remaining = bit_width;
  int current_pos = global_start;

  BitBlockItem* first_item = nullptr;

  while (remaining > 0) {
    int row = current_pos / bits_per_row_;
    int col = current_pos % bits_per_row_;
    int bits_in_row = std::min(remaining, bits_per_row_ - col);

    int x = margin_left_ + col * cell_size_;
    int y = margin_top_ + row * cell_size_;

    auto* item =
        new BitBlockItem(name, byte_offset, start_bit, bits_in_row, color,
                         cell_size_);
    item->setPos(x, y);
    addItem(item);
    connect(item, &BitBlockItem::clicked, this, &IcdBitLayoutScene::blockClicked);
    connect(item, &BitBlockItem::contextMenuAction,
            this, &IcdBitLayoutScene::contextMenuAction);
    connect(item, &BitBlockItem::hovered,
            this, &IcdBitLayoutScene::onBlockHovered);

    if (!first_item) first_item = item;

    current_pos += bits_in_row;
    remaining -= bits_in_row;
  }

  return first_item;
}

void IcdBitLayoutScene::clearBlocks() {
  for (auto* item : items()) {
    if (auto* block = dynamic_cast<BitBlockItem*>(item)) {
      removeItem(block);
      delete block;
    }
  }
}

void IcdBitLayoutScene::highlightBlock(const QString& name) {
  selected_name_ = name;
  for (auto* item : items()) {
    if (auto* block = dynamic_cast<BitBlockItem*>(item)) {
      block->setHighlighted(block->name() == name);
    }
  }
}

void IcdBitLayoutScene::onBlockHovered(const QString& name, bool on) {
  if (on) {
    // Hover enter: highlight all blocks with this name
    for (auto* item : items()) {
      if (auto* block = dynamic_cast<BitBlockItem*>(item)) {
        block->setHighlighted(block->name() == name);
      }
    }
  } else {
    // Hover leave: restore selection highlight
    for (auto* item : items()) {
      if (auto* block = dynamic_cast<BitBlockItem*>(item)) {
        block->setHighlighted(block->name() == selected_name_);
      }
    }
  }
  emit blockHovered(name, on);
}

void IcdBitLayoutScene::drawBackground(QPainter* painter, const QRectF&) {
  bool dark = core::common::isDarkTheme();

  painter->fillRect(sceneRect(), dark ? QColor(30, 30, 30)
                                      : QColor(245, 245, 245));

  int rows = (frame_length_ * 8 + bits_per_row_ - 1) / bits_per_row_;
  int cols = bits_per_row_;

  // Grid
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      int x = margin_left_ + c * cell_size_;
      int y = margin_top_ + r * cell_size_;

      QColor cell_color;
      if (dark) {
        cell_color = ((r * cols + c) % 2 == 0) ? QColor(40, 40, 40)
                                                : QColor(45, 45, 45);
      } else {
        cell_color = ((r * cols + c) % 2 == 0) ? QColor(232, 232, 232)
                                                : QColor(238, 238, 238);
      }
      painter->fillRect(x, y, cell_size_, cell_size_, cell_color);
      painter->setPen(QPen(dark ? QColor(55, 55, 55) : QColor(210, 210, 210), 1));
      painter->drawRect(x, y, cell_size_, cell_size_);
    }
  }

  // 4-byte separator lines
  int bytes_per_row = bits_per_row_ / 8;
  painter->setPen(QPen(dark ? QColor(90, 90, 90) : QColor(190, 190, 190), 1));
  for (int r = 0; r < rows; ++r) {
    for (int b = 1; b < bytes_per_row; ++b) {
      int x = margin_left_ + b * 8 * cell_size_;
      int y = margin_top_ + r * cell_size_;
      painter->drawLine(x, y, x, y + cell_size_);
    }
  }

  // Top bit ruler
  painter->setPen(dark ? QColor(180, 180, 180) : QColor(120, 120, 120));
  QFont small_font;
  small_font.setPointSize(7);
  painter->setFont(small_font);
  for (int c = 0; c < cols; ++c) {
    if (c % 4 == 0) {
      painter->drawText(margin_left_ + c * cell_size_ + 2,
                        margin_top_ - 8, QString::number(c));
    }
  }

  // Left byte offset ruler
  for (int r = 0; r < rows; ++r) {
    int byte_start = r * (bits_per_row_ / 8);
    painter->drawText(4, margin_top_ + r * cell_size_ + cell_size_ - 4,
                      QString::number(byte_start) + "B");
  }
}

// ============================================================
// IcdBitLayoutView
// ============================================================
IcdBitLayoutView::IcdBitLayoutView(QWidget* parent) : QWidget(parent) {
  initUi();
  setFrameData(4, 32);
}

void IcdBitLayoutView::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Toolbar
  auto* toolbar = new QWidget(this);
  toolbar->setFixedHeight(32);
  auto* tb_layout = new QHBoxLayout(toolbar);
  tb_layout->setContentsMargins(8, 0, 8, 0);

  mode_combo_ = new QComboBox(this);
  mode_combo_->addItem(QStringLiteral("32-bit 字模式"));
  mode_combo_->addItem(QStringLiteral("字节模式 (8-bit)"));
  mode_combo_->setEnabled(false);

  tb_layout->addWidget(new QLabel(QStringLiteral("显示模式:"), this));
  tb_layout->addWidget(mode_combo_);
  tb_layout->addStretch();
  tb_layout->addWidget(
      new QLabel(QStringLiteral("帧长度: 16 bytes  |  滚轮缩放 · 中键平移"),
                 this));

  layout->addWidget(toolbar);

  // QGraphicsView
  view_ = new QGraphicsView(this);
  view_->setRenderHint(QPainter::Antialiasing);
  view_->setRenderHint(QPainter::SmoothPixmapTransform);
  view_->setDragMode(QGraphicsView::ScrollHandDrag);
  view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  view_->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
  view_->setBackgroundBrush(core::common::isDarkTheme() ? QColor(30, 30, 30)
                                                        : QColor(248, 248, 248));
  view_->setFrameShape(QFrame::NoFrame);
  view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scene_ = new IcdBitLayoutScene(this);
  view_->setScene(scene_);
  view_->installEventFilter(this);
  connect(scene_, &IcdBitLayoutScene::blockClicked, this,
          &IcdBitLayoutView::blockClicked);
  connect(scene_, &IcdBitLayoutScene::contextMenuAction,
          this, &IcdBitLayoutView::contextMenuAction);
  connect(scene_, &IcdBitLayoutScene::blockHovered,
          this, &IcdBitLayoutView::blockHovered);

  layout->addWidget(view_, 1);
}

void IcdBitLayoutView::loadFromFrame(const icd::Frame& frame) {
    clearBlocks();

    // Collect all leaf nodes from the frame's node tree
    QVector<const icd::Node*> leaves;
    for (const auto& root : frame.roots()) {
        collectLeafNodes(*root, leaves);
    }

    if (leaves.isEmpty()) return;

    // Calculate frame length from max extent
    int max_bits = 0;
    for (auto* node : leaves) {
        int end = (node->offset() * 8) + node->bit_offset() + node->bit_width();
        if (end > max_bits) max_bits = end;
    }
    int frame_length = (max_bits + 7) / 8;
    if (frame_length < 1) frame_length = 1;

    setFrameData(frame_length, 32);

    for (auto* node : leaves) {
        QColor color = resolveGroupColor(*node, core::common::isDarkTheme());
        QString qname = QString::fromStdString(std::string(node->name()));
        addBlock(qname, node->offset(), node->bit_offset(), node->bit_width(), color);
    }

    view_->scale(0.75, 0.75);
    view_->centerOn(0, 0);
}

void IcdBitLayoutView::collectLeafNodes(const icd::Node& node, QVector<const icd::Node*>& leaves) {
    if (node.children().empty()) {
        leaves.push_back(&node);
    } else {
        for (const auto& child : node.children()) {
            collectLeafNodes(*child, leaves);
        }
    }
}

void IcdBitLayoutView::setFrameData(int length_bytes, int bits_per_row) {
  scene_->setFrame(length_bytes, bits_per_row);
}

BitBlockItem* IcdBitLayoutView::addBlock(const QString& name, int byte_offset,
                                          int start_bit, int bit_width,
                                          const QColor& color) {
  return scene_->addBlock(name, byte_offset, start_bit, bit_width, color);
}

void IcdBitLayoutView::clearBlocks() { scene_->clearBlocks(); }

void IcdBitLayoutView::highlightBlock(const QString& name) {
  scene_->highlightBlock(name);
}

void IcdBitLayoutView::fitToContent() {
  QRectF r = scene_->sceneRect();
  view_->fitInView(r.adjusted(-10, -10, 10, 10), Qt::KeepAspectRatio);
}

bool IcdBitLayoutView::eventFilter(QObject* obj, QEvent* event) {
  if (obj == view_ && event->type() == QEvent::Wheel) {
    auto* wheel = static_cast<QWheelEvent*>(event);
    double factor = wheel->delta() > 0 ? 1.15 : 1.0 / 1.15;
    view_->scale(factor, factor);
    return true;
  }
  return QWidget::eventFilter(obj, event);
}

}  // namespace etest::protocal
