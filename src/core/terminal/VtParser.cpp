#include "terminal/VtParser.h"

#include <QTextCodec>

namespace etest {
namespace core {
namespace terminal {

VtParser::VtParser(QObject* parent) : QObject(parent) {}

void VtParser::parse(const QByteArray& data) {
  for (int i = 0; i < data.size(); ++i) {
    processByte(static_cast<unsigned char>(data[i]));
  }
  // Flush any remaining ground buffer
  flushGroundBuffer();
}

void VtParser::flushGroundBuffer() {
  if (groundBuffer_.isEmpty()) return;

  // Try UTF-8 first, fallback to system locale (GBK on Chinese Windows)
  QString decoded = QString::fromUtf8(groundBuffer_);
  if (decoded.contains(QChar::ReplacementCharacter)) {
    // UTF-8 decode failed, try system codec
    decoded = QString::fromLocal8Bit(groundBuffer_);
  }

  if (!decoded.isEmpty()) {
    emit text(decoded);
  }
  groundBuffer_.clear();
}

void VtParser::processByte(unsigned char byte) {
  switch (state_) {
    case kGround:
      if (byte == 0x1B) {
        flushGroundBuffer();
        state_ = kEscape;
      } else if (byte == 0x07) {
        flushGroundBuffer();
        emit bell();
      } else if (byte == 0x0D) {
        flushGroundBuffer();
        emit text(QStringLiteral("\r"));
      } else if (byte == 0x0A) {
        flushGroundBuffer();
        emit text(QStringLiteral("\n"));
      } else if (byte == 0x08) {
        flushGroundBuffer();
        emit text(QStringLiteral("\b"));
      } else if (byte == 0x09) {
        flushGroundBuffer();
        emit text(QStringLiteral("\t"));
      } else if (byte >= 0x20) {
        // Accumulate for multi-byte decoding
        groundBuffer_.append(static_cast<char>(byte));
      }
      // Ignore other C0 controls
      break;

    case kEscape:
      if (byte == '[') {
        state_ = kCsiEntry;
        csiParams_.clear();
        csiIntermediates_.clear();
        csiFinal_ = 0;
      } else if (byte == ']') {
        state_ = kOscString;
        oscBuffer_.clear();
      } else if (byte == '7' || byte == '8') {
        state_ = kGround;
      } else if (byte == 'D') {
        emit scrollUp(1);
        state_ = kGround;
      } else if (byte == 'M') {
        emit scrollDown(1);
        state_ = kGround;
      } else if (byte == '(') {
        state_ = kGround;
      } else {
        state_ = kGround;
      }
      break;

    case kCsiEntry:
      if (byte >= '0' && byte <= '9') {
        csiParams_.append(static_cast<char>(byte));
        state_ = kCsiParam;
      } else if (byte == ';') {
        csiParams_.append(';');
        state_ = kCsiParam;
      } else if (byte == '?') {
        csiParams_.append('?');
        state_ = kCsiParam;
      } else if (byte >= 0x20 && byte <= 0x2F) {
        csiIntermediates_.append(static_cast<char>(byte));
        state_ = kCsiIntermediate;
      } else if (byte >= 0x40 && byte <= 0x7E) {
        csiFinal_ = static_cast<char>(byte);
        executeCsi();
        state_ = kGround;
      }
      break;

    case kCsiParam:
      if ((byte >= '0' && byte <= '9') || byte == ';' || byte == '?' ||
          byte == ':') {
        csiParams_.append(static_cast<char>(byte));
      } else if (byte >= 0x20 && byte <= 0x2F) {
        csiIntermediates_.append(static_cast<char>(byte));
        state_ = kCsiIntermediate;
      } else if (byte >= 0x40 && byte <= 0x7E) {
        csiFinal_ = static_cast<char>(byte);
        executeCsi();
        state_ = kGround;
      }
      break;

    case kCsiIntermediate:
      if (byte >= 0x20 && byte <= 0x2F) {
        csiIntermediates_.append(static_cast<char>(byte));
      } else if (byte >= 0x40 && byte <= 0x7E) {
        csiFinal_ = static_cast<char>(byte);
        executeCsi();
        state_ = kGround;
      }
      break;

    case kOscString:
      if (byte == 0x07) {
        parseOsc(oscBuffer_);
        state_ = kGround;
      } else if (byte == 0x1B) {
        parseOsc(oscBuffer_);
        state_ = kEscape;
      } else {
        oscBuffer_.append(static_cast<char>(byte));
      }
      break;
  }
}

void VtParser::executeCsi() {
  if (csiParams_.startsWith('?')) {
    return;
  }

  QVector<int> params;
  if (!csiParams_.isEmpty()) {
    QList<QByteArray> parts = csiParams_.split(';');
    for (const auto& part : parts) {
      bool ok = false;
      int val = part.toInt(&ok);
      params.append(ok ? val : 0);
    }
  }

  auto param = [&params](int index, int defaultVal) -> int {
    return (index < params.size() && params[index] > 0) ? params[index]
                                                         : defaultVal;
  };

  switch (csiFinal_) {
    case 'm':
      if (params.isEmpty()) params.append(0);
      emit sgr(params);
      break;

    case 'H':
    case 'f':
      emit cursorPosition(param(0, 1), param(1, 1));
      break;

    case 'A':
      emit cursorUp(param(0, 1));
      break;

    case 'B':
      emit cursorDown(param(0, 1));
      break;

    case 'C':
      emit cursorForward(param(0, 1));
      break;

    case 'D':
      emit cursorBack(param(0, 1));
      break;

    case 'J':
      emit eraseDisplay(param(0, 0));
      break;

    case 'K':
      emit eraseLine(param(0, 0));
      break;

    case 'S':
      emit scrollUp(param(0, 1));
      break;

    case 'T':
      emit scrollDown(param(0, 1));
      break;

    case 'r':
      if (params.size() >= 2) {
        emit setScrollRegion(params[0], params[1]);
      } else {
        emit setScrollRegion(1, 0);
      }
      break;

    case 'E':
      emit cursorDown(param(0, 1));
      emit cursorPosition(0, 1);
      break;

    case 'F':
      emit cursorUp(param(0, 1));
      break;

    default:
      break;
  }
}

void VtParser::parseOsc(const QByteArray& data) {
  int sepIdx = data.indexOf(';');
  if (sepIdx < 0) return;

  int code = data.left(sepIdx).toInt();
  QString value = QString::fromUtf8(data.mid(sepIdx + 1));

  if (code == 0 || code == 2) {
    emit setTitle(value);
  }
}

}  // namespace terminal
}  // namespace core
}  // namespace etest
