#include <QApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "PenguinOverlay.h"

// ============================================================================
//  DemoWindow – a minimal window that spawns penguins when idle
// ============================================================================

class DemoWindow : public QWidget {
  Q_OBJECT
 public:
  explicit DemoWindow(QWidget* parent = nullptr) : QWidget(parent) {
    setWindowTitle(QStringLiteral("ETest 企鹅屏保 Demo"));
    resize(500, 350);

    // Simple instructions in the middle
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel(
        QStringLiteral("<h1>🐧 企鹅屏保</h1>"
                       "<p style='font-size:14px; color:#666;'>"
                       "5 秒无操作 → 企鹅出现<br>"
                       "鼠标碰到企鹅 → 企鹅逃跑<br>"
                       "拖动滑块 → 调触发时间</p>"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* slider_layout = new QHBoxLayout;
    slider_layout->setAlignment(Qt::AlignCenter);

    auto* label = new QLabel(QStringLiteral("触发延迟:"));
    slider_layout->addWidget(label);

    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(2, 30);
    slider->setValue(idleThreshold_);
    slider->setFixedWidth(200);
    slider_layout->addWidget(slider);

    auto* value_label = new QLabel(
        QStringLiteral("%1 秒").arg(idleThreshold_));
    slider_layout->addWidget(value_label);

    layout->addLayout(slider_layout);

    connect(slider, &QSlider::valueChanged, this, [this, value_label](int v) {
      idleThreshold_ = v;
      value_label->setText(QStringLiteral("%1 秒").arg(v));
    });

    // Poll idle every second
    lastActive_ = QDateTime::currentMSecsSinceEpoch();
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &DemoWindow::checkIdle);
    timer->start(1000);

    // Reset idle on any input
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
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 idleMs = now - lastActive_;

    // Remove finished penguins
    penguins_.erase(
        std::remove_if(penguins_.begin(), penguins_.end(),
                       [](PenguinOverlay* p) { return p == nullptr; }),
        penguins_.end());

    if (idleMs > idleThreshold_ * 1000 && penguins_.size() < 3) {
      bool fromLeft = (penguins_.size() % 2 == 0);
      auto* p = new PenguinOverlay(fromLeft);
      p->show();
      penguins_.append(p);
      connect(p, &PenguinOverlay::done, this, [this, p]() {
        auto it = std::find(penguins_.begin(), penguins_.end(), p);
        if (it != penguins_.end()) *it = nullptr;
      });
      // Reset idle so we don't spawn them all at once
      lastActive_ = now;
    }
  }

 private:
  qint64 lastActive_ = 0;
  int idleThreshold_ = 5;
  QVector<PenguinOverlay*> penguins_;
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
