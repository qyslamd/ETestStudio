#include "SidebarNavWidget.h"

#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "ThemeManager.h"
#include "logger/Logger.h"

namespace etest::app {

using etest::core_ui::AppIconProvider;
using etest::core_ui::ThemeManager;

SidebarNavWidget::SidebarNavWidget(QWidget* parent) : QWidget(parent) {
  initUi();

  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &SidebarNavWidget::reloadIcons);
}

void SidebarNavWidget::initUi() {
  setFixedWidth(48);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 4, 0, 4);
  layout->setSpacing(0);

  top_layout_ = new QVBoxLayout();
  top_layout_->setSpacing(4);
  top_layout_->setContentsMargins(0, 0, 0, 0);

  layout->addLayout(top_layout_);
  layout->addStretch();
}

void SidebarNavWidget::addPage(const QString& id,
                               const QString& tooltip,
                               const QString& iconName) {
  // 不重复添加相同 ID 的按钮
  for (const auto& p : pages_) {
    if (p.id == id)
      return;
  }

  pages_.append({id, iconName, tooltip});

  auto* btn = createButton(tooltip);
  btn->setIcon(AppIconProvider::instance().icon(iconName));
  btn->setIconSize(QSize(kNormalIconSize, kNormalIconSize));
  buttons_.append(btn);
  top_layout_->addWidget(btn);

  connect(btn, &QAbstractButton::clicked, this, [this, id]() {
    LOG_INFO("PROJECT_UI", "侧边栏导航 [page={}]", id.toStdString());
    emit pageClicked(id);
  });

  // 默认选中第一个添加的页面
  if (buttons_.size() == 1) {
    setActivePageId(id);
  }
}

void SidebarNavWidget::reloadIcons() {
  for (int i = 0; i < buttons_.size(); ++i) {
    buttons_[i]->setIcon(AppIconProvider::instance().icon(pages_[i].iconName));
  }
  updateActiveIconSize();
}

void SidebarNavWidget::setLoginState(bool /*loggedIn*/,
                                     const QString& /*userName*/,
                                     const QString& /*role*/) {
  // 登录状态展示已迁移到 QAB 登录菜单，此处保留为空操作
}

void SidebarNavWidget::setLoginActive(bool /*active*/) {
  // 已迁移到 QAB
}

void SidebarNavWidget::setSettingsActive(bool /*active*/) {
  // 已迁移到 QAB
}

void SidebarNavWidget::updateActiveIconSize() {
  for (int i = 0; i < buttons_.size(); ++i) {
    bool active = (pages_[i].id == active_page_id_);
    buttons_[i]->setIconSize(active ? QSize(kActiveIconSize, kActiveIconSize)
                                    : QSize(kNormalIconSize, kNormalIconSize));
  }
}

QToolButton* SidebarNavWidget::createButton(const QString& tooltip) {
  auto* btn = new QToolButton(this);
  btn->setToolTip(tooltip);
  btn->setFixedSize(48, 40);
  btn->setCheckable(true);
  btn->setAutoRaise(true);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setObjectName(QStringLiteral("sidebarNavBtn"));
  btn->installEventFilter(this);
  return btn;
}

void SidebarNavWidget::setActivePageId(const QString& id) {
  active_page_id_ = id;
  for (int i = 0; i < pages_.size(); ++i) {
    buttons_[i]->setChecked(pages_[i].id == id);
  }
  updateActiveIconSize();
}

void SidebarNavWidget::clearActivePage() {
  active_page_id_.clear();
  for (auto* btn : buttons_) {
    btn->setChecked(false);
    btn->setIconSize(QSize(kNormalIconSize, kNormalIconSize));
  }
}

QString SidebarNavWidget::activePageId() const {
  return active_page_id_;
}

bool SidebarNavWidget::eventFilter(QObject* obj, QEvent* event) {
  auto* btn = qobject_cast<QToolButton*>(obj);
  if (!btn)
    return QWidget::eventFilter(obj, event);

  if (event->type() == QEvent::HoverEnter) {
    btn->setIconSize(QSize(kActiveIconSize, kActiveIconSize));
    return true;
  }

  if (event->type() == QEvent::HoverLeave) {
    // 检查是否为页面按钮（根据 active 状态恢复）
    int idx = buttons_.indexOf(btn);
    if (idx >= 0 && pages_[idx].id == active_page_id_) {
      btn->setIconSize(QSize(kActiveIconSize, kActiveIconSize));
    } else {
      btn->setIconSize(QSize(kNormalIconSize, kNormalIconSize));
    }
    return true;
  }

  return QWidget::eventFilter(obj, event);
}

}  // namespace etest::app
