#include "IcdProtocolUtils.h"

namespace etest::protocal::utils {

std::string valueTypeToString(icd::ValueType vt) {
  switch (vt) {
    case icd::ValueType::boolean:   return "bool";
    case icd::ValueType::byte_:     return "uint8";
    case icd::ValueType::bytes:     return "bytes";
    case icd::ValueType::word:      return "uint16";
    case icd::ValueType::shortint:  return "int16";
    case icd::ValueType::smallint:  return "int16";
    case icd::ValueType::longword:  return "uint32";
    case icd::ValueType::integer:   return "int32";
    case icd::ValueType::ulong_:    return "uint64";
    case icd::ValueType::single:    return "float";
    case icd::ValueType::double_:   return "double";
    case icd::ValueType::string_:   return "string";
    case icd::ValueType::unknown:   return "unknown";
  }
  return "unknown";
}

const char* valueTypeName(icd::ValueType vt) {
  switch (vt) {
    case icd::ValueType::unknown:   return "unknown";
    case icd::ValueType::boolean:   return "boolean";
    case icd::ValueType::byte_:     return "uint8";
    case icd::ValueType::bytes:     return "bytes";
    case icd::ValueType::word:      return "uint16";
    case icd::ValueType::shortint:  return "int16";
    case icd::ValueType::smallint:  return "int16";
    case icd::ValueType::longword:  return "uint32";
    case icd::ValueType::integer:   return "int32";
    case icd::ValueType::ulong_:    return "uint64";
    case icd::ValueType::single:    return "float";
    case icd::ValueType::double_:   return "double";
    case icd::ValueType::string_:   return "string";
  }
  return "unknown";
}

icd::ValueType valueTypeFromName(const std::string& name) {
  if (name == "uint8")   return icd::ValueType::byte_;
  if (name == "uint16")  return icd::ValueType::word;
  if (name == "int16")   return icd::ValueType::shortint;
  if (name == "uint32")  return icd::ValueType::longword;
  if (name == "int32")   return icd::ValueType::integer;
  if (name == "uint64")  return icd::ValueType::ulong_;
  if (name == "float")   return icd::ValueType::single;
  if (name == "double")  return icd::ValueType::double_;
  if (name == "boolean") return icd::ValueType::boolean;
  if (name == "bytes")   return icd::ValueType::bytes;
  if (name == "string")  return icd::ValueType::string_;
  return icd::ValueType::unknown;
}

const char* tagName(icd::Tag tag) {
  switch (tag) {
    case icd::Tag::none:             return "none";
    case icd::Tag::head:             return "head";
    case icd::Tag::length:           return "length";
    case icd::Tag::count:            return "count";
    case icd::Tag::sum:              return "sum";
    case icd::Tag::sum2:             return "sum";
    case icd::Tag::xor_:             return "xor";
    case icd::Tag::xor1:             return "xor";
    case icd::Tag::xor2:             return "xor";
    case icd::Tag::init_value:       return "init_value";
    case icd::Tag::signal_in_value:  return "signal_in_value";
    case icd::Tag::big_endian_value: return "big_endian_value";
  }
  return "none";
}

icd::Tag tagFromName(const std::string& name) {
  if (name == "head")            return icd::Tag::head;
  if (name == "length")           return icd::Tag::length;
  if (name == "count")            return icd::Tag::count;
  if (name == "sum")              return icd::Tag::sum;
  if (name == "xor")              return icd::Tag::xor_;
  if (name == "signal_in_value")  return icd::Tag::signal_in_value;
  return icd::Tag::none;
}

int frameTypeIndex(icd::FrameType ft) {
  switch (ft) {
    case icd::FrameType::data:     return 0;
    case icd::FrameType::cmd:      return 1;
    case icd::FrameType::data_cmd: return 2;
  }
  return 0;
}

icd::FrameType frameTypeFromIndex(int idx) {
  static constexpr icd::FrameType types[] = {
      icd::FrameType::data, icd::FrameType::cmd, icd::FrameType::data_cmd};
  if (idx >= 0 && idx < 3) return types[idx];
  return icd::FrameType::data;
}

int byteOrderIndex(icd::ByteOrder bo) {
  switch (bo) {
    case icd::ByteOrder::little_endian: return 0;
    case icd::ByteOrder::big_endian:    return 1;
  }
  return 0;
}

icd::ByteOrder byteOrderFromIndex(int idx) {
  return idx == 1 ? icd::ByteOrder::big_endian : icd::ByteOrder::little_endian;
}

}  // namespace etest::protocal::utils
