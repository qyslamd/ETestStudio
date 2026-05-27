#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "PenguinWidget.h"

// ============================================================================
//  DemoWindow — main window with a stage area for the penguin
// ============================================================================

class DemoWindow : public QWidget {
  Q_OBJECT
 public:
  explicit DemoWindow(QWidget* parent = nullptr)
      : QWidget(parent) {
    setWindowTitle(QStringLiteral("MobaXterm 风格企鹅 Demo"));
    resize(520, 420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    // ---- Top: description ----
    auto* header = new QWidget(this);
    auto* hdrLayout = new QVBoxLayout(header);
    hdrLayout->setContentsMargins(20, 16, 20, 8);

    auto* title = new QLabel(QStringLiteral("桌面企鹅"), header);
    title->setStyleSheet(QStringLiteral("font-size:18px; font-weight:bold; color:#E0E0E0;"));
    title->setAlignment(Qt::AlignCenter);
    hdrLayout->addWidget(title);

    auto* desc = new QLabel(
        QStringLiteral("空闲一段时间后，一只小企鹅会在窗口内出现散步。\n"
                       "点击企鹅它会害羞地跑开。"),
        header);
    desc->setStyleSheet(QStringLiteral("font-size:12px; color:#888;"));
    desc->setAlignment(Qt::AlignCenter);
    hdrLayout->addWidget(desc);
    root->addWidget(header);

    // ---- Center: stage ----
    stage_ = new QFrame(this);
    stage_->setObjectName(QStringLiteral("penguinStage"));
    stage_->setStyleSheet(
        QStringLiteral("#penguinStage {"
                       "  background-color: #1E1E2E;"
                       "  border: 1px solid #3A3A4A;"
                       "  border-radius: 8px;"
                       "}"));
    stage_->setMinimumHeight(240);
    root->addWidget(stage_, 1);

    // ---- Bottom: controls ----
    auto* controls = new QWidget(this);
    auto* ctrlLayout = new QHBoxLayout(controls);
    ctrlLayout->setContentsMargins(20, 8, 20, 16);

    auto* sliderLabel =
        new QLabel(QStringLiteral("空闲触发:"), controls);
    sliderLabel->setStyleSheet(QStringLiteral("color:#AAA; font-size:13px;"));
    ctrlLayout->addWidget(sliderLabel);

    auto* slider = new QSlider(Qt::Horizontal, controls);
    slider->setRange(2, 30);
    slider->setValue(idleThreshold_);
    slider->setFixedWidth(160);
    slider->setStyleSheet(
        QStringLiteral("QSlider::groove:horizontal { height:4px; background:#3A3A4A;"
                       " border-radius:2px; }"
                       "QSlider::handle:horizontal { background:#0E639C; width:14px;"
                       " height:14px; margin:-5px 0; border-radius:7px; }"));
    ctrlLayout->addWidget(slider);

    auto* sliderValue =
        new QLabel(QStringLiteral("%1 秒").arg(idleThreshold_), controls);
    sliderValue->setStyleSheet(QStringLiteral("color:#AAA; font-size:13px;"));
    sliderValue->setFixedWidth(50);
    ctrlLayout->addWidget(sliderValue);

    connect(slider, &QSlider::valueChanged, this,
            [this, sliderValue](int v) {
              idleThreshold_ = v;
              sliderValue->setText(QStringLiteral("%1 秒").arg(v));
            });

    ctrlLayout->addStretch();

    statusLabel_ = new QLabel(QStringLiteral("等待操作..."), controls);
    statusLabel_->setStyleSheet(QStringLiteral("color:#666; font-size:12px;"));
    ctrlLayout->addWidget(statusLabel_);

    root->addWidget(controls);

    // ---- Idle detection ----
    lastActive_ = QDateTime::currentMSecsSinceEpoch();
    auto* idleTimer = new QTimer(this);
    connect(idleTimer, &QTimer::timeout, this, &DemoWindow::checkIdle);
    idleTimer->start(1000);

    qApp->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override {
    switch (event->type()) {
      case QEvent::MouseMove:
      case QEvent::MouseButtonPress:
      case QEvent::KeyPress:
      case QEvent::Wheel:
        lastActive_ = QDateTime::currentMSecsSinceEpoch();
        break;
      default:
        break;
    }
    return QWidget::eventFilter(obj, event);
  }

 private slots:
  void checkIdle() {
    if (penguin_) {
      statusLabel_->setText(
          QStringLiteral("企鹅 %1").arg(penguin_->stateName()));
    } else {
      statusLabel_->setText(QStringLiteral("等待操作..."));
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastActive_ > idleThreshold_ * 1000 && !penguin_) {
      spawnPenguin();
    }
  }

 private:
  void spawnPenguin() {
    if (penguin_) return;
    penguin_ = new PenguinWidget(stage_);
    connect(penguin_, &PenguinWidget::dismissed, this, [this]() {
      penguin_ = nullptr;
    });
    penguin_->appear();
    statusLabel_->setText(
        QStringLiteral("企鹅 %1").arg(penguin_->stateName()));
  }

  qint64 lastActive_ = 0;
  int idleThreshold_ = 5;
  QFrame* stage_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  PenguinWidget* penguin_ = nullptr;
};

// ============================================================================

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("penguin-demo"));

  DemoWindow w;
  w.show();

  return app.exec();
}

#include "main.moc"
