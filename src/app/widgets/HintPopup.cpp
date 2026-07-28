#include "HintPopup.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include "HintMessageDelegate.h"
#include "MessageService.h"

namespace etest::app {

static constexpr int kPopupWidth = 360;
static constexpr int kPopupMaxHeight = 400;
static constexpr int kPopupMinHeight = 140;
// toolbar 32 + separator 1
static constexpr int kChromeHeight = 33;

HintPopup::HintPopup(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                 Qt::NoDropShadowWindowHint);
#ifdef Q_OS_WIN
  setAttribute(Qt::WA_TranslucentBackground);
#endif
  setAttribute(Qt::WA_NoMouseReplay);
  setFixedWidth(kPopupWidth);
  setFixedHeight(kPopupMinHeight);

  // 内容容器（QSS 绘制边框 + 圆角，替代 QGraphicsDropShadowEffect）
  auto* content = new QFrame(this);
  content->setObjectName(QStringLiteral("HintPopupContent"));

  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(content);

  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // 顶部工具栏
  auto* toolbar = new QFrame(content);
  toolbar->setObjectName(QStringLiteral("HintPopupToolbar"));
  toolbar->setFixedHeight(32);
  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(8, 0, 8, 0);
  toolbar_layout->addStretch();

  mark_all_btn_ = new QToolButton(toolbar);
  mark_all_btn_->setObjectName(QStringLiteral("HintPopupMarkAllBtn"));
  mark_all_btn_->setText(QStringLiteral("一键已读"));
  mark_all_btn_->setCursor(Qt::PointingHandCursor);
  mark_all_btn_->setAutoRaise(true);
  toolbar_layout->addWidget(mark_all_btn_);

  clear_btn_ = new QToolButton(toolbar);
  clear_btn_->setObjectName(QStringLiteral("HintPopupClearBtn"));
  clear_btn_->setText(QStringLiteral("清空"));
  clear_btn_->setCursor(Qt::PointingHandCursor);
  clear_btn_->setAutoRaise(true);
  toolbar_layout->addWidget(clear_btn_);

  layout->addWidget(toolbar);

  // 分割线
  auto* separator = new QFrame(content);
  separator->setObjectName(QStringLiteral("HintPopupSeparator"));
  separator->setFixedHeight(1);
  layout->addWidget(separator);

  // QListView
  list_view_ = new QListView(content);
  list_view_->setObjectName(QStringLiteral("HintPopupList"));
  list_view_->setModel(&MessageService::instance());
  list_view_->setSelectionMode(QAbstractItemView::NoSelection);
  list_view_->setMouseTracking(true);
  list_view_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  list_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_view_->setFrameShape(QFrame::NoFrame);

  delegate_ = new HintMessageDelegate(list_view_);
  list_view_->setItemDelegate(delegate_);
  // 安装事件过滤器以追踪 per-region hover
  list_view_->viewport()->installEventFilter(this);

  layout->addWidget(list_view_);

  // 空消息提示
  empty_label_ = new QLabel(content);
  empty_label_->setObjectName(QStringLiteral("HintPopupEmpty"));
  empty_label_->setAlignment(Qt::AlignCenter);
  empty_label_->setText(QStringLiteral("暂无消息"));
  empty_label_->hide();
  layout->addWidget(empty_label_);

  // 连接 delegate 信号 -> MessageService
  connect(delegate_, &HintMessageDelegate::actionTriggered, this,
          [](int row) { MessageService::instance().triggerAction(row); });
  connect(delegate_, &HintMessageDelegate::closeRequested, this,
          [this](int row) {
            MessageService::instance().removeAt(row);
            if (MessageService::instance().rowCount() == 0) {
              close();
            } else {
              refresh();
            }
          });

  // 连接工具栏按钮
  connect(mark_all_btn_, &QToolButton::clicked, this,
          [this]() { MessageService::instance().markAllRead(); });
  connect(clear_btn_, &QToolButton::clicked, this, [this]() {
    MessageService::instance().clearAll();
    close();
  });

  // 消息变化时刷新
  connect(&MessageService::instance(),
          &MessageService::rowsInserted, this,
          [this]() { refresh(); });
  connect(&MessageService::instance(),
          &MessageService::rowsRemoved, this,
          [this]() { refresh(); });
  connect(&MessageService::instance(),
          &MessageService::modelReset, this,
          [this]() { refresh(); });
}

bool HintPopup::eventFilter(QObject* obj, QEvent* event) {
  if (obj == list_view_->viewport()) {
    if (event->type() == QEvent::MouseMove) {
      auto* mouseEvent = static_cast<QMouseEvent*>(event);
      QModelIndex idx = list_view_->indexAt(mouseEvent->pos());
      int newRow = idx.isValid() ? idx.row() : -1;
      // 若行发生变化，刷新旧行以清除 hover 高亮
      if (newRow != last_hovered_row_ && last_hovered_row_ >= 0) {
        QModelIndex oldIdx =
            list_view_->model()->index(last_hovered_row_, 0);
        if (oldIdx.isValid()) {
          list_view_->update(oldIdx);
        }
      }
      if (idx.isValid()) {
        bool hasAction = idx.data(MessageService::HasActionRole).toBool();
        auto region = HintMessageDelegate::hitTest(
            list_view_->visualRect(idx), mouseEvent->pos(), hasAction);
        delegate_->setHoveredRegion(idx.row(), region);
        list_view_->update(idx);
      } else {
        delegate_->setHoveredRegion(-1, HintMessageDelegate::ClickRegion::None);
      }
      last_hovered_row_ = newRow;
    } else if (event->type() == QEvent::Leave) {
      // 鼠标离开 viewport 时清除 hover
      if (last_hovered_row_ >= 0) {
        QModelIndex oldIdx =
            list_view_->model()->index(last_hovered_row_, 0);
        if (oldIdx.isValid()) {
          list_view_->update(oldIdx);
        }
      }
      delegate_->setHoveredRegion(-1, HintMessageDelegate::ClickRegion::None);
      last_hovered_row_ = -1;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void HintPopup::showBelow(const QPoint& globalPos) {
  move(globalPos);
  refresh();
  show();
}

void HintPopup::refresh() {
  int count = MessageService::instance().rowCount();
  if (count == 0) {
    list_view_->hide();
    empty_label_->show();
    setFixedHeight(kPopupMinHeight);
  } else {
    empty_label_->hide();
    list_view_->show();
    int h = kChromeHeight + count * HintMessageDelegate::kItemHeight;
    setFixedHeight(qMin(h, kPopupMaxHeight));
  }
}

}  // namespace etest::app
