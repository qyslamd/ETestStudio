#ifndef ETEST_APP_PROBLEMS_PANEL_H_
#define ETEST_APP_PROBLEMS_PANEL_H_

#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace etest::app {

class ProblemsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit ProblemsPanel(QWidget* parent = nullptr);

  void clearProblems();
  void addProblem(const QString& source, const QString& type,
                  const QString& message);
  void showSummary(int errors, int warnings);

 private:
  void initUi();

  QTableWidget* table_;
};

}  // namespace etest::app

#endif  // ETEST_APP_PROBLEMS_PANEL_H_
