#ifndef ETEST_APP_TERMINAL_PANEL_H_
#define ETEST_APP_TERMINAL_PANEL_H_

#include <QPlainTextEdit>
#include <QTextCharFormat>
#include <QVBoxLayout>
#include <QWidget>

#include "terminal/PtyProcess.h"
#include "terminal/VtParser.h"

namespace etest {
namespace app {

class TerminalPanel : public QWidget {
  Q_OBJECT

 public:
  explicit TerminalPanel(QWidget* parent = nullptr);
  ~TerminalPanel() override;

  void startShell(const QString& command = QString());
  void sendInput(const QByteArray& data);

 private:
  void initUi();
  void initSignals();

  // Screen buffer operations
  void flushToDisplay();
  void scrollBufferUp(int n);
  void scrollBufferDown(int n);

  // VtParser handlers
  void onText(const QString& str);
  void onSgr(const QVector<int>& params);
  void onCursorPosition(int row, int col);
  void onCursorUp(int n);
  void onCursorDown(int n);
  void onCursorForward(int n);
  void onCursorBack(int n);
  void onEraseDisplay(int mode);
  void onEraseLine(int mode);
  void onScrollUp(int n);
  void onScrollDown(int n);
  void onSetScrollRegion(int top, int bottom);

  // Process lifecycle
  void onProcessFinished(int exitCode);
  void restartShell();

 protected:
  void showEvent(QShowEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  core::terminal::PtyProcess* process_ = nullptr;
  core::terminal::VtParser* parser_ = nullptr;
  QPlainTextEdit* display_ = nullptr;

  struct Cell {
    QChar ch = QChar::Space;
    QColor fg = QColor("#CCCCCC");
    QColor bg = QColor("#1E1E1E");
    bool bold = false;
  };

  struct Line {
    QVector<Cell> cells;
    Line() = default;
    explicit Line(int cols) : cells(cols) {}
    void resize(int cols) { cells.resize(cols); }
    int length() const { return cells.size(); }
  };

  struct State {
    int cursorRow = 0;
    int cursorCol = 0;
    int cols = 80;
    int rows = 24;
    int scrollRegionTop = 0;
    int scrollRegionBottom = 23;
    QColor currentFg = QColor("#CCCCCC");
    QColor currentBg = QColor("#1E1E1E");
    bool currentBold = false;
    bool dirty = false;  // needs display refresh
  } state_;

  QVector<Line> screen_;
  QVector<Line> scrollback_;  // lines scrolled off the top
  static const int kMaxScrollback = 10000;
  bool shellExited_ = false;
  bool shell_started_ = false;
  int lastExitCode_ = 0;
  QTimer* resize_timer_ = nullptr;
};

}  // namespace app
}  // namespace etest

#endif  // ETEST_APP_TERMINAL_PANEL_H_
