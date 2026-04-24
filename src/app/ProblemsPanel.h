#ifndef ETEST_APP_PROBLEMS_PANEL_H_
#define ETEST_APP_PROBLEMS_PANEL_H_

#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace etest {
namespace app {

class ProblemsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit ProblemsPanel(QWidget* parent = nullptr);

 private:
  void setupUi();

  QTableWidget* table_;
};

}  // namespace app
}  // namespace etest

#endif  // ETEST_APP_PROBLEMS_PANEL_H_
