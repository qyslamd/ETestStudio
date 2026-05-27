#include "TuxConsoleSaver.h"

#include <QPainter>
#include <QFont>

TuxConsoleSaver::TuxConsoleSaver(QWidget* parent) : TuxSaverWidget(parent) {}

// ═════════════════════════════════════════════════════════════════════════════
//  Background — terminal + floor
// ═════════════════════════════════════════════════════════════════════════════

void TuxConsoleSaver::drawBackground(QPainter& p) const {
  drawConsoleBackground(p);
  drawFloor(p);
}

void TuxConsoleSaver::drawConsoleBackground(QPainter& p) const {
  p.fillRect(rect(), QColor(0x0C, 0x0C, 0x0C));

  QFont mono("Consolas", 10);
  mono.setStyleHint(QFont::Monospace);
  p.setFont(mono);

  static const char* lines[] = {
      "root@tux-saver:~$ ./configure --enable-fun",
      "checking for cute penguin... yes",
      "checking for waddle support... yes",
      "checking for fish cache... /var/cache/sardines",
      "checking for Tux... found at /usr/bin/penguin",
      "root@tux-saver:~$ make -j4",
      "[ 12%] Building CXX object waddle.cc",
      "[ 38%] Building CXX object blink.cc",
      "[ 55%] Building CXX object head_tilt.cc",
      "[ 72%] Building CXX object wing_flap.cc",
      "[ 89%] Building CXX object flee.cc",
      "[100%] Linking CXX executable tux-saver",
      "root@tux-saver:~$ ./tux-saver --interactive",
      ">>>> Tux Console Saver v2.7 <<<<",
      "> initializing penguin subsystem...",
      "> calibrating wobble... OK",
      "> loading fish patterns... OK",
      "> penguin is ready!",
      "",
      "root@tux-saver:~$ _",
  };

  p.setPen(QColor(0x33, 0xFF, 0x33, 60));
  const int lineCount = sizeof(lines) / sizeof(lines[0]);
  for (int i = 0; i < lineCount; ++i) {
    int yy = 34 + i * 18;
    if (yy > floorY() - 20) break;
    p.drawText(16, yy, QString::fromLatin1(lines[i]));
  }

  // Scanline overlay
  for (int y = 0; y < height(); y += 3) {
    p.fillRect(0, y, width(), 1, QColor(0, 0, 0, 20));
  }
}

void TuxConsoleSaver::drawFloor(QPainter& p) const {
  qreal fy = floorY();
  p.fillRect(0, static_cast<int>(fy), width(), height() - static_cast<int>(fy),
             QColor(0x1A, 0x1A, 0x1A));

  p.setPen(QPen(QColor(0x33, 0x33, 0x44), 1));
  p.drawLine(0, static_cast<int>(fy), width(), static_cast<int>(fy));

  p.setPen(QPen(QColor(0x22, 0x22, 0x22), 1));
  for (int x = 0; x < width(); x += 30) {
    p.drawLine(x, static_cast<int>(fy) + 1, x, height());
  }
  for (int y = static_cast<int>(fy) + 10; y < height(); y += 10) {
    p.drawLine(0, y, width(), y);
  }
}
