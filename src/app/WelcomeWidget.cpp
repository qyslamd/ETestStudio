#include "WelcomeWidget.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QVBoxLayout>

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "EyeWidget.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "project/ProjectManager.h"
#include "version.h"

namespace etest::app {

using namespace core::config;
using namespace core::project;

WelcomeWidget::WelcomeWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void WelcomeWidget::initUi() {
  setObjectName("WelcomeWidget");

  auto* outerLayout = new QHBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);

  // 居中容器，固定宽度
  auto* centerWidget = new QWidget(this);
  centerWidget->setFixedWidth(600);
  centerWidget->setObjectName("WelcomeCenter");

  auto* layout = new QVBoxLayout(centerWidget);
  layout->setContentsMargins(40, 40, 40, 40);
  layout->setSpacing(12);

  // === 标题行：Logo + 图标 ===
  auto* titleRow = new QWidget(this);
  auto* titleLayout = new QHBoxLayout(titleRow);
  titleLayout->setContentsMargins(0, 0, 0, 0);

  auto* logoLabel = new QLabel("ETest Demo", this);
  logoLabel->setObjectName("WelcomeLogo");
  logoLabel->setAlignment(Qt::AlignCenter);

  auto* iconLabel = new QLabel(this);
  iconLabel->setObjectName("WelcomeAppIcon");
  iconLabel->setFixedSize(48, 48);

  QPixmap source(":/resources/icons/app_icon.svg");
  if (!source.isNull()) {
    source =
        source.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap rounded(48, 48);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, 48, 48);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, source);
    painter.end();
    iconLabel->setPixmap(rounded);
  }

  titleLayout->addStretch();
  titleLayout->addWidget(logoLabel);
  titleLayout->addSpacing(12);
  titleLayout->addWidget(iconLabel);
  titleLayout->addStretch();

  layout->addWidget(titleRow);

  // === 版本 ===
  auto* versionLabel = new QLabel(QString("v%1").arg(PROJECT_VERSION), this);
  versionLabel->setObjectName("WelcomeVersion");
  versionLabel->setAlignment(Qt::AlignCenter);

  layout->addWidget(versionLabel);

  // === 快捷操作按钮 ===
  auto* btnLayout = new QHBoxLayout();
  btnLayout->setSpacing(12);

  btn_new_project_ = new QPushButton(QStringLiteral("新建项目"), this);
  btn_new_project_->setObjectName("WelcomeActionButton");
  btn_new_project_->setFixedHeight(36);
  btn_new_project_->setCursor(Qt::PointingHandCursor);

  btn_open_project_ = new QPushButton(QStringLiteral("打开项目"), this);
  btn_open_project_->setObjectName("WelcomeActionButton");
  btn_open_project_->setFixedHeight(36);
  btn_open_project_->setCursor(Qt::PointingHandCursor);

  btnLayout->addStretch();
  btnLayout->addWidget(btn_new_project_);
  btnLayout->addWidget(btn_open_project_);
  btnLayout->addStretch();

  layout->addLayout(btnLayout);

  // 眼睛互动（全局鼠标追踪）
  auto* eye_row = new QHBoxLayout();
  eye_row->addStretch();
  auto* eye_widget = new EyeWidget(this);
  eye_widget->setFixedSize(240, 120);
  eye_row->addWidget(eye_widget);
  eye_row->addStretch();
  layout->addLayout(eye_row);

  // === 最近项目 ===
  auto* recentHeader = new QLabel(QStringLiteral("最近项目"), this);
  recentHeader->setObjectName("WelcomeSectionHeader");
  layout->addWidget(recentHeader);

  recent_list_ = new QListWidget(this);
  recent_list_->setObjectName("WelcomeRecentList");
  recent_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  recent_list_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  recent_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  recent_list_->setMaximumHeight(200);
  layout->addWidget(recent_list_);

  // === 常用快捷键 ===
  auto* shortcutHeader = new QLabel(QStringLiteral("常用快捷键"), this);
  shortcutHeader->setObjectName("WelcomeSectionHeader");
  layout->addWidget(shortcutHeader);

  struct ShortcutItem {
    QString key;
    QString desc;
  };

  QList<ShortcutItem> shortcuts = {
      {"Ctrl+N", QStringLiteral("新建项目")},
      {"Ctrl+O", QStringLiteral("打开项目")},
      {"Ctrl+S", QStringLiteral("保存文件")},
      {"Ctrl+W", QStringLiteral("关闭文件")},
      {"Ctrl+Shift+F", QStringLiteral("全局搜索")},
      {"Ctrl+P", QStringLiteral("关闭项目")},
  };

  for (const auto& item : shortcuts) {
    auto* row = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 2, 0, 2);

    auto* keyLabel = new QLabel(item.key, row);
    keyLabel->setObjectName("WelcomeShortcutKey");
    keyLabel->setFixedWidth(120);

    auto* descLabel = new QLabel(item.desc, row);
    descLabel->setObjectName("WelcomeShortcutDesc");

    rowLayout->addWidget(keyLabel);
    rowLayout->addWidget(descLabel);
    rowLayout->addStretch();

    layout->addWidget(row);
  }

  layout->addStretch();

  outerLayout->addStretch();
  outerLayout->addWidget(centerWidget);
  outerLayout->addStretch();

  // 初始加载最近项目
  refreshRecentProjects();
}

void WelcomeWidget::initSignals() {
  connect(btn_new_project_, &QPushButton::clicked, this,
          &WelcomeWidget::newProjectRequested);
  connect(btn_open_project_, &QPushButton::clicked, this,
          &WelcomeWidget::openProjectRequested);

  connect(recent_list_, &QListWidget::itemClicked, this,
          [this](QListWidgetItem* item) {
            QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty()) {
              emit projectOpenRequested(path);
            }
          });

  // 右键菜单：从列表移除
  connect(
      recent_list_, &QListWidget::customContextMenuRequested, this,
      [this](const QPoint& pos) {
        auto* item = recent_list_->itemAt(pos);
        if (!item)
          return;

        auto* menu = new QMenu(this);
        menu->setObjectName("WelcomeContextMenu");
        auto* removeAction = menu->addAction(QStringLiteral("从列表中移除"));
        connect(removeAction, &QAction::triggered, this, [item, this]() {
          QString path = item->data(Qt::UserRole).toString();
          QStringList recentList = ConfigManager::instance().get<QStringList>(
              CONFIG_RECENT_PROJECT_LIST);
          recentList.removeAll(path);
          ConfigManager::instance().set(CONFIG_RECENT_PROJECT_LIST, recentList);
          refreshRecentProjects();
        });
        menu->exec(recent_list_->mapToGlobal(pos));
      });
}

void WelcomeWidget::refreshRecentProjects() {
  recent_list_->clear();

  QStringList recentList =
      ConfigManager::instance().get<QStringList>(CONFIG_RECENT_PROJECT_LIST);

  for (const QString& path : recentList) {
    QFileInfo fi(path);
    QString displayName = fi.completeBaseName();
    if (displayName.isEmpty())
      continue;

    auto* item = new QListWidgetItem(
        QString("%1  —  %2").arg(displayName, fi.absolutePath()), recent_list_);
    item->setData(Qt::UserRole, path);
    item->setToolTip(path);
  }

  if (recent_list_->count() == 0) {
    auto* emptyItem =
        new QListWidgetItem(QStringLiteral("暂无最近项目"), recent_list_);
    emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsEnabled);
  }
}

}  // namespace etest::app
