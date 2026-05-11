#pragma once

#include <icd/export.hpp>

#include <optional>
#include <variant>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace icd {

enum class Format {
    auto_detect,
    xml,
    json
};

enum class FrameType {
    data,
    cmd,
    data_cmd
};

enum class ByteOrder {
    little_endian,
    big_endian
};

enum class ValueType {
    unknown,
    boolean,
    byte_,
    bytes,
    word,
    shortint,
    smallint,
    longword,
    integer,
    ulong_,
    single,
    double_,
    string_
};

enum class Tag {
    none,
    head,
    length,
    count,
    sum,
    sum2,
    xor_,
    xor1,
    xor2,
    init_value,
    signal_in_value,
    big_endian_value
};

enum class DecodeMode {
    eager,
    lazy
};

struct ICD_UTILITY_API NodeAttrs {
    std::string system_name;
    std::string group_name;
    std::string unit;
    std::string value_text_list;
    std::string scale_formula;
    std::string scale_convertor;
    std::string link_to;

    std::optional<float> min;
    std::optional<float> max;
    std::optional<float> scale_a;
    std::optional<float> scale_b;
    bool is_scaled {false};
};

using NodeValue = std::variant<bool,
                               std::uint16_t,
                               std::int16_t,
                               std::uint32_t,
                               std::int32_t,
                               std::uint64_t,
                               std::int64_t,
                               double,
                               std::string,
                               std::vector<std::byte>>;

} // namespace icd
