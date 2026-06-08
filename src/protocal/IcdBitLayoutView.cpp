#include "IcdBitLayoutView.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsView>
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <cmath>
#include <icd/node.hpp>

#include "ThemeManager.h"

namespace {

// ============================================================
// Group → Color mapping (semantic, not hash-based)
// ============================================================
struct GroupColor {
    QColor dark;
    QColor light;
};

static const QMap<QString, GroupColor> kGroupColors = {
    {QStringLiteral("header"),   {QColor( 50, 130, 240), QColor( 75, 160, 250)}},
    {QStringLiteral("payload"),  {QColor( 60, 205,  75), QColor( 80, 220,  95)}},
    {QStringLiteral("checksum"), {QColor(230,  70,  55), QColor(245,  90,  75)}},
    {QStringLiteral("length"),   {QColor(252, 185,  50), QColor(255, 205,  70)}},
    {QStringLiteral("count"),    {QColor(155,  50, 195), QColor(175,  70, 215)}},
    {QStringLiteral("address"),  {QColor( 55, 200, 240), QColor( 75, 215, 250)}},
};

static const GroupColor kDefaultGroupColor = {
    QColor(127, 140, 141), QColor(149, 165, 166)
};

// Fallback palette for fields without explicit group/tag — cycles round-robin
static const QVector<GroupColor> kPaletteCycle = {
    kGroupColors["header"],
    kGroupColors["payload"],
    kGroupColors["checksum"],
    kGroupColors["length"],
    kGroupColors["count"],
    kGroupColors["address"],
};

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
    QString group = QString::fromStdString(node.attrs().group_name);
    if (group.isEmpty()) {
        group = tagToGroupName(node.tag());
    }
    if (group.isEmpty()) {
        return dark ? kDefaultGroupColor.dark : kDefaultGroupColor.light;
    }
    auto it = kGroupColors.find(group);
    if (it != kGroupColors.end()) {
        return dark ? it->dark : it->light;
    }
    return dark ? kDefaultGroupColor.dark : kDefaultGroupColor.light;
}

// Return a hue-shifted "gradient partner" for multi-hue gradients.
// Each group's main color transitions toward the next color in a rainbow cycle.
static QColor gradientPartner(const QColor& base) {
    int h = base.hue();
    int s = base.saturation();
    int v = base.value();

    if (h >= 200 && h < 260)         // blue → purple
        return QColor::fromHsv(275, qMin(s + 10, 255), qMin(v + 20, 255));
    if (h >= 90 && h < 170)          // green → teal
        return QColor::fromHsv(178, s, qMin(v + 15, 255));
    if (h >= 350 || h < 15)          // red → orange
        return QColor::fromHsv(28, qMin(s + 5, 255), v);
    if (h >= 30 && h < 65)           // yellow → lime
        return QColor::fromHsv(85, qMin(s + 10, 255), qMin(v + 10, 255));
    if (h >= 260 && h < 295)         // purple → pink
        return QColor::fromHsv(325, s, qMin(v + 10, 255));
    if (h >= 170 && h < 200)         // cyan → blue
        return QColor::fromHsv(218, s, qMin(v + 15, 255));
    return base.lighter(130);
}

}  // anonymous namespace

namespace etest::protocal {

// ============================================================
// FieldSectionItem
// ============================================================
FieldSectionItem::FieldSectionItem(const QString& name, int byte_offset,
                                   int start_bit, int bit_width,
                                   const QColor& color, int cell_size,
                                   int bits_per_row, QGraphicsItem* parent)
    : QGraphicsObject(parent),
      name_(name),
      byte_offset_(byte_offset),
      start_bit_(start_bit),
      bit_width_(bit_width),
      color_(color),
      cell_size_(cell_size),
      bits_per_row_(bits_per_row) {
  setAcceptHoverEvents(true);
  setCursor(Qt::PointingHandCursor);
}

int FieldSectionItem::totalHeight() const {
  if (bit_width_ <= 0) return kHeaderHeight;
  int rows = (bit_width_ + bits_per_row_ - 1) / bits_per_row_;
  return kHeaderHeight + rows * cell_size_;
}

int FieldSectionItem::sectionWidth() const {
  if (bit_width_ <= 0) return cell_size_;
  int cols = std::min(bit_width_, bits_per_row_);
  return cols * cell_size_;
}

QRectF FieldSectionItem::boundingRect() const {
  return QRectF(0, 0, sectionWidth(), totalHeight());
}

void FieldSectionItem::setHighlighted(bool on) {
  highlighted_ = on;
  update();
}

void FieldSectionItem::setHovered(bool on) {
  hovered_ = on;
  update();
}

void FieldSectionItem::paint(QPainter* painter,
                              const QStyleOptionGraphicsItem*, QWidget*) {
  bool dark = etest::app::ThemeManager::instance().isDarkTheme();

  int cols = std::min(bit_width_, bits_per_row_);
  int rows = (bit_width_ + bits_per_row_ - 1) / bits_per_row_;
  int sec_w = cols * cell_size_;
  int sec_h = totalHeight();

  // ── Section background ──
  QColor bg = dark ? QColor(32, 32, 35) : QColor(245, 245, 247);
  if (highlighted_) {
    bg = dark ? QColor(38, 38, 42) : QColor(238, 238, 242);
  }
  painter->fillRect(0, 0, sec_w, sec_h, bg);

  // ── Hover overlay (subtle, drawn before header) ──
  if (hovered_ && !highlighted_) {
    QColor ho = Qt::white;
    ho.setAlpha(dark ? 15 : 30);
    painter->fillRect(0, 0, sec_w, sec_h, ho);
  }

  // ── Header bar (vibrant multi-hue gradient) ──
  int accent_w = highlighted_ ? 6 : 4;
  int sec_hdr = kHeaderHeight;

  QColor c1 = color_;                         // main hue
  QColor c2 = gradientPartner(color_);         // shifted hue (teal/purple/orange…)
  QColor c_bright = color_.lighter(180);

  // Left accent bar: vertical multi-hue gradient
  {
    int h = highlighted_ ? sec_h : sec_hdr;
    QLinearGradient g(0, 0, 0, h);
    g.setColorAt(0.0, c_bright);
    g.setColorAt(0.4, c1);
    g.setColorAt(0.8, c2);
    g.setColorAt(1.0, c2.darker(130));
    painter->fillRect(0, 0, accent_w, h, g);
  }

  // Header background: horizontal multi-hue gradient (fully opaque)
  {
    QLinearGradient g(0, 0, sec_w, 0);
    g.setColorAt(0.0, c1);
    g.setColorAt(0.5, c2);
    g.setColorAt(0.85, c2.darker(110));
    g.setColorAt(1.0, c2.darker(130));
    painter->fillRect(0, 0, sec_w, sec_hdr, g);
  }

  // Glossy highlight: thin white gradient at the top of the header
  {
    QLinearGradient g(0, 0, 0, sec_hdr * 0.55);
    g.setColorAt(0.0, QColor(255, 255, 255, 60));
    g.setColorAt(0.5, QColor(255, 255, 255, 20));
    g.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->fillRect(0, 0, sec_w, sec_hdr * 0.55, g);
  }

  // Header bottom gradient separator line
  {
    QLinearGradient g(0, 0, sec_w, 0);
    g.setColorAt(0.0, c1.lighter(120));
    g.setColorAt(0.4, c2);
    g.setColorAt(0.8, c2.darker(110));
    g.setColorAt(1.0, c2.darker(140));
    painter->setPen(QPen(g, highlighted_ ? 2 : 1));
    painter->drawLine(0, sec_hdr, sec_w, sec_hdr);
  }

  // Header text (bright white)
  int global_start = byte_offset_ * 8 + start_bit_;
  int global_end = global_start + bit_width_ - 1;
  QString range_str = (bit_width_ > 1)
      ? QStringLiteral("%1~%2").arg(global_start).arg(global_end)
      : QString::number(global_start);
  QString header_text = QStringLiteral("%1  [%2 bits]")
      .arg(name_).arg(range_str);

  painter->setPen(highlighted_ ? Qt::white : c_bright);
  QFont hf = painter->font();
  hf.setPointSize(10);
  hf.setBold(true);
  painter->setFont(hf);
  painter->drawText(QRectF(14, 0, sec_w - 14, sec_hdr),
                    Qt::AlignVCenter | Qt::AlignLeft, header_text);

  // ── Bit cells ──
  int global_base = byte_offset_ * 8 + start_bit_;

  // Cell area background
  painter->fillRect(0, kHeaderHeight + 1, sec_w, sec_h - kHeaderHeight - 1,
                    dark ? QColor(28, 28, 30) : QColor(250, 250, 250));

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      int local_idx = r * bits_per_row_ + c;
      if (local_idx >= bit_width_) break;

      int cell_x = c * cell_size_;
      int cell_y = kHeaderHeight + r * cell_size_;

      // Cell background
      painter->fillRect(cell_x + 1, cell_y + 1, cell_size_ - 2, cell_size_ - 2,
                        dark ? QColor(38, 38, 40) : QColor(238, 238, 240));

      // Cell border (right + bottom only, spreadsheet style)
      QColor border_col = dark ? QColor(50, 50, 53) : QColor(222, 222, 225);
      painter->setPen(QPen(border_col, 1));
      painter->drawLine(cell_x + cell_size_, cell_y + 1,
                        cell_x + cell_size_, cell_y + cell_size_);
      painter->drawLine(cell_x + 1, cell_y + cell_size_,
                        cell_x + cell_size_, cell_y + cell_size_);

      // Bit index text
      int global_bit = global_base + local_idx;
      painter->setPen(dark ? QColor(160, 160, 165) : QColor(130, 130, 135));
      QFont cell_font = painter->font();
      cell_font.setPointSize(7);
      painter->setFont(cell_font);
      painter->drawText(QRectF(cell_x + 1, cell_y + 1,
                                cell_size_ - 2, cell_size_ - 2),
                        Qt::AlignCenter, QString::number(global_bit));
    }
  }

  // ── Selection indicator (most prominent layer) ──
  if (highlighted_) {
    // Full-height left accent bar: multi-hue gradient
    {
      QLinearGradient g(0, 0, 0, sec_h);
      g.setColorAt(0.0, color_.lighter(190));
      g.setColorAt(0.3, color_);
      g.setColorAt(0.6, c2);
      g.setColorAt(1.0, c2.darker(130));
      painter->fillRect(0, 0, 6, sec_h, g);
    }

    // Outer selection border: matching gradient
    {
      QLinearGradient g(0, 0, 0, sec_h);
      g.setColorAt(0.0, dark ? Qt::white : color_.lighter(140));
      g.setColorAt(0.4, color_);
      g.setColorAt(0.7, c2);
      g.setColorAt(1.0, c2.darker(120));
      painter->setPen(QPen(g, 2));
      painter->drawRect(1, 1, sec_w - 2, sec_h - 2);
    }
  }

  // ── Hover border (subtle, shown only when not selected) ──
  if (hovered_ && !highlighted_) {
    painter->setPen(QPen(QColor(255, 255, 255, dark ? 50 : 100), 1));
    painter->drawRect(1, 1, sec_w - 2, sec_h - 2);
  }

  // ── Bottom separator ──
  if (!highlighted_) {
    painter->setPen(QPen(dark ? QColor(45, 45, 48) : QColor(228, 228, 230), 1));
    painter->drawLine(4, sec_h - 1, sec_w, sec_h - 1);
  }
}

void FieldSectionItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  if (!hovered_) {
    hovered_ = true;
    emit hovered(name_, true);
  }
  update();
}

void FieldSectionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  if (hovered_) {
    hovered_ = false;
    emit hovered(name_, false);
  }
  update();
}

void FieldSectionItem::mousePressEvent(QGraphicsSceneMouseEvent*) {
  emit clicked(name_);
}

void FieldSectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
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
  setBackgroundBrush(etest::app::ThemeManager::instance().isDarkTheme()
                         ? QColor(24, 24, 26)
                         : QColor(252, 252, 253));
}

FieldSectionItem* IcdBitLayoutScene::addBlock(const QString& name,
                                               int byte_offset, int start_bit,
                                               int bit_width,
                                               const QColor& color) {
  auto* item = new FieldSectionItem(name, byte_offset, start_bit,
                                     bit_width, color, cell_size_, 8);
  item->setPos(left_margin_, next_y_);
  addItem(item);
  next_y_ += item->totalHeight() + section_spacing_;

  connect(item, &FieldSectionItem::clicked,
          this, &IcdBitLayoutScene::blockClicked);
  connect(item, &FieldSectionItem::contextMenuAction,
          this, &IcdBitLayoutScene::contextMenuAction);
  connect(item, &FieldSectionItem::hovered,
          this, &IcdBitLayoutScene::onBlockHovered);

  return item;
}

void IcdBitLayoutScene::clearBlocks() {
  for (auto* item : items()) {
    if (auto* block = dynamic_cast<FieldSectionItem*>(item)) {
      removeItem(block);
      delete block;
    }
  }
  next_y_ = left_margin_;
  selected_name_.clear();
  setSceneRect(0, 0, 100, 100);
}

void IcdBitLayoutScene::highlightBlock(const QString& name) {
  selected_name_ = name;
  for (auto* item : items()) {
    if (auto* block = dynamic_cast<FieldSectionItem*>(item)) {
      block->setHighlighted(block->name() == name);
    }
  }
}

void IcdBitLayoutScene::onBlockHovered(const QString& name, bool on) {
  // Toggle hover state on matching items — never touches highlighted_
  for (auto* item : items()) {
    if (auto* block = dynamic_cast<FieldSectionItem*>(item)) {
      if (block->name() == name) {
        block->setHovered(on);
      }
    }
  }
  emit blockHovered(name, on);
}

// ============================================================
// IcdBitLayoutView
// ============================================================
IcdBitLayoutView::IcdBitLayoutView(QWidget* parent) : QWidget(parent) {
  initUi();
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

  tb_layout->addStretch();
  tb_layout->addWidget(
      new QLabel(QStringLiteral("滚轮缩放 · 中键平移"), this));

  layout->addWidget(toolbar);

  // QGraphicsView
  view_ = new QGraphicsView(this);
  view_->setRenderHint(QPainter::Antialiasing);
  view_->setRenderHint(QPainter::SmoothPixmapTransform);
  view_->setDragMode(QGraphicsView::ScrollHandDrag);
  view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  view_->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
  view_->setBackgroundBrush(etest::app::ThemeManager::instance().isDarkTheme()
                                ? QColor(24, 24, 26)
                                : QColor(252, 252, 253));
  view_->setFrameShape(QFrame::NoFrame);
  view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  scene_ = new IcdBitLayoutScene(this);
  view_->setScene(scene_);
  view_->installEventFilter(this);

  connect(scene_, &IcdBitLayoutScene::blockClicked,
          this, &IcdBitLayoutView::blockClicked);
  connect(scene_, &IcdBitLayoutScene::contextMenuAction,
          this, &IcdBitLayoutView::contextMenuAction);
  connect(scene_, &IcdBitLayoutScene::blockHovered,
          this, &IcdBitLayoutView::blockHovered);

  // Theme switch: refresh backgrounds and reload blocks
  connect(&etest::app::ThemeManager::instance(),
          &etest::app::ThemeManager::themeChanged, this, [this](bool) {
            bool dark = etest::app::ThemeManager::instance().isDarkTheme();
            scene_->setBackgroundBrush(dark ? QColor(24, 24, 26)
                                            : QColor(252, 252, 253));
            view_->setBackgroundBrush(dark ? QColor(24, 24, 26)
                                           : QColor(252, 252, 253));
            if (last_frame_) {
              loadFromFrame(*last_frame_);
            }
          });

  layout->addWidget(view_, 1);
}

void IcdBitLayoutView::loadFromFrame(const icd::Frame& frame) {
  last_frame_ = &frame;
  clearBlocks();

  QVector<const icd::Node*> leaves;
  for (const auto& root : frame.roots()) {
    collectLeafNodes(*root, leaves);
  }

  if (leaves.isEmpty()) return;

  bool dark = etest::app::ThemeManager::instance().isDarkTheme();
  int cycle_idx = 0;

  for (auto* node : leaves) {
    QColor color = resolveGroupColor(*node, dark);

    // If the node's group/tag doesn't map to a known color →
    // cycle through the vibrant palette instead.
    {
      QString g = QString::fromStdString(node->attrs().group_name);
      if (g.isEmpty()) g = tagToGroupName(node->tag());
      bool has_known_color = (!g.isEmpty() && kGroupColors.contains(g));
      if (!has_known_color) {
        auto& gc = kPaletteCycle[cycle_idx % kPaletteCycle.size()];
        color = dark ? gc.dark : gc.light;
        ++cycle_idx;
      }
    }

    QString qname = QString::fromStdString(std::string(node->name()));
    addBlock(qname, node->offset(), node->bit_offset(), node->bit_width(),
             color);
  }

  scene_->setSceneRect(
      scene_->itemsBoundingRect().adjusted(-10, -10, 40, 40));
  view_->centerOn(0, 0);
}

void IcdBitLayoutView::collectLeafNodes(const icd::Node& node,
                                         QVector<const icd::Node*>& leaves) {
  if (node.children().empty()) {
    leaves.push_back(&node);
  } else {
    for (const auto& child : node.children()) {
      collectLeafNodes(*child, leaves);
    }
  }
}

FieldSectionItem* IcdBitLayoutView::addBlock(const QString& name,
                                              int byte_offset, int start_bit,
                                              int bit_width,
                                              const QColor& color) {
  return scene_->addBlock(name, byte_offset, start_bit, bit_width, color);
}

void IcdBitLayoutView::clearBlocks() { scene_->clearBlocks(); }

void IcdBitLayoutView::highlightBlock(const QString& name) {
  scene_->highlightBlock(name);
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
