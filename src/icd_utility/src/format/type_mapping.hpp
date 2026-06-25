#pragma once

#include <icd/types.hpp>

#include <string>
#include <string_view>

namespace icd::format {

// .eproto / .eprotox shared type system (uint8/uint16/int32...)
std::string value_type_to_string(ValueType type);
ValueType value_type_from_string(std::string_view str);

std::string frame_type_to_string(FrameType type);
FrameType frame_type_from_string(std::string_view str);

std::string byte_order_to_string(ByteOrder order);
ByteOrder byte_order_from_string(std::string_view str);

// Legacy XML type mapping (byte/word/dword...)
std::string value_type_to_legacy_xml_string(ValueType type);
ValueType value_type_from_legacy_xml_string(std::string_view str);

// Legacy JSON type mapping
std::string value_type_to_legacy_json_string(ValueType type);
ValueType value_type_from_legacy_json_string(std::string_view str);

// Legacy Tag integer mapping (40->sum, 41->signal_in_value, 60->big_endian_value)
int tag_to_legacy_int(Tag tag);
Tag tag_from_legacy_int(int value);

// All formats use "scaleConveror" (no 't') to match legacy format.
// The C++ field name is scale_convertor (with 't') -- the difference is intentional.
inline constexpr auto kScaleConverorKey = "scaleConveror";

} // namespace icd::format
