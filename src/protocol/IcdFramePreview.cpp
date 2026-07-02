#include "IcdFramePreview.h"

#include <icd/node.hpp>

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace etest::protocol {

namespace {

// 把单个字节的解码结果按值类型格式化为可读字符串。
QString formatScalarValue(const icd::NodeValue& value) {
  return std::visit(
      [](auto&& v) -> QString {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          return v ? QStringLiteral("true") : QStringLiteral("false");
        } else if constexpr (std::is_same_v<T, std::uint16_t> ||
                             std::is_same_v<T, std::int16_t> ||
                             std::is_same_v<T, std::uint32_t> ||
                             std::is_same_v<T, std::int32_t> ||
                             std::is_same_v<T, std::uint64_t> ||
                             std::is_same_v<T, std::int64_t>) {
          return QString::number(static_cast<qulonglong>(v));
        } else if constexpr (std::is_same_v<T, double>) {
          if (std::isnan(v)) {
            return QStringLiteral("NaN");
          }
          return QString::number(v, 'g', 8);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return QString::fromStdString(v);
        } else if constexpr (std::is_same_v<T, std::vector<std::byte>>) {
          QString hex;
          hex.reserve(static_cast<int>(v.size() * 3));
          for (const auto b : v) {
            if (!hex.isEmpty()) {
              hex += QLatin1Char(' ');
            }
            hex += QString::number(
                std::to_integer<unsigned>(b), 16).rightJustified(2, '0').toUpper();
          }
          return hex;
        }
        return QStringLiteral("?");
      },
      value);
}

// 在 ValueTextList（形如 "不用=0,左单元=1"）中查找原始整数值对应的枚举文本。
// 找不到时返回空字符串。
QString lookupEnumLabel(const std::string& value_text_list,
                        std::uint64_t raw) {
  if (value_text_list.empty()) {
    return {};
  }
  const QString text = QString::fromStdString(value_text_list);
  const auto entries = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
  const QString key = QString::number(static_cast<qulonglong>(raw));
  for (const QString& entry : entries) {
    const auto parts = entry.split(QLatin1Char('='));
    if (parts.size() != 2) {
      continue;
    }
    const QString entry_value = parts[1].trimmed();
    if (entry_value == key) {
      return parts[0].trimmed();
    }
  }
  return {};
}

// 递归解码单个节点及其子节点，把结果追加到 lines（文本 + node 指针）。
void decodeNodeRecursive(const icd::Node& node,
                         const std::vector<std::byte>& bytes,
                         QVector<QPair<QString, const icd::Node*>>& rows,
                         int indent) {
  const QString name = QString::fromStdString(std::string(node.name()));
  const QString indent_str(indent, QLatin1Char(' '));

  const auto decoded = node.decode(bytes, icd::ByteOrder::little_endian);
  if (!decoded) {
    rows.append({QStringLiteral("%1%2 = 解码失败").arg(indent_str, name),
                 &node});
    return;
  }

  QString value_text = formatScalarValue(*decoded);

  // 整数类尝试枚举翻译
  std::visit(
      [&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_integral_v<T>) {
          const QString label =
              lookupEnumLabel(node.attrs().value_text_list,
                              static_cast<std::uint64_t>(v));
          if (!label.isEmpty()) {
            value_text = QStringLiteral("%1 (%2)").arg(value_text, label);
          }
        }
      },
      *decoded);

  if (!node.attrs().unit.empty()) {
    value_text += QStringLiteral(" %1").arg(
        QString::fromStdString(node.attrs().unit));
  }

  rows.append(
      {QStringLiteral("%1%2 = %3").arg(indent_str, name, value_text), &node});

  for (const auto& child : node.children()) {
    decodeNodeRecursive(*child, bytes, rows, indent + 2);
  }
}

}  // namespace

std::optional<std::vector<std::byte>> parseHexBytes(const QString& hex) {
  QString compact = hex;
  compact.remove(QRegularExpression(QStringLiteral("[\\s]")));
  if (compact.isEmpty()) {
    return std::vector<std::byte>{};
  }
  if (compact.size() % 2 != 0) {
    return std::nullopt;
  }

  std::vector<std::byte> bytes;
  bytes.reserve(static_cast<std::size_t>(compact.size() / 2));
  for (int i = 0; i < compact.size(); i += 2) {
    bool ok = false;
    const ushort byte_val = compact.mid(i, 2).toUShort(&ok, 16);
    if (!ok || byte_val > 0xFF) {
      return std::nullopt;
    }
    bytes.push_back(static_cast<std::byte>(byte_val));
  }
  return bytes;
}

QStringList decodeFramePreview(const icd::Frame& frame,
                               const std::vector<std::byte>& bytes) {
  const auto rows = decodeFramePreviewWithNodes(frame, bytes);
  QStringList lines;
  lines.reserve(rows.size());
  for (const auto& row : rows) {
    lines << row.first;
  }
  return lines;
}

QVector<QPair<QString, const icd::Node*>> decodeFramePreviewWithNodes(
    const icd::Frame& frame, const std::vector<std::byte>& bytes) {
  QVector<QPair<QString, const icd::Node*>> rows;
  for (const auto& root : frame.roots()) {
    decodeNodeRecursive(*root, bytes, rows, 0);
  }
  return rows;
}

}  // namespace etest::protocol
