#include "WelcomeWidget.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QVBoxLayout>

#include "EyeWidget.h"
#include "common/ThemeManager.h"
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
  loadBackground();
  showRandomTip();
}

void WelcomeWidget::paintEvent(QPaintEvent* /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  if (!bg_pixmap_.isNull()) {
    switch (bg_mode_) {
      case 0:  // center
      {
        int x = (width() - bg_pixmap_.width()) / 2;
        int y = (height() - bg_pixmap_.height()) / 2;
        p.drawPixmap(x, y, bg_pixmap_);
        break;
      }
      case 1:  // tile
        p.drawTiledPixmap(rect(), bg_pixmap_);
        break;
      case 2:  // stretch
        p.drawPixmap(rect(), bg_pixmap_);
        break;
    }
  }
}

void WelcomeWidget::initUi() {
  setObjectName("WelcomeWidget");

  // 每日提示
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

  auto* outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(40, 40, 40, 40);

  // 居中容器，固定宽度
  center_widget_ = new QWidget(this);
  center_widget_->setFixedWidth(600);
  center_widget_->setObjectName("WelcomeCenter");

  auto* layout = new QVBoxLayout(center_widget_);
  layout->setContentsMargins(40, 40, 40, 40);
  layout->setSpacing(12);

  // 上下弹性空间使内容垂直居中
  layout->addStretch();

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
  if (ThemeManager::instance().isDarkTheme()) {
    eye_widget->setOutlineColor(QColor(0xCC, 0xCC, 0xCC));
    eye_widget->setPupilColor(QColor(0x2C, 0x2C, 0x2C));
    eye_widget->setEyebrowColor(QColor(0x88, 0x88, 0x99));
  } else {
    eye_widget->setOutlineColor(QColor(0xB0, 0xB0, 0xB0));
    eye_widget->setPupilColor(QColor(0x44, 0x44, 0x44));
    eye_widget->setEyebrowColor(QColor(0x77, 0x77, 0x77));
  }
  eye_row->addWidget(eye_widget);
  eye_row->addStretch();
  layout->addLayout(eye_row);

  // === 最近项目 ===
  auto* recentHeader = new QLabel(QStringLiteral("最近项目"), this);
  recentHeader->setObjectName("WelcomeSectionHeader");
  layout->addWidget(recentHeader);

  recent_scroll_ = new QScrollArea(this);
  recent_scroll_->setObjectName("WelcomeRecentScroll");
  recent_scroll_->setWidgetResizable(true);
  recent_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  recent_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  recent_scroll_->setMaximumHeight(240);

  recent_container_ = new QWidget(this);
  recent_container_->setObjectName("WelcomeRecentContainer");
  recent_container_->setContextMenuPolicy(Qt::CustomContextMenu);
  recent_scroll_->setWidget(recent_container_);

  layout->addWidget(recent_scroll_);

  // === 每日提示 ===
  tip_label_ = new QLabel(this);
  tip_label_->setObjectName("WelcomeTipLabel");
  tip_label_->setWordWrap(true);
  tip_label_->setCursor(Qt::PointingHandCursor);
  layout->addWidget(tip_label_);

  layout->addStretch();

  outerLayout->addStretch();

  auto* hCenter = new QHBoxLayout();
  hCenter->addStretch();
  hCenter->addWidget(center_widget_);
  hCenter->addStretch();
  outerLayout->addLayout(hCenter);

  outerLayout->addStretch();

  // 初始加载最近项目
  refreshRecentProjects();
}

void WelcomeWidget::initSignals() {
  connect(btn_new_project_, &QPushButton::clicked, this,
          &WelcomeWidget::newProjectRequested);
  connect(btn_open_project_, &QPushButton::clicked, this,
          &WelcomeWidget::openProjectRequested);

  // 卡片点击：事件过滤器由 rebuildRecentCards 安装
  // 右键菜单：从列表中移除
  connect(
      recent_container_, &QWidget::customContextMenuRequested, this,
      [this](const QPoint& pos) {
        auto* child = recent_container_->childAt(pos);
        if (!child || !child->property("projectPath").isValid())
          return;

        QString path = child->property("projectPath").toString();
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
        menu->exec(recent_container_->mapToGlobal(pos));
      });

  // 点击每日提示切换下一条
  tip_label_->installEventFilter(this);

  // 配置变更：背景图片/模式
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
  if (obj == tip_label_ && event->type() == QEvent::MouseButtonPress) {
    showRandomTip();
    return true;
  }
  // 卡片点击: 检查是否是我们的卡片
  if (event->type() == QEvent::MouseButtonPress) {
    auto* w = qobject_cast<QWidget*>(obj);
    if (w && w->property("projectPath").isValid()) {
      QString path = w->property("projectPath").toString();
      if (!path.isEmpty()) {
        emit projectOpenRequested(path);
        return true;
      }
    }
  }
  return QWidget::eventFilter(obj, event);
}

void WelcomeWidget::rebuildRecentCards() {
  // 清除旧卡片
  QLayout* grid = recent_container_->layout();
  if (grid) {
    QLayoutItem* item;
    while ((item = grid->takeAt(0)) != nullptr) {
      if (item->widget())
        item->widget()->deleteLater();
      delete item;
    }
    delete grid;
  }

  QStringList recentList =
      ConfigManager::instance().get<QStringList>(CONFIG_RECENT_PROJECT_LIST);

  // 获取时间戳
  QVariantMap timestamps = ConfigManager::instance().get<QVariantMap>(
      CONFIG_RECENT_PROJECT_TIMESTAMPS);

  if (recentList.isEmpty()) {
    auto* emptyLabel =
        new QLabel(QStringLiteral("暂无最近项目"), recent_container_);
    emptyLabel->setObjectName("WelcomeEmptyHint");
    auto* l = new QVBoxLayout(recent_container_);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(emptyLabel, 0, Qt::AlignCenter);
    return;
  }

  auto* gridLayout = new QGridLayout(recent_container_);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setSpacing(8);

  int cols = qMax(1, qMin(2, recentList.size()));
  int row = 0, col = 0;

  for (const QString& path : recentList) {
    QFileInfo fi(path);
    QString displayName = fi.completeBaseName();
    if (displayName.isEmpty())
      continue;

    auto* card = new QFrame(recent_container_);
    card->setObjectName("WelcomeProjectCard");
    card->setCursor(Qt::PointingHandCursor);
    card->setProperty("projectPath", path);
    card->installEventFilter(this);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(4);

    auto* nameLabel = new QLabel(displayName, card);
    nameLabel->setObjectName("WelcomeCardName");

    auto* pathLabel = new QLabel(fi.absolutePath(), card);
    pathLabel->setWordWrap(true);
    pathLabel->setObjectName("WelcomeCardPath");

    // 时间戳
    QString timeStr;
    if (timestamps.contains(path)) {
      QDateTime dt = timestamps[path].toDateTime();
      timeStr = dt.toString("yyyy-MM-dd hh:mm");
    }
    auto* timeLabel = new QLabel(timeStr, card);
    timeLabel->setObjectName("WelcomeCardTime");

    cardLayout->addWidget(nameLabel);
    cardLayout->addWidget(pathLabel);
    cardLayout->addWidget(timeLabel);

    gridLayout->addWidget(card, row, col);
    col++;
    if (col >= cols) {
      col = 0;
      row++;
    }
  }

  // 填充剩余空间
  if (recentList.size() <= cols) {
    gridLayout->setRowStretch(row, 1);
    gridLayout->setColumnStretch(cols, 1);
  }
}

void WelcomeWidget::refreshRecentProjects() {
  rebuildRecentCards();
}

void WelcomeWidget::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  // 每次显示时重新加载，确保目录模式随机选图
  loadBackground();
}

void WelcomeWidget::loadBackground() {
  auto& cm = ConfigManager::instance();
  bg_dir_path_ = cm.get<QString>(CONFIG_WELCOME_BG_DIR);
  bg_image_path_ = cm.get<QString>(CONFIG_WELCOME_BG_IMAGE);
  bg_mode_ = cm.get<int>(CONFIG_WELCOME_BG_MODE, 0);

  if (!bg_dir_path_.isEmpty()) {
    // 目录模式：随机选一张图
    QDir dir(bg_dir_path_);
    QStringList entries =
        dir.entryList(image_filters_, QDir::Files, QDir::Name);
    if (!entries.isEmpty()) {
      int idx = QRandomGenerator::global()->bounded(entries.size());
      QString fullPath = dir.absoluteFilePath(entries[idx]);
      bg_pixmap_ = QPixmap(fullPath);
      if (bg_pixmap_.isNull())
        bg_pixmap_.load(fullPath, "JPG");
    } else {
      bg_pixmap_ = QPixmap();
    }
  } else if (!bg_image_path_.isEmpty()) {
    bg_pixmap_ = QPixmap(bg_image_path_);
    if (bg_pixmap_.isNull())
      bg_pixmap_.load(bg_image_path_, "JPG");
  } else {
    bg_pixmap_ = QPixmap();
  }

  update();
}

void WelcomeWidget::showRandomTip() {
  if (tips_.isEmpty())
    return;
  int idx = QRandomGenerator::global()->bounded(tips_.size());
  tip_label_->setText(QStringLiteral("\xE2\x96\xB6 %1").arg(tips_[idx]));
}

}  // namespace etest::app
