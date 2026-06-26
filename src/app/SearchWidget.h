#ifndef ETEST_APP_SEARCH_WIDGET_H_
#define ETEST_APP_SEARCH_WIDGET_H_

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
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

 signals:
  void fileOpenRequested(const QString& filePath, int line);

 private:
  enum class MatchMode { Normal, Regex };

  void initUi();
  void initSignals();

  void performSearch();
  void clearResults();
  QString fileIconName(const QString& suffix) const;
  QString buildRegexPattern(const QString& query) const;
  void onMatchModeToggled();

  QLineEdit* search_input_ = nullptr;
  QPushButton* search_button_ = nullptr;
  QToolButton* case_sensitive_btn_ = nullptr;
  QToolButton* whole_word_btn_ = nullptr;
  QToolButton* regex_mode_btn_ = nullptr;
  QSplitter* splitter_ = nullptr;
  QListWidget* file_list_ = nullptr;
  QTreeWidget* result_tree_ = nullptr;
  QLabel* status_label_ = nullptr;

  QString search_root_;
  MatchMode match_mode_ = MatchMode::Normal;

  static const int kMaxResults = 1000;
  static const int kMaxFileResults = 200;
};

}  // namespace etest::app

#endif  // ETEST_APP_SEARCH_WIDGET_H_
