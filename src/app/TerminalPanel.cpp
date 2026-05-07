#include "TerminalPanel.h"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBlock>
#include <QTimer>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"

namespace etest {
namespace app {

using namespace core::config;
using namespace core::terminal;

TerminalPanel::TerminalPanel(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

TerminalPanel::~TerminalPanel() {
  if (process_) {
    process_->terminate();
    delete process_;
  }
}

void TerminalPanel::initUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  display_ = new QPlainTextEdit(this);
  display_->setReadOnly(false);
  display_->setFont(QFont("Consolas", 11));
  display_->setLineWrapMode(QPlainTextEdit::NoWrap);
  display_->setCenterOnScroll(false);

  QPalette pal = display_->palette();
  pal.setColor(QPalette::Base, QColor("#1E1E1E"));
  pal.setColor(QPalette::Text, QColor("#CCCCCC"));
  pal.setColor(QPalette::Highlight, QColor("#264F78"));
  pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
  display_->setPalette(pal);
  display_->viewport()->setPalette(pal);

  // Set visible cursor color
  display_->setCursorWidth(8);  // block-like cursor width
  QTextCursor cur = display_->textCursor();
  cur.setVisualNavigation(true);

  // Install event filter to capture keyboard input on the display widget
  display_->installEventFilter(this);
  display_->viewport()->installEventFilter(this);

  layout->addWidget(display_);

  resize_timer_ = new QTimer(this);
  resize_timer_->setSingleShot(true);
  resize_timer_->setInterval(100);

  // Initialize screen buffer
  screen_.resize(state_.rows);
  for (auto& line : screen_) {
    line = Line(state_.cols);
  }
}

void TerminalPanel::initSignals() {
  connect(resize_timer_, &QTimer::timeout, this, [this]() {
    if (!process_ || !process_->isRunning()) return;

    QFontMetrics fm(display_->font());
    int charWidth = fm.horizontalAdvance('M');
    int charHeight = fm.lineSpacing();
    if (charWidth <= 0 || charHeight <= 0) return;

    int cols = display_->viewport()->width() / charWidth;
    int rows = display_->viewport()->height() / charHeight;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    if (cols != state_.cols || rows != state_.rows) {
      state_.cols = cols;
      state_.rows = rows;
      state_.scrollRegionBottom = rows - 1;

      // Resize screen buffer
      screen_.resize(rows);
      for (auto& line : screen_) {
        line.resize(cols);
      }

      process_->resize(cols, rows);
      state_.dirty = true;
      flushToDisplay();
    }
  });
}

void TerminalPanel::startShell(const QString& command) {
  if (process_) {
    process_->terminate();
    delete process_;
  }
  if (parser_) {
    delete parser_;
    parser_ = nullptr;
  }

  // Reset state
  state_.cursorRow = 0;
  state_.cursorCol = 0;
  state_.currentFg = QColor("#CCCCCC");
  state_.currentBg = QColor("#1E1E1E");
  state_.currentBold = false;
  state_.dirty = false;
  scrollback_.clear();
  shellExited_ = false;
  lastExitCode_ = 0;

  QFontMetrics fm(display_->font());
  int charWidth = fm.horizontalAdvance('M');
  int charHeight = fm.lineSpacing();
  int cols = display_->viewport()->width() / qMax(charWidth, 1);
  int rows = display_->viewport()->height() / qMax(charHeight, 1);
  if (cols < 1) cols = 80;
  if (rows < 1) rows = 24;
  state_.cols = cols;
  state_.rows = rows;
  state_.scrollRegionTop = 0;
  state_.scrollRegionBottom = rows - 1;

  // Init screen buffer
  screen_.clear();
  screen_.resize(rows);
  for (auto& line : screen_) {
    line = Line(cols);
  }

  process_ = PtyProcess::create(this);
  parser_ = new VtParser(this);

  connect(process_, &PtyProcess::readyRead, this, [this]() {
    if (!process_) return;
    QByteArray data = process_->readAll();
    if (!data.isEmpty() && parser_) {
      parser_->parse(data);
      if (state_.dirty) {
        flushToDisplay();
        state_.dirty = false;
      }
    }
  });

  connect(parser_, &VtParser::text, this, &TerminalPanel::onText);
  connect(parser_, &VtParser::sgr, this, &TerminalPanel::onSgr);
  connect(parser_, &VtParser::cursorPosition, this,
          &TerminalPanel::onCursorPosition);
  connect(parser_, &VtParser::cursorUp, this, &TerminalPanel::onCursorUp);
  connect(parser_, &VtParser::cursorDown, this, &TerminalPanel::onCursorDown);
  connect(parser_, &VtParser::cursorForward, this,
          &TerminalPanel::onCursorForward);
  connect(parser_, &VtParser::cursorBack, this, &TerminalPanel::onCursorBack);
  connect(parser_, &VtParser::eraseDisplay, this,
          &TerminalPanel::onEraseDisplay);
  connect(parser_, &VtParser::eraseLine, this, &TerminalPanel::onEraseLine);
  connect(parser_, &VtParser::scrollUp, this, &TerminalPanel::onScrollUp);
  connect(parser_, &VtParser::scrollDown, this, &TerminalPanel::onScrollDown);
  connect(parser_, &VtParser::setScrollRegion, this,
          &TerminalPanel::onSetScrollRegion);

  connect(process_, &PtyProcess::processFinished, this,
          &TerminalPanel::onProcessFinished);

  QString cmd =
      command.isEmpty()
          ? ConfigManager::instance().get<QString>(CONFIG_TERMINAL_SHELL,
                                                    CONFIG_TERMINAL_DEFAULT_SHELL)
          : command;

  if (!process_->start(cmd, cols, rows)) {
    display_->setPlainText(QStringLiteral("Failed to start terminal."));
  } else {
    display_->setFocus();
  }
}

void TerminalPanel::sendInput(const QByteArray& data) {
  if (process_ && process_->isRunning()) {
    process_->write(data);
  }
}

void TerminalPanel::flushToDisplay() {
  // Check if user is scrolled up viewing history
  QScrollBar* vbar = display_->verticalScrollBar();
  bool userScrolledUp = vbar->value() < vbar->maximum();

  // Build the display: scrollback + screen buffer
  QString text;
  int totalLines = scrollback_.size() + screen_.size();

  // Scrollback lines
  for (int r = 0; r < scrollback_.size(); ++r) {
    const auto& line = scrollback_[r];
    int lastChar = -1;
    for (int c = line.length() - 1; c >= 0; --c) {
      if (line.cells[c].ch != QChar::Space) {
        lastChar = c;
        break;
      }
    }
    for (int c = 0; c <= lastChar; ++c) {
      text.append(line.cells[c].ch);
    }
    text.append('\n');
  }

  // Screen lines
  for (int r = 0; r < screen_.size(); ++r) {
    const auto& line = screen_[r];
    int minLen = (r == state_.cursorRow) ? state_.cursorCol + 1 : 0;
    int lastChar = -1;
    for (int c = line.length() - 1; c >= minLen; --c) {
      if (line.cells[c].ch != QChar::Space) {
        lastChar = c;
        break;
      }
    }
    int lineEnd = qMax(lastChar, minLen - 1);
    for (int c = 0; c <= lineEnd; ++c) {
      text.append(line.cells[c].ch);
    }
    if (r < screen_.size() - 1) {
      text.append('\n');
    }
  }

  // Replace document content
  QTextCursor cursor(display_->document());
  cursor.select(QTextCursor::Document);
  cursor.insertText(text);

  // Position cursor at terminal location (scrollback offset + cursorRow)
  QTextCursor textCursor(display_->document());
  textCursor.movePosition(QTextCursor::Start);
  int targetRow = scrollback_.size() + state_.cursorRow;
  for (int r = 0; r < targetRow; ++r) {
    textCursor.movePosition(QTextCursor::Down);
  }
  QTextBlock block = textCursor.block();
  int lineLen = block.text().length();
  int col = qMin(state_.cursorCol, lineLen);
  textCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col);
  display_->setTextCursor(textCursor);

  // Auto-scroll to bottom unless user is viewing history
  if (!userScrolledUp) {
    display_->ensureCursorVisible();
  }
}

void TerminalPanel::scrollBufferUp(int n) {
  for (int i = 0; i < n; ++i) {
    if (state_.scrollRegionTop < screen_.size()) {
      // Save scrolled-off line to scrollback
      if (state_.scrollRegionTop == 0) {
        scrollback_.append(screen_[0]);
        if (scrollback_.size() > kMaxScrollback) {
          scrollback_.removeFirst();
        }
      }
      screen_.remove(state_.scrollRegionTop);
      screen_.insert(state_.scrollRegionBottom, Line(state_.cols));
    }
  }
}

void TerminalPanel::scrollBufferDown(int n) {
  for (int i = 0; i < n; ++i) {
    if (state_.scrollRegionBottom < screen_.size()) {
      screen_.remove(state_.scrollRegionBottom);
      screen_.insert(state_.scrollRegionTop, Line(state_.cols));
    }
  }
}

void TerminalPanel::onText(const QString& str) {
  state_.dirty = true;

  for (int i = 0; i < str.size(); ++i) {
    QChar ch = str[i];

    if (ch == '\r') {
      state_.cursorCol = 0;
      continue;
    }

    if (ch == '\n') {
      state_.cursorRow++;
      if (state_.cursorRow > state_.scrollRegionBottom) {
        state_.cursorRow = state_.scrollRegionBottom;
        scrollBufferUp(1);
      }
      continue;
    }

    if (ch == '\b' || ch == QChar(0x7F)) {
      if (state_.cursorCol > 0) {
        state_.cursorCol--;
      }
      continue;
    }

    if (ch == '\t') {
      int nextTab = ((state_.cursorCol / 8) + 1) * 8;
      if (nextTab >= state_.cols) nextTab = state_.cols - 1;
      for (int c = state_.cursorCol; c < nextTab; ++c) {
        if (state_.cursorRow < screen_.size() && c < screen_[state_.cursorRow].length()) {
          auto& cell = screen_[state_.cursorRow].cells[c];
          cell.ch = QChar::Space;
          cell.fg = state_.currentFg;
          cell.bg = state_.currentBg;
          cell.bold = state_.currentBold;
        }
      }
      state_.cursorCol = nextTab;
      continue;
    }

    // Regular character (including Unicode/multi-byte)
    if (state_.cursorRow < screen_.size() && state_.cursorCol < screen_[state_.cursorRow].length()) {
      auto& cell = screen_[state_.cursorRow].cells[state_.cursorCol];
      cell.ch = ch;
      cell.fg = state_.currentFg;
      cell.bg = state_.currentBg;
      cell.bold = state_.currentBold;
    }

    state_.cursorCol++;
    if (state_.cursorCol >= state_.cols) {
      state_.cursorCol = 0;
      state_.cursorRow++;
      if (state_.cursorRow > state_.scrollRegionBottom) {
        state_.cursorRow = state_.scrollRegionBottom;
        scrollBufferUp(1);
      }
    }
  }
}

void TerminalPanel::onSgr(const QVector<int>& params) {
  static const QColor kAnsiColors[] = {
      QColor("#1E1E1E"), QColor("#CD3131"), QColor("#0DBC79"),
      QColor("#E5E510"), QColor("#2472C8"), QColor("#BC3FBC"),
      QColor("#11A8CD"), QColor("#CCCCCC"), QColor("#666666"),
      QColor("#F14C4C"), QColor("#23D18B"), QColor("#F5F543"),
      QColor("#3B8EEA"), QColor("#D670D6"), QColor("#29B8DB"),
      QColor("#E5E5E5"),
  };

  for (int i = 0; i < params.size(); ++i) {
    int p = params[i];

    if (p == 0) {
      state_.currentFg = QColor("#CCCCCC");
      state_.currentBg = QColor("#1E1E1E");
      state_.currentBold = false;
    } else if (p == 1) {
      state_.currentBold = true;
    } else if (p == 22) {
      state_.currentBold = false;
    } else if (p >= 30 && p <= 37) {
      state_.currentFg = kAnsiColors[p - 30];
    } else if (p == 38) {
      if (i + 1 < params.size() && params[i + 1] == 2) {
        if (i + 4 < params.size()) {
          state_.currentFg = QColor(params[i + 2], params[i + 3], params[i + 4]);
          i += 4;
        }
      } else if (i + 1 < params.size() && params[i + 1] == 5) {
        if (i + 2 < params.size()) {
          int idx = params[i + 2];
          if (idx >= 0 && idx < 16)
            state_.currentFg = kAnsiColors[idx];
          else if (idx >= 16 && idx < 232) {
            int v = idx - 16;
            state_.currentFg = QColor((v / 36) * 51, ((v % 36) / 6) * 51, (v % 6) * 51);
          } else if (idx >= 232 && idx < 256) {
            int gray = 8 + (idx - 232) * 10;
            state_.currentFg = QColor(gray, gray, gray);
          }
          i += 2;
        }
      }
    } else if (p == 39) {
      state_.currentFg = QColor("#CCCCCC");
    } else if (p >= 40 && p <= 47) {
      state_.currentBg = kAnsiColors[p - 40];
    } else if (p == 48) {
      if (i + 1 < params.size() && params[i + 1] == 2) {
        if (i + 4 < params.size()) {
          state_.currentBg = QColor(params[i + 2], params[i + 3], params[i + 4]);
          i += 4;
        }
      } else if (i + 1 < params.size() && params[i + 1] == 5) {
        if (i + 2 < params.size()) {
          int idx = params[i + 2];
          if (idx >= 0 && idx < 16)
            state_.currentBg = kAnsiColors[idx];
          else if (idx >= 232 && idx < 256) {
            int gray = 8 + (idx - 232) * 10;
            state_.currentBg = QColor(gray, gray, gray);
          }
          i += 2;
        }
      }
    } else if (p == 49) {
      state_.currentBg = QColor("#1E1E1E");
    }
  }
}

void TerminalPanel::onCursorPosition(int row, int col) {
  state_.cursorRow = qBound(0, row - 1, state_.rows - 1);
  state_.cursorCol = qBound(0, col - 1, state_.cols - 1);
}

void TerminalPanel::onCursorUp(int n) {
  state_.cursorRow = qMax(state_.scrollRegionTop, state_.cursorRow - n);
}

void TerminalPanel::onCursorDown(int n) {
  state_.cursorRow = qMin(state_.scrollRegionBottom, state_.cursorRow + n);
}

void TerminalPanel::onCursorForward(int n) {
  state_.cursorCol = qMin(state_.cols - 1, state_.cursorCol + n);
}

void TerminalPanel::onCursorBack(int n) {
  state_.cursorCol = qMax(0, state_.cursorCol - n);
}

void TerminalPanel::onEraseDisplay(int mode) {
  state_.dirty = true;
  switch (mode) {
    case 0:  // From cursor to end
      // Clear rest of current line
      for (int c = state_.cursorCol; c < state_.cols; ++c) {
        if (state_.cursorRow < screen_.size() && c < screen_[state_.cursorRow].length()) {
          screen_[state_.cursorRow].cells[c] = Cell();
        }
      }
      // Clear lines below
      for (int r = state_.cursorRow + 1; r < state_.rows; ++r) {
        screen_[r] = Line(state_.cols);
      }
      break;
    case 1:  // From start to cursor
      for (int c = 0; c <= state_.cursorCol; ++c) {
        if (state_.cursorRow < screen_.size() && c < screen_[state_.cursorRow].length()) {
          screen_[state_.cursorRow].cells[c] = Cell();
        }
      }
      for (int r = 0; r < state_.cursorRow; ++r) {
        screen_[r] = Line(state_.cols);
      }
      break;
    case 2:  // Entire display
      for (int r = 0; r < state_.rows; ++r) {
        screen_[r] = Line(state_.cols);
      }
      break;
  }
}

void TerminalPanel::onEraseLine(int mode) {
  state_.dirty = true;
  if (state_.cursorRow >= screen_.size()) return;

  switch (mode) {
    case 0:  // From cursor to end of line
      for (int c = state_.cursorCol; c < state_.cols; ++c) {
        if (c < screen_[state_.cursorRow].length()) {
          screen_[state_.cursorRow].cells[c] = Cell();
        }
      }
      break;
    case 1:  // From start of line to cursor
      for (int c = 0; c <= state_.cursorCol; ++c) {
        if (c < screen_[state_.cursorRow].length()) {
          screen_[state_.cursorRow].cells[c] = Cell();
        }
      }
      break;
    case 2:  // Entire line
      screen_[state_.cursorRow] = Line(state_.cols);
      break;
  }
}

void TerminalPanel::onScrollUp(int n) {
  state_.dirty = true;
  scrollBufferUp(n);
}

void TerminalPanel::onScrollDown(int n) {
  state_.dirty = true;
  scrollBufferDown(n);
}

void TerminalPanel::onSetScrollRegion(int top, int bottom) {
  state_.scrollRegionTop = qMax(0, top - 1);
  state_.scrollRegionBottom =
      bottom > 0 ? qMin(bottom - 1, state_.rows - 1) : state_.rows - 1;
}

void TerminalPanel::onProcessFinished(int exitCode) {
  shellExited_ = true;
  lastExitCode_ = exitCode;

  // Display exit message in terminal
  QString msg = QStringLiteral("\r\n[进程已退出，代码为 %1]\r\n按 Enter 键或点击此处重启终端...")
                    .arg(exitCode);

  // Write exit message directly to screen buffer
  for (int i = 0; i < msg.size(); ++i) {
    QChar ch = msg[i];
    if (ch == '\r') {
      state_.cursorCol = 0;
      continue;
    }
    if (ch == '\n') {
      state_.cursorRow++;
      if (state_.cursorRow > state_.scrollRegionBottom) {
        state_.cursorRow = state_.scrollRegionBottom;
        scrollBufferUp(1);
      }
      continue;
    }
    if (state_.cursorRow < screen_.size() && state_.cursorCol < screen_[state_.cursorRow].length()) {
      auto& cell = screen_[state_.cursorRow].cells[state_.cursorCol];
      cell.ch = ch;
      cell.fg = QColor("#569CD6");  // blue hint color
      cell.bg = state_.currentBg;
      cell.bold = false;
    }
    state_.cursorCol++;
    if (state_.cursorCol >= state_.cols) {
      state_.cursorCol = 0;
      state_.cursorRow++;
      if (state_.cursorRow > state_.scrollRegionBottom) {
        state_.cursorRow = state_.scrollRegionBottom;
        scrollBufferUp(1);
      }
    }
  }

  state_.dirty = true;
  flushToDisplay();
}

void TerminalPanel::restartShell() {
  startShell();
}

bool TerminalPanel::eventFilter(QObject* obj, QEvent* event) {
  if (event->type() == QEvent::KeyPress) {
    auto* keyEvent = static_cast<QKeyEvent*>(event);

    if (!process_ || !process_->isRunning()) {
      if (shellExited_) {
        // Any key or Enter restarts the shell
        restartShell();
      }
      return true;
    }

    // Any key input should scroll to bottom
    QScrollBar* vbar = display_->verticalScrollBar();
    if (vbar->value() < vbar->maximum()) {
      vbar->setValue(vbar->maximum());
    }

    QString text = keyEvent->text();
    int key = keyEvent->key();
    Qt::KeyboardModifiers mods = keyEvent->modifiers();

    if (key == Qt::Key_C && mods & Qt::ControlModifier &&
        !(mods & Qt::ShiftModifier)) {
      process_->write("\x03");
      return true;
    }

    if (key == Qt::Key_V && mods & Qt::ControlModifier) {
      QClipboard* clipboard = QApplication::clipboard();
      QString clipText = clipboard->text();
      if (!clipText.isEmpty()) {
        clipText.replace("\r\n", "\r").replace("\n", "\r");
        process_->write(clipText.toUtf8());
      }
      return true;
    }

    QByteArray seq;
    switch (key) {
      case Qt::Key_Return:
      case Qt::Key_Enter:
        seq = "\r";
        break;
      case Qt::Key_Backspace:
        seq = "\x08";
        break;
      case Qt::Key_Tab:
        seq = "\t";
        break;
      case Qt::Key_Up:
        seq = (mods & Qt::ShiftModifier) ? "\x1B[1;2A" : "\x1B[A";
        break;
      case Qt::Key_Down:
        seq = (mods & Qt::ShiftModifier) ? "\x1B[1;2B" : "\x1B[B";
        break;
      case Qt::Key_Right:
        seq = (mods & Qt::ShiftModifier) ? "\x1B[1;2C" : "\x1B[C";
        break;
      case Qt::Key_Left:
        seq = (mods & Qt::ShiftModifier) ? "\x1B[1;2D" : "\x1B[D";
        break;
      case Qt::Key_Home:
        seq = "\x1B[H";
        break;
      case Qt::Key_End:
        seq = "\x1B[F";
        break;
      case Qt::Key_PageUp:
        seq = "\x1B[5~";
        break;
      case Qt::Key_PageDown:
        seq = "\x1B[6~";
        break;
      case Qt::Key_Delete:
        seq = "\x1B[3~";
        break;
      case Qt::Key_Insert:
        seq = "\x1B[2~";
        break;
      default:
        break;
    }

    if (!seq.isEmpty()) {
      process_->write(seq);
      return true;
    }

    if (key == Qt::Key_D && mods & Qt::ControlModifier) {
      process_->write("\x04");
      return true;
    }
    if (key == Qt::Key_Z && mods & Qt::ControlModifier) {
      process_->write("\x1A");
      return true;
    }

    if (!text.isEmpty()) {
      process_->write(text.toUtf8());
      return true;
    }

    return true;  // Swallow all other key events for the terminal
  }

  // Block input method events (prevents IME from editing the document)
  if (event->type() == QEvent::InputMethod ||
      event->type() == QEvent::InputMethodQuery) {
    return true;
  }

  // Block mouse middle-click paste, left-click restarts if shell exited
  if (event->type() == QEvent::MouseButtonPress) {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton && shellExited_) {
      restartShell();
      return true;
    }
    if (mouseEvent->button() == Qt::MiddleButton) {
      // Forward clipboard content to PTY instead of pasting into document
      if (process_ && process_->isRunning()) {
        QClipboard* clipboard = QApplication::clipboard();
        QString clipText = clipboard->text();
        if (!clipText.isEmpty()) {
          clipText.replace("\r\n", "\r").replace("\n", "\r");
          process_->write(clipText.toUtf8());
        }
      }
      return true;
    }
  }

  return QWidget::eventFilter(obj, event);
}

void TerminalPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  // Set focus to display when terminal tab becomes visible
  if (display_) {
    display_->setFocus();
  }
}

void TerminalPanel::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (resize_timer_) {
    resize_timer_->start();
  }
}

}  // namespace app
}  // namespace etest
