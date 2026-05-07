#ifndef ETEST_APP_SEARCH_WIDGET_H_
#define ETEST_APP_SEARCH_WIDGET_H_

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

namespace etest::app {

class SearchWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SearchWidget(QWidget* parent = nullptr);

  void setSearchRoot(const QString& path);
  QString searchRoot() const;
  void setFocusOnSearchInput();

 Q_SIGNALS:
  void fileOpenRequested(const QString& filePath, int line);

 private:
  void initUi();
  void initSignals();

  void performSearch();
  void clearResults();

  QLineEdit* search_input_ = nullptr;
  QPushButton* search_button_ = nullptr;
  QCheckBox* case_sensitive_check_ = nullptr;
  QTreeWidget* result_tree_ = nullptr;
  QLabel* status_label_ = nullptr;

  QString search_root_;

  static const int kMaxResults = 1000;
};

}  // namespace etest::app

#endif  // ETEST_APP_SEARCH_WIDGET_H_
