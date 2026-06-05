#include "WelcomeWidget.h"

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QVBoxLayout>

#include "widgets/EyeWidget.h"
#include "common/ThemeManager.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "widgets/PaintedClockWidget.h"
#include "grid/grid_global_def.hpp"
#include "grid/grid_tile.h"
#include "core/utils/PixmapUtil.h"
#include "grid/grid_global_def.hpp"
#include "grid/grid_layout.h"
#include "project/ProjectManager.h"
#include "version.h"

namespace etest::app {

using namespace core::config;
using namespace core::project;

WelcomeWidget::WelcomeWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
  loadBackground();
  showRandomTip();
}

void WelcomeWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  if (!bg_pixmap_.isNull()) {
    switch (bg_mode_) {
      case 0: {
        int x = (width() - bg_pixmap_.width()) / 2;
        int y = (height() - bg_pixmap_.height()) / 2;
        p.drawPixmap(x, y, bg_pixmap_);
        break;
      }
      case 1:
        p.drawTiledPixmap(rect(), bg_pixmap_);
        break;
      case 2:
        p.drawPixmap(rect(), bg_pixmap_);
        break;
    }
  }

  // 绘制网格叠加层
  if (draw_grid_overlay_ && grid_layout_) {
    p.setRenderHint(QPainter::Antialiasing, true);
    for (auto& cell : grid_layout_->layoutedGrid()) {
      if (cell.second) {
        // 已占用的网格
        p.fillRect(cell.first, occupied_grid_color_);
      }
      // 网格线
      p.setPen(QPen(grid_color_, 0.5, Qt::DashLine));
      p.setBrush(Qt::NoBrush);
      p.drawRect(cell.first);
    }
  }

  // 绘制拖拽预览
  if (!best_drop_rect_.isEmpty()) {
    p.setRenderHint(QPainter::Antialiasing, true);
    QRectF largeRect;
    for (auto& r : best_drop_rect_) largeRect = largeRect.united(r);

    switch (drag_preview_style_) {
      case None:
        break;
      case Grid: {
        p.setPen(QPen(QColor(0, 120, 215), 2));
        p.setBrush(Qt::NoBrush);
        for (auto& r : best_drop_rect_)
          p.drawRoundedRect(r, grid::Radius, grid::Radius);
        break;
      }
      case ShadowImg:
        if (!drop_pixmap_.isNull())
          p.drawPixmap(largeRect, drop_pixmap_, drop_pixmap_.rect());
        break;
      case PureColor: {
        QPainterPath path;
        path.addRoundedRect(largeRect, grid::Radius, grid::Radius);
        p.fillPath(path, QColor(0, 120, 215, 60));
        p.setPen(QPen(QColor(0, 120, 215), 2));
        p.drawPath(path);
        break;
      }
    }
  }
}

void WelcomeWidget::initUi() {
  setObjectName("WelcomeWidget");
  setAcceptDrops(true);

  tips_ << QStringLiteral("按 Ctrl+N 快速新建项目")
        << QStringLiteral("按 Ctrl+Shift+F 进行全局搜索")
        << QStringLiteral("在拓扑编辑器中双击设备可配置端口")
        << QStringLiteral("ICD 位视图支持逐位编辑信号定义")
        << QStringLiteral("测试用例支持 Lua 脚本编写")
        << QStringLiteral("右键单击最近项目可从列表中移除")
        << QStringLiteral("硬件面板显示当前连接的测试设备")
        << QStringLiteral("输出面板支持多级日志过滤")
        << QStringLiteral("协议编辑器支持导入标准 ICD 格式")
        << QStringLiteral("报告可导出为 PDF 格式")
        << QStringLiteral("按 Ctrl+Tab 快速切换编辑器标签页")
        << QStringLiteral("项目备份默认每 5 分钟自动保存")
        << QStringLiteral("终端面板支持 cmd、PowerShell、bash")
        << QStringLiteral("在设置中可切换暗色/亮色主题")
        << QStringLiteral("活动栏按钮支持自定义页面顺序");

  grid_layout_ = new grid::GridLayout(this);
  grid_layout_->setAutoLayout(true);
  grid_layout_->setMargin(24);

  // === Tile 1: 新建项目 (1x1) ===
  auto* newProjectContent = new QWidget;
  newProjectContent->setObjectName("WelcomeActionContent");
  auto* npLayout = new QVBoxLayout(newProjectContent);
  npLayout->setContentsMargins(0, 0, 0, 0);
  npLayout->setAlignment(Qt::AlignCenter);
  auto* npIcon = new QLabel("+");
  npIcon->setObjectName("WelcomeActionIcon");
  npIcon->setAlignment(Qt::AlignCenter);
  auto* npText = new QLabel(QStringLiteral("新建项目"));
  npText->setObjectName("WelcomeActionText");
  npText->setAlignment(Qt::AlignCenter);
  npLayout->addWidget(npIcon);
  npLayout->addWidget(npText);

  auto* newProjectTile = new grid::GridTile(grid::_1_1, this);
  newProjectTile->setObjectName("WelcomeTileAction");
  newProjectTile->setContentWidget(newProjectContent);
  newProjectTile->setNameText("");
  connect(newProjectTile, &grid::GridTile::clicked, this,
          &WelcomeWidget::newProjectRequested);
  grid_layout_->addWidget(newProjectTile);

  // === Tile 2: 打开项目 (1x1) ===
  auto* openProjectContent = new QWidget;
  openProjectContent->setObjectName("WelcomeActionContent");
  auto* opLayout = new QVBoxLayout(openProjectContent);
  opLayout->setContentsMargins(0, 0, 0, 0);
  opLayout->setAlignment(Qt::AlignCenter);
  auto* opIcon = new QLabel(QStringLiteral("\xF0\x9F\x93\x82"));
  opIcon->setObjectName("WelcomeActionIcon");
  opIcon->setAlignment(Qt::AlignCenter);
  auto* opText = new QLabel(QStringLiteral("打开项目"));
  opText->setObjectName("WelcomeActionText");
  opText->setAlignment(Qt::AlignCenter);
  opLayout->addWidget(opIcon);
  opLayout->addWidget(opText);

  auto* openProjectTile = new grid::GridTile(grid::_1_1, this);
  openProjectTile->setObjectName("WelcomeTileAction");
  openProjectTile->setContentWidget(openProjectContent);
  openProjectTile->setNameText("");
  connect(openProjectTile, &grid::GridTile::clicked, this,
          &WelcomeWidget::openProjectRequested);
  grid_layout_->addWidget(openProjectTile);

  // === Tile 3: 每日提示 (1x2) ===
  tip_tile_ = new grid::GridTile(grid::_1_2, this);
  tip_tile_->setObjectName("WelcomeTileTip");
  tip_tile_->setCursor(Qt::PointingHandCursor);
  {
    auto tip_content = new QLabel(tip_tile_);
    tip_content->setAlignment(Qt::AlignCenter);
    tip_content->setWordWrap(true);
    tip_content->setObjectName("WelcomeTipContent");
    tip_tile_->setContentWidget(tip_content);
  }
  tip_tile_->setNameText(QString());  // 隐藏底部 label
  connect(tip_tile_, &grid::GridTile::clicked, this,
          &WelcomeWidget::showRandomTip);
  grid_layout_->addWidget(tip_tile_);

  // === Tile 4: EyeWidget + Logo (2x2) ===
  eye_widget_ = new EyeWidget(this);
  eye_widget_->setObjectName("WelcomeEyeWidget");
  eye_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  eye_widget_->setMinimumSize(200, 40);
  if (ThemeManager::instance().isDarkTheme()) {
    eye_widget_->setOutlineColor(QColor(0xCC, 0xCC, 0xCC));
    eye_widget_->setPupilColor(QColor(0x2C, 0x2C, 0x2C));
    eye_widget_->setEyebrowColor(QColor(0x88, 0x88, 0x99));
  } else {
    eye_widget_->setOutlineColor(QColor(0xB0, 0xB0, 0xB0));
    eye_widget_->setPupilColor(QColor(0x44, 0x44, 0x44));
    eye_widget_->setEyebrowColor(QColor(0x77, 0x77, 0x77));
  }

  auto* eyeContent = new QWidget;
  eyeContent->setObjectName("WelcomeEyeContent");

  // Logo 顶栏 + EyeWidget 纵向堆叠
  auto* eyeMainLayout = new QVBoxLayout(eyeContent);
  eyeMainLayout->setContentsMargins(0, 0, 0, 0);
  eyeMainLayout->setSpacing(0);

  // 顶栏：Logo + 版本 + 图标
  auto* eyeTopBar = new QWidget;
  eyeTopBar->setObjectName("WelcomeEyeTopBar");
  auto* topBarLayout = new QHBoxLayout(eyeTopBar);
  topBarLayout->setContentsMargins(10, 2, 10, 2);
  topBarLayout->setSpacing(4);

  auto* eyeLogoLabel = new QLabel("ETest Demo");
  eyeLogoLabel->setObjectName("WelcomeEyeLogoLabel");
  auto* eyeVersionLabel = new QLabel(QString("v%1").arg(PROJECT_VERSION));
  eyeVersionLabel->setObjectName("WelcomeEyeVersionLabel");
  auto* eyeIconLabel = new QLabel;
  eyeIconLabel->setObjectName("WelcomeEyeIconLabel");
  eyeIconLabel->setFixedSize(20, 20);
  {
    QPixmap source(":/resources/icons/app_icon.svg");
    if (!source.isNull()) {
      source = source.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      QPixmap rounded(20, 20);
      rounded.fill(Qt::transparent);
      QPainter p2(&rounded);
      p2.setRenderHint(QPainter::Antialiasing);
      QPainterPath path;
      path.addEllipse(0, 0, 20, 20);
      p2.setClipPath(path);
      p2.drawPixmap(0, 0, source);
      p2.end();
      eyeIconLabel->setPixmap(rounded);
    } else {
      eyeIconLabel->setText("E");
      eyeIconLabel->setStyleSheet("font-size:12px; font-weight:bold; background:transparent;");
    }
  }

  topBarLayout->addWidget(eyeLogoLabel);
  topBarLayout->addWidget(eyeVersionLabel);
  topBarLayout->addStretch();
  topBarLayout->addWidget(eyeIconLabel);

  eyeMainLayout->addWidget(eyeTopBar);
  eyeMainLayout->addWidget(eye_widget_);

  auto* eyeTile = new grid::GridTile(grid::_2_2, this);
  eyeTile->setObjectName("WelcomeTileEye");
  eyeTile->setContentWidget(eyeContent);
  eyeTile->setNameText("");
  grid_layout_->addWidget(eyeTile);

  // === Tile 5: 时钟 (2x2) ===
  {
    auto* clockWidget = new PaintedClockWidget(this);
    clockWidget->setMinimumSize(100, 100);

    auto* clockTile = new grid::GridTile(grid::_2_2, this);
    clockTile->setObjectName("WelcomeTileClock");
    clockTile->setContentWidget(clockWidget);
    clockTile->setNameText("");
    grid_layout_->addWidget(clockTile);
  }

  // === Tile 6-9: 最近项目 (动态, 1x1 each) ===
  rebuildRecentTiles();
}

void WelcomeWidget::initSignals() {
  // 右键菜单：从列表中移除
  connect(&ConfigManager::instance(), &ConfigManager::configChanged, this,
          [this](const QString& key) {
            if (key == QString::fromLatin1(CONFIG_WELCOME_BG_IMAGE) ||
                key == QString::fromLatin1(CONFIG_WELCOME_BG_DIR) ||
                key == QString::fromLatin1(CONFIG_WELCOME_BG_MODE)) {
              loadBackground();
            }
          });
}

bool WelcomeWidget::eventFilter(QObject* obj, QEvent* event) {
  return QWidget::eventFilter(obj, event);
}

void WelcomeWidget::rebuildRecentTiles() {
  // 移除旧的最近项目磁贴
  for (auto tile : recent_tiles_) {
    grid_layout_->removeWidget(tile);
    tile->deleteLater();
  }
  recent_tiles_.clear();

  QStringList recentList =
      ConfigManager::instance().get<QStringList>(CONFIG_RECENT_PROJECT_LIST);
  QVariantMap timestamps = ConfigManager::instance().get<QVariantMap>(
      CONFIG_RECENT_PROJECT_TIMESTAMPS);

  for (const QString& path : recentList) {
    QFileInfo fi(path);
    QString displayName = fi.completeBaseName();
    if (displayName.isEmpty()) continue;

    auto* card = new QWidget;
    card->setObjectName("WelcomeProjectCardContent");
    card->setAttribute(Qt::WA_TranslucentBackground);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 6, 8, 6);
    cardLayout->setSpacing(2);

    auto* nameLabel = new QLabel(displayName);
    nameLabel->setObjectName("WelcomeCardName");
    nameLabel->setWordWrap(true);

    auto* pathLabel = new QLabel(fi.absolutePath());
    pathLabel->setObjectName("WelcomeCardPath");

    QString timeStr;
    if (timestamps.contains(path)) {
      QDateTime dt = timestamps[path].toDateTime();
      timeStr = dt.toString("yyyy-MM-dd hh:mm");
    }
    auto* timeLabel = new QLabel(timeStr);
    timeLabel->setObjectName("WelcomeCardTime");
    if (timeStr.isEmpty()) timeLabel->hide();

    cardLayout->addWidget(nameLabel);
    cardLayout->addWidget(pathLabel);
    cardLayout->addStretch();
    cardLayout->addWidget(timeLabel);

    auto tile = new grid::GridTile(grid::_1_1, this);
    tile->setObjectName("WelcomeTileRecent");
    tile->setContentWidget(card);
    tile->setNameText("");
    tile->setProperty("projectPath", path);
    tile->setCursor(Qt::PointingHandCursor);

    connect(tile, &grid::GridTile::clicked, this,
            [this, tile]() {
              QString p = tile->property("projectPath").toString();
              if (!p.isEmpty()) emit projectOpenRequested(p);
            });
    connect(tile, &grid::GridTile::contextMenuRequested, this,
            [this](const QPoint& pos, int) {
              auto* tile = qobject_cast<grid::GridTile*>(QObject::sender());
              if (!tile) return;
              QString path = tile->property("projectPath").toString();
              if (path.isEmpty()) return;
              auto* menu = new QMenu(this);
              menu->setObjectName("WelcomeContextMenu");
              auto* removeAction = menu->addAction(QStringLiteral("从列表中移除"));
              connect(removeAction, &QAction::triggered, this, [this, path]() {
                QStringList recentList = ConfigManager::instance().get<QStringList>(
                    CONFIG_RECENT_PROJECT_LIST);
                recentList.removeAll(path);
                ConfigManager::instance().set(CONFIG_RECENT_PROJECT_LIST, recentList);
                refreshRecentProjects();
              });
              menu->exec(pos);
            });

    recent_tiles_.append(tile);
    grid_layout_->addWidget(tile);
  }
}

void WelcomeWidget::refreshRecentProjects() { rebuildRecentTiles(); }

void WelcomeWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  loadBackground();
}

void WelcomeWidget::loadBackground() {
  auto& cm = ConfigManager::instance();
  bg_dir_path_ = cm.get<QString>(CONFIG_WELCOME_BG_DIR);
  bg_image_path_ = cm.get<QString>(CONFIG_WELCOME_BG_IMAGE);
  bg_mode_ = cm.get<int>(CONFIG_WELCOME_BG_MODE, 0);

  if (!bg_dir_path_.isEmpty()) {
    QDir dir(bg_dir_path_);
    QStringList entries =
        dir.entryList(image_filters_, QDir::Files, QDir::Name);
    if (!entries.isEmpty()) {
      int idx = QRandomGenerator::global()->bounded(entries.size());
      QString fullPath = dir.absoluteFilePath(entries[idx]);
      bg_pixmap_ = QPixmap(fullPath);
      if (bg_pixmap_.isNull()) bg_pixmap_.load(fullPath, "JPG");
    } else {
      bg_pixmap_ = QPixmap();
    }
  } else if (!bg_image_path_.isEmpty()) {
    bg_pixmap_ = QPixmap(bg_image_path_);
    if (bg_pixmap_.isNull()) bg_pixmap_.load(bg_image_path_, "JPG");
  } else {
    bg_pixmap_ = QPixmap();
  }
  update();
}

void WelcomeWidget::showRandomTip() {
  if (tips_.isEmpty()) return;
  int idx = QRandomGenerator::global()->bounded(tips_.size());

  // 将提示文字设到 content_widget_ 上（居中显示）
  auto tipContent = qobject_cast<QLabel*>(tip_tile_->contentWidget());
  if (tipContent) {
    tipContent->setText(QStringLiteral("%1  %2")
                            .arg(QString::fromUtf8("\xF0\x9F\x92\xA1"))
                            .arg(tips_[idx]));
  }
}

// ==================== 网格叠加层配置 ====================

void WelcomeWidget::setGridOverlayVisible(bool visible) {
  draw_grid_overlay_ = visible;
  update();
}
bool WelcomeWidget::gridOverlayVisible() const { return draw_grid_overlay_; }
void WelcomeWidget::setGridColor(const QColor& c) {
  grid_color_ = c;
  if (draw_grid_overlay_) update();
}
void WelcomeWidget::setOccupiedGridColor(const QColor& c) {
  occupied_grid_color_ = c;
  if (draw_grid_overlay_) update();
}
void WelcomeWidget::setDragPreviewStyle(DragPreviewStyle style) {
  drag_preview_style_ = style;
}
WelcomeWidget::DragPreviewStyle WelcomeWidget::dragPreviewStyle() const {
  return drag_preview_style_;
}

// ==================== 拖拽重排 ====================

grid::GridTile* WelcomeWidget::getTileUnderMouse(QWidget* child) const {
  if (!child) return nullptr;
  auto widget = child->parentWidget();
  if (auto tile = qobject_cast<grid::GridTile*>(widget)) {
    return (tile->parentWidget() == this) ? tile : nullptr;
  }
  if (widget == this) return nullptr;
  return getTileUnderMouse(widget);
}

void WelcomeWidget::mousePressEvent(QMouseEvent* event) {
  drag_start_pos_ = event->pos();
  QWidget::mousePressEvent(event);
}

void WelcomeWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!enable_drag_edit_) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  QPoint delta = event->pos() - drag_start_pos_;
  if (delta.manhattanLength() < QApplication::startDragDistance()) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  auto child = childAt(event->pos());
  if (!child) return;

  auto dragTile = getTileUnderMouse(child);
  if (!dragTile) return;
  // 不拖拽提示磁贴
  if (dragTile == tip_tile_) return;

  auto pixmap = dragTile->grab();

  QByteArray itemData;
  QDataStream dataStream(&itemData, QIODevice::WriteOnly);
  dataStream << etest::core::utils::PixmapUtil::grayOpacityImg(pixmap);

  auto* mimeData = new QMimeData;
  mimeData->setData(grid::MimeType, itemData);

  auto* drag = new QDrag(this);
  drag->setMimeData(mimeData);
  drag->setPixmap(pixmap);
  drag->setHotSpot(event->pos() - dragTile->pos());

  dragTile->setDragingState(true);
  drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
  dragTile->setDragingState(false);
  drag->deleteLater();
}

void WelcomeWidget::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasFormat(grid::MimeType)) {
    if (event->source() == this) {
      event->setDropAction(Qt::MoveAction);
      event->accept();
    } else {
      event->acceptProposedAction();
    }
  } else {
    event->ignore();
  }
}

void WelcomeWidget::dragMoveEvent(QDragMoveEvent* event) {
  if (!event->mimeData()->hasFormat(grid::MimeType)) {
    event->ignore();
    return;
  }

  auto dragItem = qobject_cast<grid::GridTile*>(event->source());
  if (!dragItem) return;

  QByteArray itemData = event->mimeData()->data(grid::MimeType);
  QDataStream dataStream(&itemData, QIODevice::ReadOnly);
  QPixmap pix;
  dataStream >> pix;

  auto trend = grid::getDragingTrend(drag_start_pos_, event->pos());
  best_drop_rect_ = grid_layout_->dealWithDragMove(dragItem, event->pos(), trend);
  drop_pixmap_ = pix;

  event->setDropAction(Qt::MoveAction);
  event->accept();
}

void WelcomeWidget::dropEvent(QDropEvent* event) {
  if (!event->mimeData()->hasFormat(grid::MimeType)) {
    event->ignore();
    return;
  }

  auto dragItem = qobject_cast<grid::GridTile*>(event->source());
  if (!dragItem) return;

  if (best_drop_rect_.isEmpty()) {
    dragItem->setDragingState(false);
    dragItem->posResetAnimation(this->rect(), event->pos(), dragItem->pos());
    grid_layout_->dropApplied(false);
  } else {
    dragItem->move(best_drop_rect_.first().toRect().topLeft());
    grid_layout_->dropApplied(true);
  }
  best_drop_rect_.clear();
  drop_pixmap_ = QPixmap();
  update();

  event->setDropAction(Qt::MoveAction);
  event->accept();
}

}  // namespace etest::app
