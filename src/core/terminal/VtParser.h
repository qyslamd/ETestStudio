#ifndef ETEST_CORE_TERMINAL_VTPARSER_H_
#define ETEST_CORE_TERMINAL_VTPARSER_H_

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

namespace etest {
namespace core {
namespace terminal {

class VtParser : public QObject {
  Q_OBJECT

 public:
  explicit VtParser(QObject* parent = nullptr);

  void parse(const QByteArray& data);

 signals:
  void text(const QString& str);
  void sgr(const QVector<int>& params);
  void cursorPosition(int row, int col);
  void cursorUp(int n);
  void cursorDown(int n);
  void cursorForward(int n);
  void cursorBack(int n);
  void eraseDisplay(int mode);
  void eraseLine(int mode);
  void scrollUp(int n);
  void scrollDown(int n);
  void setScrollRegion(int top, int bottom);
  void bell();
  void setTitle(const QString& title);

 private:
  void processByte(unsigned char byte);
  void flushGroundBuffer();
  void executeCsi();
  void parseOsc(const QByteArray& data);

  enum State {
    kGround,
    kEscape,
    kCsiEntry,
    kCsiParam,
    kCsiIntermediate,
    kOscString,
  };

  State state_ = kGround;
  QByteArray groundBuffer_;  // accumulates printable bytes for UTF-8 decode
  QByteArray csiParams_;
  QByteArray csiIntermediates_;
  char csiFinal_ = 0;
  QByteArray oscBuffer_;
};

}  // namespace terminal
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_TERMINAL_VTPARSER_H_
