#pragma once

#include <QQueue>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;

namespace etest::app {

class HintBarWidget : public QWidget {
  Q_OBJECT
 public:
  struct HintData {
    QString text;
    QString actionLabel;
    std::function<void()> action;
  };

  explicit HintBarWidget(QWidget* parent = nullptr);

  void postHint(const QString& text,
                const QString& actionLabel = QString(),
                std::function<void()> action = nullptr);
  void clearAll();

 private:
  static constexpr int kMaxVisible = 3;
  static constexpr int kItemHeight = 30;

  struct HintEntry {
    HintData data;
    QWidget* container;
  };

  QList<HintEntry> active_hints_;
  QQueue<HintData> pending_queue_;

  QWidget* createHintItem(const HintData& data);
  void dismissHint(int index);
  void rebuild();
};

}  // namespace etest::app
