#include "type_mapping.hpp"

#include <string>
#include <string_view>

namespace icd::format {

// ========== .eproto / .eprotox shared value type mapping ==========

std::string value_type_to_string(ValueType type) {
    switch (type) {
    case ValueType::boolean:  return "boolean";
    case ValueType::byte_:    return "uint8";
    case ValueType::bytes:    return "bytes";
    case ValueType::word:     return "uint16";
    case ValueType::shortint: return "int16";
    case ValueType::smallint: return "smallint";
    case ValueType::longword: return "uint32";
    case ValueType::integer:  return "int32";
    case ValueType::ulong_:   return "uint64";
    case ValueType::single:   return "float";
    case ValueType::double_:  return "double";
    case ValueType::string_:  return "string";
    case ValueType::unknown:  return "unknown";
    }
    return "unknown";
}

ValueType value_type_from_string(std::string_view str) {
    if (str == "boolean")  return ValueType::boolean;
    if (str == "uint8")    return ValueType::byte_;
    if (str == "bytes")    return ValueType::bytes;
    if (str == "uint16")   return ValueType::word;
    if (str == "int16")    return ValueType::shortint;
    if (str == "smallint") return ValueType::smallint;
    if (str == "uint32")   return ValueType::longword;
    if (str == "int32")    return ValueType::integer;
    if (str == "uint64")   return ValueType::ulong_;
    if (str == "float")    return ValueType::single;
    if (str == "double")   return ValueType::double_;
    if (str == "string")   return ValueType::string_;
    return ValueType::unknown;
}

// ========== FrameType string mapping ==========

std::string frame_type_to_string(FrameType type) {
    switch (type) {
    case FrameType::data:     return "data";
    case FrameType::cmd:      return "cmd";
    case FrameType::data_cmd: return "dataCfg";
    }
    return "data";
}

FrameType frame_type_from_string(std::string_view str) {
    if (str == "cmd")     return FrameType::cmd;
    if (str == "data")    return FrameType::data;
    if (str == "dataCfg") return FrameType::data_cmd;
    return FrameType::data;
}

// ========== ByteOrder string mapping ==========

std::string byte_order_to_string(ByteOrder order) {
    switch (order) {
    case ByteOrder::little_endian: return "littleEndian";
    case ByteOrder::big_endian:    return "bigEndian";
    }
    return "littleEndian";
}

ByteOrder byte_order_from_string(std::string_view str) {
    return (str == "bigEndian") ? ByteOrder::big_endian : ByteOrder::little_endian;
}

// ========== Legacy XML value type mapping ==========

std::string value_type_to_legacy_xml_string(ValueType type) {
    switch (type) {
    case ValueType::byte_:    return "byte";
    case ValueType::bytes:    return "bytes";
    case ValueType::word:     return "word";
    case ValueType::longword: return "dword";
    case ValueType::shortint: return "short";
    case ValueType::smallint: return "int";
    case ValueType::integer:  return "int";
    case ValueType::single:   return "float";
    case ValueType::double_:  return "double";
    case ValueType::string_:  return "string";
    case ValueType::boolean:  return "byte";
    case ValueType::ulong_:   return "dword";
    case ValueType::unknown:  return "byte";
    }
    return "byte";
}

ValueType value_type_from_legacy_xml_string(std::string_view str) {
    if (str == "byte")   return ValueType::byte_;
    if (str == "bytes")  return ValueType::bytes;
    if (str == "word")   return ValueType::word;
    if (str == "dword")  return ValueType::longword;
    if (str == "short")  return ValueType::shortint;
    if (str == "float")  return ValueType::single;
    if (str == "double") return ValueType::double_;
    if (str == "string") return ValueType::string_;
    if (str == "int")    return ValueType::integer;
    return ValueType::unknown;
}

// ========== Legacy JSON value type mapping ==========

std::string value_type_to_legacy_json_string(ValueType type) {
    switch (type) {
    case ValueType::byte_:    return "byte";
    case ValueType::bytes:    return "bytes";
    case ValueType::word:     return "word";
    case ValueType::longword: return "int";
    case ValueType::shortint: return "int16";
    case ValueType::smallint: return "smallint";
    case ValueType::integer:  return "int";
    case ValueType::single:   return "float";
    case ValueType::double_:  return "double";
    case ValueType::string_:  return "string";
    case ValueType::boolean:  return "byte";
    case ValueType::ulong_:   return "int";
    case ValueType::unknown:  return "byte";
    }
    return "byte";
}

ValueType value_type_from_legacy_json_string(std::string_view str) {
    if (str == "byte")     return ValueType::byte_;
    if (str == "bytes")    return ValueType::bytes;
    if (str == "word")     return ValueType::word;
    if (str == "int")      return ValueType::integer;
    if (str == "int16")    return ValueType::shortint;
    if (str == "smallint") return ValueType::smallint;
    if (str == "float")    return ValueType::single;
    if (str == "double")   return ValueType::double_;
    if (str == "string")   return ValueType::string_;
    return ValueType::unknown;
}

// ========== Legacy Tag integer mapping ==========

int tag_to_legacy_int(Tag tag) {
    switch (tag) {
    case Tag::none:              return 0;
    case Tag::head:              return 1;
    case Tag::length:            return 2;
    case Tag::count:             return 3;
    case Tag::sum:               return 40;
    case Tag::sum2:              return 40;
    case Tag::xor_:              return 40;
    case Tag::xor1:              return 40;
    case Tag::xor2:              return 40;
    case Tag::init_value:        return 9;
    case Tag::signal_in_value:   return 41;
    case Tag::big_endian_value:  return 60;
    }
    return 0;
}

Tag tag_from_legacy_int(int value) {
    switch (value) {
    case 0:  return Tag::none;
    case 1:  return Tag::head;
    case 2:  return Tag::length;
    case 3:  return Tag::count;
    case 9:  return Tag::init_value;
    case 40: return Tag::sum;
    case 41: return Tag::signal_in_value;
    case 60: return Tag::big_endian_value;
    default: return Tag::none;
    }
}

} // namespace icd::format
