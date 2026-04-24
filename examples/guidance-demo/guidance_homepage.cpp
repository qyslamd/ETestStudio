#include "guidance_homepage.h"
#include <QApplication>
#include <QByteArray>
#include <QEasingCurve>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QtDebug>
#include "guidance_config.h"
#include "guidance_controller.h"
#include "ui_guidance_homepage.h"
#include "utility_qt.h"
#include "window_mover.h"

GuidanceHomePage::GuidanceHomePage(GuidanceController* controller,
                                   QWidget* parent)
    : QWidget(parent),
      ui(new Ui::GuidanceHomePage),
      model_(new QStandardItemModel(this)),
      controller_(controller) {
  ui->setupUi(this);
  initUi();
  initSignals();
}

GuidanceHomePage::~GuidanceHomePage() {
  delete ui;
  model_->clear();
}

void GuidanceHomePage::initUi() {
  setWindowFlag(Qt::Popup);
  setWindowFlag(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);

  widget_ = ui->widget;
  mover_ = new WindowMover(widget_, this);

  utility_qt::createGraphicsShadow(widget_);

  ui->listView->setModel(model_);
  ui->listView->setViewMode(QListView::IconMode);
  ui->listView->setIconSize(QSize(64, 64));
  ui->listView->setGridSize(QSize(128, 128));

  ui->labelTitle->setText("选择一个教程");
  ui->labelDesc->setText("点击左侧列表，选择一个教程进行查看。");
  ui->btnGo->setEnabled(false);
}

void GuidanceHomePage::initSignals() {
  connect(ui->btnClose, &QPushButton::clicked, this,
          &GuidanceHomePage::actHideAnimation);
  connect(ui->btnAll, &QPushButton::clicked, this,
          &GuidanceHomePage::onButtonAllClicked);
  connect(ui->btnGo, &QPushButton::clicked, this,
          &GuidanceHomePage::onButtonGoClicked);
  connect(ui->listView, &QAbstractItemView::clicked, this,
          &GuidanceHomePage::onListViewItemClicked);
}

void GuidanceHomePage::paintEvent(QPaintEvent* event) {
  return QWidget::paintEvent(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.save();
  p.fillRect(this->rect(), QColor(205, 205, 205, 170));
  p.restore();
}

void GuidanceHomePage::showEvent(QShowEvent* event) {
  actShowAnimation();

  model_->clear();
  controller_->loadFlowToListModel(model_);
}

void GuidanceHomePage::mousePressEvent(QMouseEvent* event) {
  setAttribute(Qt::WA_NoMouseReplay);
  if (this->childAt(event->pos())) {
    return QWidget::mousePressEvent(event);
  }

  actHideAnimation();
}

void GuidanceHomePage::actShowAnimation() {
  if (!widget_) {
    return;
  }
  auto centerX = this->width() / 2;
  auto centerY = this->height() / 2;
  auto w = widget_->width();
  auto h = widget_->height();
  QPoint p1;

  switch ((quint32)QRandomGenerator::global()->generate() % 4) {
    case 0:
      p1 = QPoint(-w, centerY - h / 2);  // 左中
      break;
    case 1:
      p1 = QPoint(centerX - w / 2, -h);  // 上中
      break;
    case 2:
      p1 = QPoint(this->width(), centerY - h / 2);  // 右中
      break;
    case 3:
      p1 = QPoint(centerX - w / 2, this->height());  //下中
      break;
    default:
      break;
  }

  auto anime = new QPropertyAnimation(this);
  anime->setEasingCurve(QEasingCurve::OutQuint);
  anime->setTargetObject(widget_);
  anime->setPropertyName("pos");
  anime->setDuration(500);
  anime->setStartValue(p1);
  anime->setEndValue(QPoint(centerX - w / 2, centerY - h / 2));

  connect(anime, &QVariantAnimation::finished, anime, &QObject::deleteLater);
  anime->start();
}

void GuidanceHomePage::actHideAnimation() {
  if (!widget_) {
    return;
  }
  auto centerX = this->width() / 2;
  auto centerY = this->height() / 2;
  auto w = widget_->width();
  auto h = widget_->height();

  QPoint p2;
  switch ((quint32)QRandomGenerator::global()->generate() % 4) {
    case 0:
      p2 = QPoint(-w, centerY - h / 2);  // 左中
      break;
    case 1:
      p2 = QPoint(centerX - w / 2, -h);  // 上中
      break;
    case 2:
      p2 = QPoint(this->width(), centerY - h / 2);  // 右中
      break;
    case 3:
      p2 = QPoint(centerX - w / 2, this->height());  //下中
      break;
    default:
      break;
  }

  auto anime = new QPropertyAnimation(this);
  anime->setEasingCurve(QEasingCurve::OutQuint);
  anime->setTargetObject(widget_);
  anime->setPropertyName("pos");
  anime->setDuration(500);
  anime->setStartValue(widget_->pos());
  anime->setEndValue(p2);
  connect(anime, &QPropertyAnimation::finished, this, [=] {
    emit windowHided();
    hide();
  });
  connect(anime, &QVariantAnimation::finished, anime, &QObject::deleteLater);
  anime->start();
}

void GuidanceHomePage::onListViewItemClicked(const QModelIndex& index) {
  bool canPlay = index.isValid();
  ui->btnGo->setEnabled(canPlay);

  // 解析数据到右侧显示出来
  auto data = model_->data(index, Qt::UserRole + 1).value<GuidanceFlow*>();
  if (data) {
    ui->labelTitle->setText(data->name());
    ui->labelDesc->setText(data->description());
  }
}

void GuidanceHomePage::onButtonAllClicked() {
  if (model_->rowCount() <= 0) {
    return;
  }
  hide();
  controller_->startAll(ui->checkBoxAuto->isChecked());
  connect(controller_, &GuidanceController::finished, this, [this] {
    QTimer::singleShot(0, this, [this] {
      controller_->startAll(ui->checkBoxAuto->isChecked());
    });
  });
}

void GuidanceHomePage::onButtonGoClicked() {
  auto index = ui->listView->currentIndex();
  if (!index.isValid()) {
    return;
  }
  auto data = model_->data(index, Qt::UserRole + 1).value<GuidanceFlow*>();

  hide();
  controller_->startOne(data, ui->checkBoxAuto->isChecked());
}
