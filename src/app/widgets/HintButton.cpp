#include "HintButton.h"

#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QGuiApplication>

#include "AppIconProvider.h"
#include "HintPopup.h"
#include "MessageService.h"
#include "ThemeManager.h"

namespace etest::app {

HintButton::HintButton(QWidget* parent) : QToolButton(parent) {
  setObjectName(QStringLiteral("HintButton"));
  setToolTip(QStringLiteral("消息"));
  setAutoRaise(true);
  setIconSize(QSize(16, 16));

  popup_ = new HintPopup(parent);

  reloadIcon();

  // 点击切换 popup 显隐
  // HintPopup 设了 WA_NoMouseReplay，popup 可见时点击按钮不会触发 clicked
  connect(this, &QToolButton::clicked, this, [this]() {
    if (popup_->isVisible()) {
      return;
    }
    // 先刷新以获取准确高度，再做边界计算
    popup_->refresh();
    // 按钮左下角作为 popup 默认左上角
    QPoint pos = mapToGlobal(QPoint(0, height()));
    // 屏幕边界补偿：右侧/底部溢出时调整
    if (auto* screen = QGuiApplication::screenAt(pos)) {
      QRect avail = screen->availableGeometry();
      if (pos.x() + popup_->width() > avail.right()) {
        pos.setX(avail.right() - popup_->width());
      }
      if (pos.y() + popup_->height() > avail.bottom()) {
        pos.setY(mapToGlobal(QPoint(0, 0)).y() - popup_->height());
      }
    }
    popup_->showBelow(pos);
  });

  // 未读数变化 -> 切换图标
  connect(&MessageService::instance(),
          &MessageService::unreadCountChanged, this,
          [this]() { reloadIcon(); });

  // 主题切换 -> 刷新图标
  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this,
          [this]() { reloadIcon(); });
}

void HintButton::reloadIcon() {
  bool hasUnread = MessageService::instance().unreadCount() > 0;
  QString name =
      hasUnread ? QStringLiteral("message_alert") : QStringLiteral("message");
  setIcon(etest::core_ui::AppIconProvider::instance().icon(name));
}

}  // namespace etest::app
