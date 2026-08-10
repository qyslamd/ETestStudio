#include "WelcomeV2Widget.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QToolButton>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "core_ui/AppIconProvider.h"
#include "logger/Logger.h"
#include "version.h"
#include "widgets/RecentProjectCard.h"

namespace etest::app {

using namespace etest::core::config;
using etest::core_ui::AppIconProvider;

namespace {

struct QuickCreateInfo {
  QString iconName;
  QString title;
  QString categoryId;
  QString extension;
  QString baseName;
};

const QuickCreateInfo kQuickCreate[] = {
    {QStringLiteral("file_eproto"), QStringLiteral("协议文件"),
     QStringLiteral("protocol"), QStringLiteral("eprotox"),
     QStringLiteral("新建协议文件")},
    {QStringLiteral("file_etopo"), QStringLiteral("拓扑文件"),
     QStringLiteral("topology"), QStringLiteral("etopo"),
     QStringLiteral("新建拓扑文件")},
    {QStringLiteral("testprogram"), QStringLiteral("测试程序"),
     QStringLiteral("testprog"), QStringLiteral("etprog"),
     QStringLiteral("新建测试程序")},
};

}  // namespace

WelcomeV2Widget::WelcomeV2Widget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
  loadBackground();
  showNextTip();
}

void WelcomeV2Widget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  if (!bg_pixmap_.isNull()) {
    switch (bg_mode_) {
      case 1:
        p.drawTiledPixmap(rect(), bg_pixmap_);
        break;
      case 2:
        p.drawPixmap(rect(), bg_pixmap_);
        break;
      default: {
        int x = (width() - bg_pixmap_.width()) / 2;
        int y = (height() - bg_pixmap_.height()) / 2;
        p.drawPixmap(x, y, bg_pixmap_);
      }
    }
  }
}

void WelcomeV2Widget::initUi() {
  setObjectName(QStringLiteral("welcomeV2"));
  // Fluent Mica 卡片：外层留边距让圆角卡片悬浮于欢迎画布（背景图透过卡片显示）
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(28, 28, 28, 28);

  auto* card = new QFrame(this);
  card->setObjectName(QStringLiteral("welcomeCard"));
  card->setFrameShape(QFrame::NoFrame);
  auto* cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(32, 26, 32, 20);
  cardLayout->setSpacing(16);
  layout->addWidget(card, 1);

  auto* inner = cardLayout;

  // ── 头部品牌区 ──
  auto* header = new QHBoxLayout();
  header->setSpacing(12);
  auto* icon = new QLabel(this);
  icon->setObjectName(QStringLiteral("welcomeBrandIcon"));
  icon->setFixedSize(44, 44);
  icon->setPixmap(AppIconProvider::instance()
                      .icon(QStringLiteral("welcome"))
                      .pixmap(24, 24));
  header->addWidget(icon);
  auto* titleGroup = new QVBoxLayout();
  titleGroup->setSpacing(2);
  auto* title = new QLabel(QStringLiteral("ETest 测试系统"), this);
  title->setObjectName(QStringLiteral("welcomeBrandTitle"));
  titleGroup->addWidget(title);
  auto* sub = new QLabel(QStringLiteral("自动化测试 · 一站式"), this);
  sub->setObjectName(QStringLiteral("welcomeBrandSub"));
  titleGroup->addWidget(sub);
  header->addLayout(titleGroup);
  header->addStretch();
  auto* version = new QLabel(QStringLiteral("v%1").arg(PROJECT_VERSION), this);
  version->setObjectName(QStringLiteral("welcomeVersion"));
  header->addWidget(version);
  inner->addLayout(header);

  // ── 双栏内容 ──
  auto* content = new QHBoxLayout();
  content->setSpacing(28);
  auto* left = new QVBoxLayout();
  left->setSpacing(8);
  left->addWidget(makeStartSection());
  left->addSpacing(8);
  left->addWidget(makeRecentSection());
  auto* right = new QVBoxLayout();
  right->setSpacing(8);
  right->addWidget(makeQuickCreateSection());
  right->addSpacing(8);
  right->addWidget(makeGuideSection());
  content->addLayout(left);
  content->addLayout(right);
  inner->addLayout(content, 1);

  // ── 底部状态条 ──
  auto* footer = new QHBoxLayout();
  footer->setSpacing(8);
  tip_label_ = new QLabel(this);
  tip_label_->setObjectName(QStringLiteral("welcomeTip"));
  footer->addWidget(tip_label_, 1);
  auto* tipBtn = new QToolButton(this);
  tipBtn->setObjectName(QStringLiteral("welcomeTipBtn"));
  tipBtn->setText(QStringLiteral("换一条"));
  tipBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(tipBtn, &QToolButton::clicked, this, [this](bool) { showNextTip(); });
  footer->addWidget(tipBtn);
  auto* settingsBtn = new QToolButton(this);
  settingsBtn->setObjectName(QStringLiteral("welcomeTipBtn"));
  settingsBtn->setText(QStringLiteral("设置"));
  settingsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
  connect(settingsBtn, &QToolButton::clicked, this,
          [this](bool) { emit settingsRequested(); });
  footer->addWidget(settingsBtn);
  inner->addLayout(footer);
}

void WelcomeV2Widget::initSignals() {
  // 背景配置变化时重载（与 v1 同源，即时生效）
  auto& cfg = ConfigManager::instance();
  connect(&cfg, &ConfigManager::configChanged, this,
          [this](const QString& key) {
            if (key == QString::fromLatin1(CONFIG_WELCOME_BG_DIR) ||
                key == QString::fromLatin1(CONFIG_WELCOME_BG_IMAGE) ||
                key == QString::fromLatin1(CONFIG_WELCOME_BG_MODE)) {
              loadBackground();
            }
          });
}

QWidget* WelcomeV2Widget::makeStartSection() {
  auto* section = new QWidget(this);
  auto* lay = new QVBoxLayout(section);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(10);
  auto* title = new QLabel(QStringLiteral("开始"), section);
  title->setObjectName(QStringLiteral("welcomeSectionTitle"));
  lay->addWidget(title);

  auto* row = new QHBoxLayout();
  row->setSpacing(12);
  auto* newBtn = new QPushButton(QStringLiteral("新建项目"), section);
  newBtn->setObjectName(QStringLiteral("welcomePrimaryBtn"));
  newBtn->setIcon(AppIconProvider::instance().icon(QStringLiteral("plus")));
  newBtn->setCursor(Qt::PointingHandCursor);
  connect(newBtn, &QPushButton::clicked, this,
          [this](bool) { emit newProjectRequested(); });
  auto* openBtn = new QPushButton(QStringLiteral("打开项目"), section);
  openBtn->setObjectName(QStringLiteral("welcomeSecondaryBtn"));
  openBtn->setIcon(AppIconProvider::instance().icon(QStringLiteral("folder")));
  openBtn->setCursor(Qt::PointingHandCursor);
  connect(openBtn, &QPushButton::clicked, this,
          [this](bool) { emit openProjectRequested(); });
  row->addWidget(newBtn, 1);
  row->addWidget(openBtn, 1);
  lay->addLayout(row);
  return section;
}

QWidget* WelcomeV2Widget::makeRecentSection() {
  auto* section = new QWidget(this);
  auto* lay = new QVBoxLayout(section);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(8);
  auto* title = new QLabel(QStringLiteral("最近项目"), section);
  title->setObjectName(QStringLiteral("welcomeSectionTitle"));
  lay->addWidget(title);

  auto* listWidget = new QWidget(section);
  auto* listLay = new QVBoxLayout(listWidget);
  listLay->setContentsMargins(0, 0, 0, 0);
  listLay->setSpacing(6);
  recent_layout_ = listLay;
  recent_empty_ = new QLabel(QStringLiteral("暂无最近项目"), listWidget);
  recent_empty_->setObjectName(QStringLiteral("welcomeRecentEmpty"));
  recent_empty_->setAlignment(Qt::AlignCenter);
  listLay->addWidget(recent_empty_);
  listLay->addStretch();
  lay->addWidget(listWidget, 1);
  return section;
}

QWidget* WelcomeV2Widget::makeQuickCreateSection() {
  auto* section = new QWidget(this);
  auto* lay = new QVBoxLayout(section);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(10);
  auto* title = new QLabel(QStringLiteral("快速新建"), section);
  title->setObjectName(QStringLiteral("welcomeSectionTitle"));
  lay->addWidget(title);

  auto* grid = new QHBoxLayout();
  grid->setSpacing(10);
  for (const QuickCreateInfo& info : kQuickCreate) {
    auto* btn = new QPushButton(info.title, section);
    btn->setObjectName(QStringLiteral("welcomeQuickBtn"));
    btn->setIcon(AppIconProvider::instance().icon(info.iconName));
    btn->setCursor(Qt::PointingHandCursor);
    connect(btn, &QPushButton::clicked, this, [this, info](bool) {
      emit createFileRequested(info.categoryId, info.extension, info.baseName);
    });
    grid->addWidget(btn, 1);
  }
  lay->addLayout(grid);
  return section;
}

QWidget* WelcomeV2Widget::makeGuideSection() {
  auto* section = new QWidget(this);
  auto* lay = new QVBoxLayout(section);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(8);
  auto* title = new QLabel(QStringLiteral("入门指南"), section);
  title->setObjectName(QStringLiteral("welcomeSectionTitle"));
  lay->addWidget(title);

  const QStringList steps = {
      QStringLiteral("新建项目"),   QStringLiteral("定义拓扑"),
      QStringLiteral("编写协议"),   QStringLiteral("生成测试程序"),
      QStringLiteral("运行与报告"),
  };
  for (int i = 0; i < steps.size(); ++i) {
    auto* step = new QWidget(section);
    step->setObjectName(QStringLiteral("welcomeGuideStep"));
    auto* stepLay = new QHBoxLayout(step);
    stepLay->setContentsMargins(0, 2, 0, 2);
    stepLay->setSpacing(10);
    auto* num = new QLabel(QString::number(i + 1), step);
    num->setObjectName(QStringLiteral("welcomeGuideNum"));
    num->setFixedSize(24, 24);
    num->setAlignment(Qt::AlignCenter);
    stepLay->addWidget(num);
    auto* text = new QLabel(steps[i], step);
    text->setObjectName(QStringLiteral("welcomeGuideText"));
    stepLay->addWidget(text);
    lay->addWidget(step);
  }
  lay->addStretch();
  return section;
}

void WelcomeV2Widget::refreshRecentProjects() {
  rebuildRecentList();
}

void WelcomeV2Widget::rebuildRecentList() {
  while (QLayoutItem* item = recent_layout_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      if (w != recent_empty_) {
        w->deleteLater();
      }
    }
    delete item;
  }
  // 空态标签常驻列表首项（不被 clear 销毁），列表为空时显示
  recent_layout_->addWidget(recent_empty_);

  auto& cfg = ConfigManager::instance();
  const QStringList recentList =
      cfg.get<QStringList>(CONFIG_RECENT_PROJECT_LIST);
  const QVariantMap timestamps =
      cfg.get<QVariantMap>(CONFIG_RECENT_PROJECT_TIMESTAMPS);

  for (const QString& path : recentList) {
    QFileInfo fi(path);
    const QString displayName = fi.completeBaseName();
    if (displayName.isEmpty()) {
      continue;
    }
    QString timeStr;
    if (timestamps.contains(path)) {
      const QDateTime dt = timestamps[path].toDateTime();
      timeStr = dt.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    }
    auto* card =
        new RecentProjectCard(path, displayName, fi.absolutePath(), timeStr);
    connect(card, &RecentProjectCard::openRequested, this,
            &WelcomeV2Widget::projectOpenRequested);
    connect(card, &RecentProjectCard::removeRequested, this,
            [this](const QString& p) {
              auto& c = ConfigManager::instance();
              QStringList list = c.get<QStringList>(CONFIG_RECENT_PROJECT_LIST);
              list.removeAll(p);
              c.set(CONFIG_RECENT_PROJECT_LIST, list);
              rebuildRecentList();
            });
    recent_layout_->addWidget(card);
  }
  recent_layout_->addStretch();
  recent_empty_->setVisible(recentList.isEmpty());
}

void WelcomeV2Widget::showNextTip() {
  if (tips_.isEmpty()) {
    tips_ << QStringLiteral("按 Ctrl+N 快速新建项目")
          << QStringLiteral("按 Ctrl+Shift+F 进行全局搜索")
          << QStringLiteral("在拓扑编辑器中双击设备可配置端口")
          << QStringLiteral("测试用例支持 Lua 脚本编写")
          << QStringLiteral("输出面板支持多级日志过滤")
          << QStringLiteral("报告可导出为 PDF 格式");
  }
  tip_index_ = (tip_index_ + 1) % tips_.size();
  if (tip_label_) {
    tip_label_->setText(tips_[tip_index_]);
  }
}

void WelcomeV2Widget::loadBackground() {
  auto& cfg = ConfigManager::instance();
  bg_dir_path_ = cfg.get<QString>(CONFIG_WELCOME_BG_DIR);
  bg_image_path_ = cfg.get<QString>(CONFIG_WELCOME_BG_IMAGE);
  bg_mode_ = cfg.get<int>(CONFIG_WELCOME_BG_MODE, 0);

  if (!bg_dir_path_.isEmpty()) {
    QDir dir(bg_dir_path_);
    const QStringList entries =
        dir.entryList(image_filters_, QDir::Files, QDir::Name);
    if (!entries.isEmpty()) {
      const int idx = QRandomGenerator::global()->bounded(entries.size());
      const QString fullPath = dir.absoluteFilePath(entries[idx]);
      bg_pixmap_ = QPixmap(fullPath);
      if (bg_pixmap_.isNull()) {
        bg_pixmap_.load(fullPath, "JPG");
      }
    } else {
      bg_pixmap_ = QPixmap();
    }
  } else if (!bg_image_path_.isEmpty()) {
    bg_pixmap_ = QPixmap(bg_image_path_);
  } else {
    bg_pixmap_ = QPixmap();
  }
  update();
}

}  // namespace etest::app
