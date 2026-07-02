#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QPair>

#include <icd/frame.hpp>
#include <icd/node.hpp>

#include <optional>
#include <vector>
#include <cstddef>

namespace etest::protocol {

// 把十六进制字符串（可含空格，大小写不敏感）解析为字节序列。
// 长度为奇数或含非法字符时返回 std::nullopt。
std::optional<std::vector<std::byte>> parseHexBytes(const QString& hex);

// 对一段报文字节按帧协议递归解析，生成「字段名 = 值」形式的文本行列表。
// 解码失败的字段也会生成一行，并在行中标注失败。
QStringList decodeFramePreview(const icd::Frame& frame,
                               const std::vector<std::byte>& bytes);

// 同上，但额外返回每行对应的 node 指针，用于 UI 联动。
// 行顺序与返回的 node 列表顺序一致；解码失败的行也会对应其 node。
QVector<QPair<QString, const icd::Node*>> decodeFramePreviewWithNodes(
    const icd::Frame& frame, const std::vector<std::byte>& bytes);

}  // namespace etest::protocol
