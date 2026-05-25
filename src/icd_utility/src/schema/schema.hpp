#pragma once

#include <icd/types.hpp>

#include <optional>
#include <string>
#include <vector>

namespace icd::schema {

struct SchemaNodeDef {
    std::string name;
    std::string description;
    int offset {};
    int bit_offset {};
    int bit_width {};
    ValueType value_type {ValueType::unknown};
    Tag tag {Tag::none};
    NodeAttrs attrs;
    std::vector<SchemaNodeDef> children;
};

struct SchemaFrameDef {
    int id {};
    std::string name;
    std::string description;
    FrameType type {FrameType::data};
    ByteOrder order {ByteOrder::little_endian};
    std::vector<SchemaNodeDef> roots;
};

struct SchemaFileEntry {
    std::optional<int> id;
    std::string logical_name;
    std::string description;
    std::string path;
    std::optional<FrameType> type;
    std::optional<ByteOrder> order;
    Format format {Format::auto_detect};
};

struct SchemaConfig {
    std::vector<SchemaFileEntry> files;
    std::vector<SchemaFrameDef> frames;
};

} // namespace icd::schema
