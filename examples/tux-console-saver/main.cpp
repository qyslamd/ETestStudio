#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QSlider>
#include <QVBoxLayout>

#include "TuxConsoleSaver.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("MobaXterm Tux Console Saver Demo"));
    resize(800, 500);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    saver_ = new TuxConsoleSaver(this);
    layout->addWidget(saver_, 1);

    // Control bar
    auto* ctrl = new QWidget(this);
    ctrl->setFixedHeight(36);
    ctrl->setStyleSheet(
        QStringLiteral("background:#1a1a1a; border-top:1px solid #333;"));
    auto* ctrlLayout = new QHBoxLayout(ctrl);
    ctrlLayout->setContentsMargins(10, 0, 10, 0);

    auto* label = new QLabel(QStringLiteral("空闲触发延迟:"), ctrl);
    label->setStyleSheet(QStringLiteral("color:#aaa; font:10pt Consolas;"));
    ctrlLayout->addWidget(label);

    auto* slider = new QSlider(Qt::Horizontal, ctrl);
    slider->setRange(2, 60);
    slider->setValue(5);
    slider->setFixedWidth(200);
    ctrlLayout->addWidget(slider);

    auto* valueLabel = new QLabel(QStringLiteral("5 秒"), ctrl);
    valueLabel->setStyleSheet(QStringLiteral("color:#aaa; font:10pt Consolas;"));
    ctrlLayout->addWidget(valueLabel);

    ctrlLayout->addStretch();
    layout->addWidget(ctrl);

    connect(slider, &QSlider::valueChanged, this, [this, valueLabel](int v) {
      saver_->setIdleThreshold(v);
      valueLabel->setText(QStringLiteral("%1 秒").arg(v));
    });
  }

 private:
  TuxConsoleSaver* saver_ = nullptr;
};

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("tux-console-saver"));

  MainWindow w;
  w.show();

  return app.exec();
}

#include "main.moc"
