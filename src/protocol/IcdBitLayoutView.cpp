#include "IcdBitLayoutView.h"
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsView>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <icd/node.hpp>
#include "IcdProtocolUtils.h"
#include "Logger.h"
#include "ThemeManager.h"

namespace {

struct GroupColor {
  QColor dark;
  QColor light;
};

static const QMap<QString, GroupColor> kGroupColors = {
    {QStringLiteral("header"), {QColor(50, 130, 240), QColor(75, 160, 250)}},
    {QStringLiteral("payload"), {QColor(60, 205, 75), QColor(80, 220, 95)}},
    {QStringLiteral("checksum"), {QColor(230, 70, 55), QColor(245, 90, 75)}},
    {QStringLiteral("length"), {QColor(252, 185, 50), QColor(255, 205, 70)}},
    {QStringLiteral("count"), {QColor(155, 50, 195), QColor(175, 70, 215)}},
    {QStringLiteral("address"), {QColor(55, 200, 240), QColor(75, 215, 250)}},
};

static const GroupColor kDefaultGroupColor = {QColor(127, 140, 141),
                                              QColor(149, 165, 166)};

static const QVector<GroupColor> kPaletteCycle = {
    kGroupColors["header"], kGroupColors["payload"], kGroupColors["checksum"],
    kGroupColors["length"], kGroupColors["count"],   kGroupColors["address"],
};

static QString tagToGroupName(icd::Tag tag) {
  switch (tag) {
    case icd::Tag::head:
      return QStringLiteral("header");
    case icd::Tag::length:
      return QStringLiteral("length");
    case icd::Tag::count:
      return QStringLiteral("count");
    case icd::Tag::sum:
    case icd::Tag::sum2:
    case icd::Tag::xor_:
    case icd::Tag::xor1:
    case icd::Tag::xor2:
      return QStringLiteral("checksum");
    default:
      return {};
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

static QColor gradientPartner(const QColor& base) {
  int h = base.hue();
  int s = base.saturation();
  int v = base.value();

  if (h >= 200 && h < 260) {
    return QColor::fromHsv(275, qMin(s + 10, 255), qMin(v + 20, 255));
  }
  if (h >= 90 && h < 170) {
    return QColor::fromHsv(178, s, qMin(v + 15, 255));
  }
  if (h >= 350 || h < 15) {
    return QColor::fromHsv(28, qMin(s + 5, 255), v);
  }
  if (h >= 30 && h < 65) {
    return QColor::fromHsv(85, qMin(s + 10, 255), qMin(v + 10, 255));
  }
  if (h >= 260 && h < 295) {
    return QColor::fromHsv(325, s, qMin(v + 10, 255));
  }
  if (h >= 170 && h < 200) {
    return QColor::fromHsv(218, s, qMin(v + 15, 255));
  }
  return base.lighter(130);
}

static int absoluteStartBit(const icd::Node& node) {
  return node.offset() * 8 + node.bit_offset();
}

static QString valueTypeText(const icd::Node& node) {
  return QString::fromLatin1(
      etest::protocol::utils::valueTypeName(node.value_type()));
}

static QString nodeNameText(const icd::Node& node) {
  return QString::fromStdString(std::string(node.name()));
}

static QString tagSemanticName(icd::Tag tag) {
  switch (tag) {
    case icd::Tag::head:
      return QStringLiteral("帧头");
    case icd::Tag::length:
      return QStringLiteral("长度");
    case icd::Tag::count:
      return QStringLiteral("计数");
    case icd::Tag::sum:
    case icd::Tag::sum2:
    case icd::Tag::xor_:
    case icd::Tag::xor1:
    case icd::Tag::xor2:
      return QStringLiteral("校验");
    case icd::Tag::init_value:
      return QStringLiteral("初值");
    case icd::Tag::signal_in_value:
      return QStringLiteral("信号值");
    case icd::Tag::big_endian_value:
      return QStringLiteral("大端");
    default:
      return {};
  }
}

static bool looksLikeEnum(const QString& value_text_list) {
  // 形如 "不用=0,左单元=1" 视为枚举定义
  return value_text_list.contains('=') && value_text_list.contains(',');
}

}  // anonymous namespace

using namespace etest::core_ui;

namespace etest::protocol {

QString buildNodeTooltip(const icd::Node& node) {
  QString name = nodeNameText(node);
  QString desc = QString::fromStdString(std::string(node.description()));
  QString type_text =
      QString::fromLatin1(utils::valueTypeName(node.value_type()));

  int abs_start = node.offset() * 8 + node.bit_offset();
  int abs_end = abs_start + node.bit_width() - 1;

  QStringList lines;
  lines << QStringLiteral("<b>%1</b>").arg(name);
  if (!desc.isEmpty()) {
    lines << desc;
  }
  lines << QStringLiteral("Type: %1 | Bit: %2~%3 (%4 bits) | Offset: %5")
               .arg(type_text)
               .arg(abs_start)
               .arg(abs_end)
               .arg(node.bit_width())
               .arg(node.offset());

  QString tag_name = tagSemanticName(node.tag());
  if (!tag_name.isEmpty()) {
    lines << QStringLiteral("Tag: %1").arg(tag_name);
  }
  if (isNodeBigEndian(node)) {
    lines << QStringLiteral("ByteOrder: 大端");
  }

  const auto& attrs = node.attrs();
  if (!attrs.unit.empty()) {
    lines << QStringLiteral("Unit: %1").arg(QString::fromStdString(attrs.unit));
  }
  if (attrs.is_scaled) {
    QStringList scale_parts;
    scale_parts << QStringLiteral("Scale");
    if (attrs.scale_a.has_value()) {
      scale_parts << QStringLiteral("A=%1").arg(*attrs.scale_a);
    }
    if (attrs.scale_b.has_value()) {
      scale_parts << QStringLiteral("B=%1").arg(*attrs.scale_b);
    }
    lines << scale_parts.join(' ');
  }
  if (attrs.min.has_value() || attrs.max.has_value()) {
    QString min_text = attrs.min.has_value() ? QString::number(*attrs.min)
                                             : QStringLiteral("-");
    QString max_text = attrs.max.has_value() ? QString::number(*attrs.max)
                                             : QStringLiteral("-");
    lines << QStringLiteral("Range: %1 ~ %2").arg(min_text, max_text);
  }
  if (!attrs.value_text_list.empty()) {
    QString vtl = QString::fromStdString(attrs.value_text_list);
    QString label =
        looksLikeEnum(vtl) ? QStringLiteral("枚举") : QStringLiteral("默认值");
    lines << QStringLiteral("%1: %2").arg(label, vtl);
  }
  if (!attrs.link_to.empty()) {
    lines << QStringLiteral("LinkTo: %1")
                 .arg(QString::fromStdString(attrs.link_to));
  }
  if (!attrs.system_name.empty()) {
    lines << QStringLiteral("System: %1")
                 .arg(QString::fromStdString(attrs.system_name));
  }
  if (!attrs.group_name.empty()) {
    lines << QStringLiteral("Group: %1")
                 .arg(QString::fromStdString(attrs.group_name));
  }

  return lines.join(QStringLiteral("<br>"));
}

QStringList buildNodeBadges(const icd::Node& node) {
  QStringList badges;
  QString tag_name = tagSemanticName(node.tag());
  if (!tag_name.isEmpty()) {
    badges << tag_name;
  }
  if (isNodeBigEndian(node)) {
    badges << QStringLiteral("大端");
  }
  if (node.attrs().is_scaled) {
    badges << QStringLiteral("缩放");
  }
  QString vtl = QString::fromStdString(node.attrs().value_text_list);
  if (looksLikeEnum(vtl)) {
    badges << QStringLiteral("枚举");
  }
  return badges;
}

bool isNodeBigEndian(const icd::Node& node) {
  return node.tag() == icd::Tag::big_endian_value;
}

LayoutNodeItem::LayoutNodeItem(QGraphicsItem* parent)
    : QGraphicsObject(parent) {}

FieldSectionItem::FieldSectionItem(const icd::Node* node,
                                   const QString& value_type,
                                   const QColor& color,
                                   int cell_size,
                                   int bits_per_row,
                                   QGraphicsItem* parent)
    : FieldSectionItem(node ? nodeNameText(*node) : QString(),
                       value_type,
                       node ? node->offset() : 0,
                       node ? node->bit_offset() : 0,
                       node ? node->bit_width() : 0,
                       color,
                       cell_size,
                       bits_per_row,
                       parent) {
  node_ = node;
  if (node_) {
    setToolTip(buildNodeTooltip(*node_));
  }
}

FieldSectionItem::FieldSectionItem(const QString& name,
                                   const QString& value_type,
                                   int byte_offset,
                                   int start_bit,
                                   int bit_width,
                                   const QColor& color,
                                   int cell_size,
                                   int bits_per_row,
                                   QGraphicsItem* parent)
    : LayoutNodeItem(parent),
      name_(name),
      value_type_(value_type),
      byte_offset_(byte_offset),
      start_bit_(start_bit),
      bit_width_(bit_width),
      color_(color),
      cell_size_(cell_size),
      bits_per_row_(bits_per_row) {
  setAcceptHoverEvents(true);
  setCursor(Qt::PointingHandCursor);

  header_font_.setPointSize(10);
  header_font_.setBold(true);
  cell_font_.setPointSize(7);
}

int FieldSectionItem::totalHeight() const {
  if (bit_width_ <= 0) {
    return kHeaderHeight;
  }
  int rows = (bit_width_ + bits_per_row_ - 1) / bits_per_row_;
  return kHeaderHeight + rows * cell_size_;
}

int FieldSectionItem::sectionWidth() const {
  if (bit_width_ <= 0) {
    return std::max(cell_size_, kMinSectionWidth);
  }
  int cols = std::min(bit_width_, bits_per_row_);
  return std::max(cols * cell_size_, kMinSectionWidth);
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

void FieldSectionItem::setHighlightedNode(const icd::Node* node) {
  setHighlighted(node_ && node_ == node);
}

void FieldSectionItem::setHoveredNode(const icd::Node* node, bool on) {
  if (node_ && node_ == node) {
    setHovered(on);
  }
}

void FieldSectionItem::paint(QPainter* painter,
                             const QStyleOptionGraphicsItem*,
                             QWidget*) {
  auto& tm = ThemeManager::instance();

  int cols = std::min(bit_width_, bits_per_row_);
  int rows = (bit_width_ + bits_per_row_ - 1) / bits_per_row_;
  int sec_w = sectionWidth();
  int sec_h = totalHeight();

  QColor bg = tm.panelBackground();
  bool dark = tm.isDarkTheme();
  if (highlighted_) {
    bg = tm.hoverBackground();
  }
  painter->fillRect(0, 0, sec_w, sec_h, bg);

  if (hovered_ && !highlighted_) {
    QColor ho = Qt::white;
    ho.setAlpha(dark ? 15 : 30);
    painter->fillRect(0, 0, sec_w, sec_h, ho);
  }

  int accent_w = highlighted_ ? 6 : 4;
  int sec_hdr = kHeaderHeight;

  QColor c1 = color_;
  QColor c2 = gradientPartner(color_);
  QColor c_bright = color_.lighter(180);

  {
    int h = highlighted_ ? sec_h : sec_hdr;
    QLinearGradient g(0, 0, 0, h);
    g.setColorAt(0.0, c_bright);
    g.setColorAt(0.4, c1);
    g.setColorAt(0.8, c2);
    g.setColorAt(1.0, c2.darker(130));
    painter->fillRect(0, 0, accent_w, h, g);
  }

  {
    QLinearGradient g(0, 0, sec_w, 0);
    g.setColorAt(0.0, c1);
    g.setColorAt(0.5, c2);
    g.setColorAt(0.85, c2.darker(110));
    g.setColorAt(1.0, c2.darker(130));
    painter->fillRect(0, 0, sec_w, sec_hdr, g);
  }

  {
    QLinearGradient g(0, 0, 0, sec_hdr * 0.55);
    g.setColorAt(0.0, QColor(255, 255, 255, 60));
    g.setColorAt(0.5, QColor(255, 255, 255, 20));
    g.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter->fillRect(0, 0, sec_w, sec_hdr * 0.55, g);
  }

  {
    QLinearGradient g(0, 0, sec_w, 0);
    g.setColorAt(0.0, c1.lighter(120));
    g.setColorAt(0.4, c2);
    g.setColorAt(0.8, c2.darker(110));
    g.setColorAt(1.0, c2.darker(140));
    painter->setPen(QPen(g, highlighted_ ? 2 : 1));
    painter->drawLine(0, sec_hdr, sec_w, sec_hdr);
  }

  int global_start = byte_offset_ * 8 + start_bit_;
  int global_end = global_start + bit_width_ - 1;
  QString range_str =
      (bit_width_ > 1)
          ? QStringLiteral("%1~%2").arg(global_start).arg(global_end)
          : QString::number(global_start);
  QString header_text =
      QStringLiteral("%1  [%2 bits]").arg(name_).arg(range_str);

  QFont badge_font;
  badge_font.setPointSize(8);
  badge_font.setBold(true);
  QFontMetrics badge_fm(badge_font);

  QStringList sem_badges = node_ ? buildNodeBadges(*node_) : QStringList{};
  int badge_pad = 6;
  int badge_gap = 4;
  int badge_h = badge_fm.height() + 4;

  int value_type_w = 0;
  if (!value_type_.isEmpty()) {
    value_type_w = badge_fm.horizontalAdvance(value_type_) + badge_pad * 2;
  }
  int sem_badges_w = 0;
  for (const auto& b : sem_badges) {
    sem_badges_w += badge_fm.horizontalAdvance(b) + badge_pad * 2 + badge_gap;
  }

  int badge_reserved = value_type_w + sem_badges_w + 8;

  painter->setPen(highlighted_ ? Qt::white : c_bright);
  painter->setFont(header_font_);
  QRectF name_rect(14, 0, sec_w - 14 - badge_reserved, sec_hdr);
  QString elided_header_text = painter->fontMetrics().elidedText(
      header_text, Qt::ElideRight,
      qMax(0, static_cast<int>(name_rect.width())));
  painter->drawText(name_rect, Qt::AlignVCenter | Qt::AlignLeft,
                    elided_header_text);

  int cur_x = sec_w - 8;
  painter->setFont(badge_font);

  if (!value_type_.isEmpty()) {
    int badge_w = badge_fm.horizontalAdvance(value_type_) + badge_pad * 2;
    cur_x -= badge_w;
    int badge_y = (sec_hdr - badge_h) / 2;

    QColor badge_bg = c_bright;
    badge_bg.setAlpha(highlighted_ ? 80 : 50);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badge_bg);
    painter->drawRoundedRect(cur_x, badge_y, badge_w, badge_h, 4, 4);

    painter->setPen(highlighted_ ? Qt::white : QColor(255, 255, 255, 220));
    painter->drawText(QRectF(cur_x, badge_y, badge_w, badge_h), Qt::AlignCenter,
                      value_type_);
    cur_x -= badge_gap;
  }

  for (const auto& b : sem_badges) {
    int badge_w = badge_fm.horizontalAdvance(b) + badge_pad * 2;
    cur_x -= badge_w;
    int badge_y = (sec_hdr - badge_h) / 2;

    QColor badge_bg =
        highlighted_ ? QColor(255, 255, 255, 90) : QColor(255, 255, 255, 45);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badge_bg);
    painter->drawRoundedRect(cur_x, badge_y, badge_w, badge_h, 4, 4);

    painter->setPen(highlighted_ ? Qt::white : QColor(255, 255, 255, 230));
    painter->drawText(QRectF(cur_x, badge_y, badge_w, badge_h), Qt::AlignCenter,
                      b);
    cur_x -= badge_gap;
  }

  int global_base = byte_offset_ * 8 + start_bit_;
  painter->fillRect(0, kHeaderHeight + 1, sec_w, sec_h - kHeaderHeight - 1,
                    dark ? QColor(28, 28, 30) : QColor(250, 250, 250));

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      int local_idx = r * bits_per_row_ + c;
      if (local_idx >= bit_width_) {
        break;
      }

      int cell_x = c * cell_size_;
      int cell_y = kHeaderHeight + r * cell_size_;

      painter->fillRect(cell_x + 1, cell_y + 1, cell_size_ - 2, cell_size_ - 2,
                        dark ? QColor(38, 38, 40) : QColor(238, 238, 240));

      QColor border_col = dark ? QColor(50, 50, 53) : QColor(222, 222, 225);
      painter->setPen(QPen(border_col, 1));
      painter->drawLine(cell_x + cell_size_, cell_y + 1, cell_x + cell_size_,
                        cell_y + cell_size_);
      painter->drawLine(cell_x + 1, cell_y + cell_size_, cell_x + cell_size_,
                        cell_y + cell_size_);

      int global_bit = global_base + local_idx;
      painter->setPen(dark ? QColor(160, 160, 165) : QColor(130, 130, 135));
      painter->setFont(cell_font_);
      painter->drawText(
          QRectF(cell_x + 1, cell_y + 1, cell_size_ - 2, cell_size_ - 2),
          Qt::AlignCenter, QString::number(global_bit));
    }
  }

  if (highlighted_) {
    QLinearGradient g(0, 0, 0, sec_h);
    g.setColorAt(0.0, dark ? Qt::white : color_.lighter(140));
    g.setColorAt(0.4, color_);
    g.setColorAt(0.7, c2);
    g.setColorAt(1.0, c2.darker(120));
    painter->setPen(QPen(g, 2));
    painter->drawRect(1, 1, sec_w - 2, sec_h - 2);
  }

  if (hovered_ && !highlighted_) {
    painter->setPen(QPen(QColor(255, 255, 255, dark ? 50 : 100), 1));
    painter->drawRect(1, 1, sec_w - 2, sec_h - 2);
  }

  if (!highlighted_) {
    painter->setPen(QPen(dark ? QColor(45, 45, 48) : QColor(228, 228, 230), 1));
    painter->drawLine(4, sec_h - 1, sec_w, sec_h - 1);
  }
}

void FieldSectionItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  if (!hovered_) {
    hovered_ = true;
    emit hovered(node_, true);
  }
  update();
}

void FieldSectionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  if (hovered_) {
    hovered_ = false;
    emit hovered(node_, false);
  }
  update();
}

void FieldSectionItem::mousePressEvent(QGraphicsSceneMouseEvent*) {
  emit clicked(node_);
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
    emit clicked(node_);
  } else if (chosen == act_add_before) {
    emit contextMenuAction(node_, QStringLiteral("addBefore"));
  } else if (chosen == act_add_after) {
    emit contextMenuAction(node_, QStringLiteral("addAfter"));
  } else if (chosen == act_delete) {
    emit contextMenuAction(node_, QStringLiteral("delete"));
  }
}

ChildFieldItem::ChildFieldItem(const icd::Node* node,
                               int relative_start,
                               int relative_end,
                               const QString& value_type,
                               const QColor& color,
                               int cell_size,
                               QGraphicsItem* parent)
    : QGraphicsObject(parent),
      node_(node),
      relative_start_(relative_start),
      relative_end_(relative_end),
      value_type_(value_type),
      color_(color),
      cell_size_(cell_size) {
  setAcceptHoverEvents(true);
  setCursor(Qt::PointingHandCursor);
  row_font_.setPointSize(9);
  if (node_) {
    setToolTip(buildNodeTooltip(*node_));
  }
}

QRectF ChildFieldItem::boundingRect() const {
  return QRectF(0, 0, row_width_, kRowHeight);
}

void ChildFieldItem::setHighlighted(bool on) {
  highlighted_ = on;
  update();
}

void ChildFieldItem::setHovered(bool on) {
  hovered_ = on;
  update();
}

void ChildFieldItem::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem*,
                           QWidget*) {
  auto& tm = ThemeManager::instance();
  bool dark = tm.isDarkTheme();
  int rh = kRowHeight;

  // Selection/hover background
  if (highlighted_) {
    QColor sel = color_;
    sel.setAlpha(tm.isDarkTheme() ? 50 : 30);
    painter->fillRect(0, 0, row_width_, rh, sel);
  } else if (hovered_) {
    QColor ho = Qt::white;
    ho.setAlpha(tm.isDarkTheme() ? 15 : 25);
    painter->fillRect(0, 0, row_width_, rh, ho);
  }

  // Tree connector: ├─ or └─
  QString connector =
      is_last_ ? QStringLiteral("  └─ ") : QStringLiteral("  ├─ ");
  painter->setPen(tm.isDarkTheme() ? QColor(160, 160, 165)
                                   : QColor(130, 130, 135));
  painter->setFont(row_font_);
  QFontMetrics fm(row_font_);
  int conn_w = fm.horizontalAdvance(connector);
  painter->drawText(4, 0, conn_w, rh, Qt::AlignVCenter | Qt::AlignLeft,
                    connector);

  // Name
  const int range_w = fm.horizontalAdvance(QStringLiteral("000~000"));
  const int pill_space = 100;  // reserved for type badge
  int name_max_w = row_width_ - conn_w - 12 - range_w - pill_space;
  QString name_text = node_ ? nodeNameText(*node_) : QString();
  QString elided =
      fm.elidedText(name_text, Qt::ElideRight, qMax(0, name_max_w));
  painter->setPen(dark ? QColor(220, 220, 225) : QColor(45, 45, 50));
  painter->drawText(4 + conn_w, 0, name_max_w, rh,
                    Qt::AlignVCenter | Qt::AlignLeft, elided);

  // Type pill
  if (!value_type_.isEmpty()) {
    int pill_w = fm.horizontalAdvance(value_type_) + 10;
    int pill_h = fm.height() + 2;
    int pill_x = row_width_ - range_w - pill_w - 8;
    int pill_y = (rh - pill_h) / 2;
    QColor pill_bg = color_;
    pill_bg.setAlpha(highlighted_ ? 80 : 30);
    painter->setPen(Qt::NoPen);
    painter->setBrush(pill_bg);
    painter->drawRoundedRect(pill_x, pill_y, pill_w, pill_h, 4, 4);
    painter->setPen(color_.lighter(180));
    painter->drawText(QRectF(pill_x, pill_y, pill_w, pill_h), Qt::AlignCenter,
                      value_type_);
  }

  // Bit range
  QString range_str =
      (relative_start_ == relative_end_)
          ? QString::number(relative_start_)
          : QStringLiteral("%1~%2").arg(relative_start_).arg(relative_end_);
  painter->setPen(dark ? QColor(150, 150, 155) : QColor(110, 110, 115));
  painter->drawText(row_width_ - range_w - 4, 0, range_w, rh,
                    Qt::AlignVCenter | Qt::AlignRight, range_str);
}

void ChildFieldItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  if (!hovered_) {
    hovered_ = true;
    emit hovered(node_, true);
  }
  update();
}

void ChildFieldItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  if (hovered_) {
    hovered_ = false;
    emit hovered(node_, false);
  }
  update();
}

void ChildFieldItem::mousePressEvent(QGraphicsSceneMouseEvent*) {
  emit clicked(node_);
}

void ChildFieldItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
  QMenu menu;
  QAction* act_edit = menu.addAction(QStringLiteral("编辑"));
  menu.addSeparator();
  QAction* act_add_before = menu.addAction(QStringLiteral("在前面插入"));
  QAction* act_add_after = menu.addAction(QStringLiteral("在后面插入"));
  menu.addSeparator();
  QAction* act_delete = menu.addAction(QStringLiteral("删除"));

  QAction* chosen = menu.exec(event->screenPos());
  if (chosen == act_edit) {
    emit clicked(node_);
  } else if (chosen == act_add_before) {
    emit contextMenuAction(node_, QStringLiteral("addBefore"));
  } else if (chosen == act_add_after) {
    emit contextMenuAction(node_, QStringLiteral("addAfter"));
  } else if (chosen == act_delete) {
    emit contextMenuAction(node_, QStringLiteral("delete"));
  }
}

ContainerFieldItem::ContainerFieldItem(const icd::Node* node,
                                       const QString& value_type,
                                       const QColor& color,
                                       int cell_size,
                                       QGraphicsItem* parent)
    : LayoutNodeItem(parent),
      node_(node),
      name_(node ? nodeNameText(*node) : QString()),
      value_type_(value_type),
      color_(color),
      cell_size_(cell_size) {
  setAcceptHoverEvents(true);
  setCursor(Qt::PointingHandCursor);
  header_font_.setPointSize(10);
  header_font_.setBold(true);
  row_font_.setPointSize(9);
  if (node_) {
    setToolTip(buildNodeTooltip(*node_));
  }
  initChildren();
}

int ContainerFieldItem::parentStartBit() const {
  return node_ ? absoluteStartBit(*node_) : 0;
}

void ContainerFieldItem::initChildren() {
  if (!node_) {
    return;
  }

  const int row_w = sectionWidth() - kChildIndent - 8;
  int parent_start = parentStartBit();
  int child_count = static_cast<int>(node_->children().size());
  for (int i = 0; i < child_count; ++i) {
    const auto& child = node_->children()[i];
    int child_start = absoluteStartBit(*child);
    int relative_start = child_start - parent_start;
    int relative_end = relative_start + child->bit_width() - 1;
    auto* item =
        new ChildFieldItem(child.get(), relative_start, relative_end,
                           valueTypeText(*child), color_, cell_size_, this);
    item->setRowWidth(row_w);
    item->is_last_ = (i == child_count - 1);
    item->setPos(kChildIndent, kHeaderHeight + 8 + i * kRowHeight);
    child_items_.append(item);

    connect(item, &ChildFieldItem::clicked, this, &ContainerFieldItem::clicked);
    connect(item, &ChildFieldItem::contextMenuAction, this,
            &ContainerFieldItem::contextMenuAction);
    connect(item, &ChildFieldItem::hovered, this, &ContainerFieldItem::hovered);
  }
}

int ContainerFieldItem::sectionWidth() const {
  return std::max(static_cast<int>(name_.size()) * 9 + 200, kMinSectionWidth);
}

int ContainerFieldItem::totalHeight() const {
  int row_count = static_cast<int>(child_items_.size());
  return kHeaderHeight + 8 + row_count * kRowHeight + kContainerPadding;
}

QRectF ContainerFieldItem::boundingRect() const {
  return QRectF(0, 0, sectionWidth(), totalHeight());
}

bool ContainerFieldItem::containsChildNode(const icd::Node* node) const {
  return childFieldItem(node) != nullptr;
}

QPair<int, int> ContainerFieldItem::childRelativeRange(
    const icd::Node* node) const {
  if (auto* item = childFieldItem(node)) {
    return qMakePair(item->relativeStart(), item->relativeEnd());
  }
  return qMakePair(-1, -1);
}

ChildFieldItem* ContainerFieldItem::childFieldItem(
    const icd::Node* node) const {
  for (auto* item : child_items_) {
    if (item->node() == node) {
      return item;
    }
  }
  return nullptr;
}

void ContainerFieldItem::setHighlightedNode(const icd::Node* node) {
  highlighted_ = node_ && node_ == node;
  for (auto* child : child_items_) {
    child->setHighlighted(child->node() == node);
  }
  update();
}

void ContainerFieldItem::setHoveredNode(const icd::Node* node, bool on) {
  if (node_ && node_ == node) {
    hovered_ = on;
  }
  for (auto* child : child_items_) {
    if (child->node() == node) {
      child->setHovered(on);
    }
  }
  update();
}

void ContainerFieldItem::paint(QPainter* painter,
                               const QStyleOptionGraphicsItem*,
                               QWidget*) {
  auto& tm = ThemeManager::instance();
  bool dark = tm.isDarkTheme();
  int sec_w = sectionWidth();
  int sec_h = totalHeight();

  // Container background
  QColor bg = tm.panelBackground();
  painter->fillRect(0, 0, sec_w, sec_h, bg);

  // Header bar
  QColor c1 = color_;
  QColor c2 = gradientPartner(color_);
  {
    QLinearGradient g(0, 0, sec_w, 0);
    g.setColorAt(0.0, c1);
    g.setColorAt(0.55, c2);
    g.setColorAt(1.0, c2.darker(125));
    painter->fillRect(0, 0, sec_w, kHeaderHeight, g);
  }

  // Header title (without value_type — it's drawn as a badge on the right)
  QString range;
  if (node_) {
    int start = absoluteStartBit(*node_);
    int end = start + node_->bit_width() - 1;
    range = QStringLiteral("%1~%2").arg(start).arg(end);
  }
  QString title = QStringLiteral("%1  [%2 bits]").arg(name_, range);

  // Build badge list: type badge first, then semantic badges
  QStringList all_badges;
  if (!value_type_.isEmpty()) {
    all_badges << value_type_;
  }
  all_badges << (node_ ? buildNodeBadges(*node_) : QStringList{});

  QFont badge_font;
  badge_font.setPointSize(8);
  badge_font.setBold(true);
  QFontMetrics badge_fm(badge_font);
  int badge_pad = 6;
  int badge_gap = 4;
  int badge_h = badge_fm.height() + 4;
  int badges_total_w = 0;
  for (const auto& b : all_badges) {
    badges_total_w += badge_fm.horizontalAdvance(b) + badge_pad * 2 + badge_gap;
  }

  painter->setPen(Qt::white);
  painter->setFont(header_font_);
  painter->drawText(QRectF(12, 0, sec_w - 24 - badges_total_w, kHeaderHeight),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    painter->fontMetrics().elidedText(
                        title, Qt::ElideRight, sec_w - 24 - badges_total_w));

  painter->setFont(badge_font);
  int cur_x = sec_w - 8;
  for (const auto& b : all_badges) {
    int badge_w = badge_fm.horizontalAdvance(b) + badge_pad * 2;
    cur_x -= badge_w;
    int badge_y = (kHeaderHeight - badge_h) / 2;
    QColor badge_bg = QColor(255, 255, 255, 45);
    painter->setPen(Qt::NoPen);
    painter->setBrush(badge_bg);
    painter->drawRoundedRect(cur_x, badge_y, badge_w, badge_h, 4, 4);
    painter->setPen(QColor(255, 255, 255, 230));
    painter->drawText(QRectF(cur_x, badge_y, badge_w, badge_h), Qt::AlignCenter,
                      b);
    cur_x -= badge_gap;
  }

  // Outer frame border
  QColor border_col = dark ? QColor(80, 80, 85) : QColor(200, 200, 205);
  painter->setPen(QPen(border_col, 1));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(0, 0, sec_w - 1, sec_h - 1);

  // Thin separator below header
  painter->setPen(QPen(c2.lighter(150), 1));
  painter->drawLine(1, kHeaderHeight, sec_w - 2, kHeaderHeight);

  // Selection indicator on outer frame
  if (highlighted_) {
    painter->setPen(QPen(Qt::white, 2));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(1, 1, sec_w - 3, sec_h - 3);
  }
}

void ContainerFieldItem::hoverEnterEvent(QGraphicsSceneHoverEvent*) {
  if (!hovered_) {
    hovered_ = true;
    emit hovered(node_, true);
  }
  update();
}

void ContainerFieldItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
  if (hovered_) {
    hovered_ = false;
    emit hovered(node_, false);
  }
  update();
}

void ContainerFieldItem::mousePressEvent(QGraphicsSceneMouseEvent*) {
  emit clicked(node_);
}

void ContainerFieldItem::contextMenuEvent(
    QGraphicsSceneContextMenuEvent* event) {
  QMenu menu;
  QAction* act_edit = menu.addAction(QStringLiteral("编辑"));
  menu.addSeparator();
  QAction* act_add_before = menu.addAction(QStringLiteral("在前面插入"));
  QAction* act_add_after = menu.addAction(QStringLiteral("在后面插入"));
  menu.addSeparator();
  QAction* act_delete = menu.addAction(QStringLiteral("删除"));

  QAction* chosen = menu.exec(event->screenPos());
  if (chosen == act_edit) {
    emit clicked(node_);
  } else if (chosen == act_add_before) {
    emit contextMenuAction(node_, QStringLiteral("addBefore"));
  } else if (chosen == act_add_after) {
    emit contextMenuAction(node_, QStringLiteral("addAfter"));
  } else if (chosen == act_delete) {
    emit contextMenuAction(node_, QStringLiteral("delete"));
  }
}

IcdBitLayoutScene::IcdBitLayoutScene(QObject* parent) : QGraphicsScene(parent) {
  LOG_INFO(
      "ICD_UI", "theme manager scene background: {}, window background: {}",
      ThemeManager::instance().sceneBackground().name().toStdString().c_str(),
      ThemeManager::instance().windowBackground().name().toStdString().c_str());

  setBackgroundBrush(ThemeManager::instance().sceneBackground());
}

void IcdBitLayoutScene::appendLayoutItem(LayoutNodeItem* item) {
  item->setPos(left_margin_, next_y_);
  addItem(item);
  items_.append(item);
  next_y_ += item->totalHeight() + section_spacing_;

  connect(item, &LayoutNodeItem::clicked, this,
          &IcdBitLayoutScene::nodeClicked);
  connect(item, &LayoutNodeItem::contextMenuAction, this,
          &IcdBitLayoutScene::nodeContextMenuAction);
  connect(item, &LayoutNodeItem::hovered, this,
          &IcdBitLayoutScene::onNodeHovered);
}

FieldSectionItem* IcdBitLayoutScene::addBlock(const QString& name,
                                              const QString& value_type,
                                              int byte_offset,
                                              int start_bit,
                                              int bit_width,
                                              const QColor& color) {
  auto* item = new FieldSectionItem(name, value_type, byte_offset, start_bit,
                                    bit_width, color, cell_size_, 8);
  appendLayoutItem(item);
  return item;
}

FieldSectionItem* IcdBitLayoutScene::addField(const icd::Node* node,
                                              const QString& value_type,
                                              const QColor& color) {
  auto* item = new FieldSectionItem(node, value_type, color, cell_size_, 8);
  appendLayoutItem(item);
  return item;
}

ContainerFieldItem* IcdBitLayoutScene::addContainer(const icd::Node* node,
                                                    const QString& value_type,
                                                    const QColor& color) {
  auto* item = new ContainerFieldItem(node, value_type, color, cell_size_);
  appendLayoutItem(item);
  return item;
}

void IcdBitLayoutScene::clearBlocks() {
  for (auto* item : items_) {
    removeItem(item);
    delete item;
  }
  items_.clear();
  next_y_ = left_margin_;
  section_spacing_ = 12;
  selected_node_ = nullptr;
  setSceneRect(0, 0, 640, 480);
}

void IcdBitLayoutScene::highlightNode(const icd::Node* node) {
  selected_node_ = node;
  for (auto* item : items_) {
    item->setHighlightedNode(node);
  }
}

void IcdBitLayoutScene::onNodeHovered(const icd::Node* node, bool on) {
  for (auto* item : items_) {
    item->setHoveredNode(node, on);
  }
  emit nodeHovered(node, on);
}

int IcdBitLayoutScene::fieldItemCount() const {
  int count = 0;
  for (auto* item : items_) {
    if (dynamic_cast<FieldSectionItem*>(item)) {
      ++count;
    }
  }
  return count;
}

int IcdBitLayoutScene::containerItemCount() const {
  int count = 0;
  for (auto* item : items_) {
    if (dynamic_cast<ContainerFieldItem*>(item)) {
      ++count;
    }
  }
  return count;
}

QVector<ContainerFieldItem*> IcdBitLayoutScene::containerItems() const {
  QVector<ContainerFieldItem*> containers;
  for (auto* item : items_) {
    if (auto* container = dynamic_cast<ContainerFieldItem*>(item)) {
      containers.append(container);
    }
  }
  return containers;
}

IcdBitLayoutView::IcdBitLayoutView(QWidget* parent) : QGraphicsView(parent) {
  initUi();
}

void IcdBitLayoutView::initUi() {
  setRenderHint(QPainter::Antialiasing);
  setDragMode(QGraphicsView::ScrollHandDrag);
  setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
  setBackgroundBrush(ThemeManager::instance().sceneBackground());
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  scene_ = new IcdBitLayoutScene(this);
  setScene(scene_);

  connect(scene_, &IcdBitLayoutScene::nodeClicked, this,
          &IcdBitLayoutView::nodeClicked);
  connect(scene_, &IcdBitLayoutScene::nodeContextMenuAction, this,
          &IcdBitLayoutView::nodeContextMenuAction);
  connect(scene_, &IcdBitLayoutScene::nodeHovered, this,
          &IcdBitLayoutView::nodeHovered);

  connect(
      &ThemeManager::instance(), &ThemeManager::themeChanged, this,
      [this](bool) {
        scene_->setBackgroundBrush(ThemeManager::instance().sceneBackground());
        setBackgroundBrush(ThemeManager::instance().sceneBackground());
        if (last_frame_) {
          loadFromFrame(*last_frame_);
        }
      });
}

void IcdBitLayoutView::loadFromFrame(const icd::Frame& frame) {
  last_frame_ = &frame;
  clearBlocks();

  if (frame.roots().empty()) {
    return;
  }

  QVector<const icd::Node*> roots;
  for (const auto& root : frame.roots()) {
    roots.push_back(root.get());
  }

  std::sort(roots.begin(), roots.end(),
            [](const icd::Node* a, const icd::Node* b) {
              if (a->offset() != b->offset()) {
                return a->offset() < b->offset();
              }
              if (a->bit_offset() != b->bit_offset()) {
                return a->bit_offset() < b->bit_offset();
              }
              return a->bit_width() > b->bit_width();
            });

  bool dark = ThemeManager::instance().isDarkTheme();
  int cycle_idx = 0;

  for (auto* node : roots) {
    QColor color = resolveGroupColor(*node, dark);
    QString g = QString::fromStdString(node->attrs().group_name);
    if (g.isEmpty()) {
      g = tagToGroupName(node->tag());
    }
    bool has_known_color = (!g.isEmpty() && kGroupColors.contains(g));
    if (!has_known_color) {
      const auto& gc = kPaletteCycle[cycle_idx % kPaletteCycle.size()];
      color = dark ? gc.dark : gc.light;
      ++cycle_idx;
    }

    if (node->children().empty()) {
      scene_->addField(node, valueTypeText(*node), color);
    } else {
      scene_->addContainer(node, valueTypeText(*node), color);
    }
  }

  QRectF content_rect =
      scene_->itemsBoundingRect().adjusted(-10, -10, 160, 160);
  QRectF default_rect(0, 0, 960, 720);
  scene_->setSceneRect(content_rect.united(default_rect));
  centerOn(0, 0);
}

FieldSectionItem* IcdBitLayoutView::addBlock(const QString& name,
                                             const QString& value_type,
                                             int byte_offset,
                                             int start_bit,
                                             int bit_width,
                                             const QColor& color) {
  return scene_->addBlock(name, value_type, byte_offset, start_bit, bit_width,
                          color);
}

void IcdBitLayoutView::clearBlocks() {
  last_frame_ = nullptr;
  scene_->clearBlocks();
  resetTransform();
  centerOn(0, 0);
}

void IcdBitLayoutView::highlightNode(const icd::Node* node) {
  scene_->highlightNode(node);
}

}  // namespace etest::protocol
