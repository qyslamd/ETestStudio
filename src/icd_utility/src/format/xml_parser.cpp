#include "xml_parser.hpp"
#include "xml_util.hpp"

#include "type_mapping.hpp"

#include <pugixml.hpp>

#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace icd::format {
namespace {

std::optional<int> parse_optional_int(const pugi::xml_node& node, const char* child_name) {
    auto child = node.child(child_name);
    if (!child) {
        return std::nullopt;
    }
    try {
        return std::stoi(child.child_value());
    } catch (...) {
        return std::nullopt;
    }
}

FrameType parse_frame_type(int raw) noexcept {
    switch (raw) {
    case 2:
        return FrameType::cmd;
    case 4:
        return FrameType::data_cmd;
    case 1:
    default:
        return FrameType::data;
    }
}

ByteOrder parse_byte_order(int raw) noexcept {
    return (raw == 1) ? ByteOrder::big_endian : ByteOrder::little_endian;
}

tl::expected<int, Error> parse_int(const pugi::xml_node& node, const char* child_name, const std::filesystem::path& file) {
    auto child = node.child(child_name);
    if (!child) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing required integer field", file, child_name});
    }
    try {
        return std::stoi(child.child_value());
    } catch (...) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "invalid integer", file, child_name});
    }
}

tl::expected<float, Error> parse_float(const pugi::xml_node& node, const char* child_name, const std::filesystem::path& file) {
    auto child = node.child(child_name);
    if (!child) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing float field", file, child_name});
    }
    try {
        return std::stof(child.child_value());
    } catch (...) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "invalid float", file, child_name});
    }
}

ValueType parse_value_type(std::string_view type) noexcept {
    if (type == "byte") return ValueType::byte_;
    if (type == "bytes") return ValueType::bytes;
    if (type == "word") return ValueType::word;
    if (type == "dword") return ValueType::longword;
    if (type == "short") return ValueType::shortint;
    if (type == "float") return ValueType::single;
    if (type == "double") return ValueType::double_;
    if (type == "string") return ValueType::string_;
    if (type == "int") return ValueType::integer;
    return ValueType::unknown;
}

tl::expected<schema::SchemaNodeDef, Error> parse_node(const pugi::xml_node& item, const std::filesystem::path& file) {
    schema::SchemaNodeDef node;

    auto offset = parse_int(item, "Offset", file);
    if (!offset) return tl::make_unexpected(offset.error());
    node.offset = *offset;

    auto start_bit = parse_int(item, "StartBit", file);
    if (!start_bit) return tl::make_unexpected(start_bit.error());
    node.bit_offset = *start_bit;

    auto bit_width = parse_int(item, "BitWidth", file);
    if (!bit_width) return tl::make_unexpected(bit_width.error());
    node.bit_width = *bit_width;

    auto type = item.child("Type");
    auto name = item.child("Name");
    auto description = item.child("Description");
    auto is_scaled = item.child("IsScaled");
    if (!type || !name || !description || !is_scaled) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing required node field", file, "Item"});
    }

    node.value_type = parse_value_type(type.child_value());
    node.name = name.child_value();
    node.description = description.child_value();

    auto is_scaled_value = parse_int(item, "IsScaled", file);
    if (!is_scaled_value) return tl::make_unexpected(is_scaled_value.error());
    node.attrs.is_scaled = (*is_scaled_value != 0);

    if (auto child = item.child("GroupName")) node.attrs.group_name = child.child_value();
    if (auto child = item.child("SystemName")) node.attrs.system_name = child.child_value();
    if (auto child = item.child("Unit")) node.attrs.unit = child.child_value();
    if (auto child = item.child("ValueTextList")) node.attrs.value_text_list = child.child_value();
    if (auto child = item.child("ScaleFormula")) node.attrs.scale_formula = child.child_value();
    if (auto child = item.child("ScaleConveror")) node.attrs.scale_convertor = child.child_value();
    if (auto child = item.child("LinkTo")) node.attrs.link_to = child.child_value();

    if (auto child = item.child("ScaleA")) {
        auto value = parse_float(item, "ScaleA", file);
        if (!value) return tl::make_unexpected(value.error());
        node.attrs.scale_a = *value;
    }
    if (auto child = item.child("ScaleB")) {
        auto value = parse_float(item, "ScaleB", file);
        if (!value) return tl::make_unexpected(value.error());
        node.attrs.scale_b = *value;
    }
    if (auto child = item.child("Min")) {
        auto value = parse_float(item, "Min", file);
        if (!value) return tl::make_unexpected(value.error());
        node.attrs.min = *value;
    }
    if (auto child = item.child("Max")) {
        auto value = parse_float(item, "Max", file);
        if (!value) return tl::make_unexpected(value.error());
        node.attrs.max = *value;
    }
    if (auto child = item.child("Tag")) {
        auto value = parse_int(item, "Tag", file);
        if (!value) return tl::make_unexpected(value.error());
        node.tag = tag_from_legacy_int(*value);
    }

    if (auto childs = item.child("Childs")) {
        for (auto child_item : childs.children("Item")) {
            auto child = parse_node(child_item, file);
            if (!child) return tl::make_unexpected(child.error());
            node.children.push_back(std::move(*child));
        }
    }

    return node;
}

} // namespace

tl::expected<schema::SchemaConfig, Error> parse_xml_config(const std::filesystem::path& path) {
    auto doc_result = load_xml_document(path);
    if (!doc_result) {
        return tl::make_unexpected(doc_result.error());
    }

    auto root = doc_result->child("ICDConfig");
    if (!root) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing ICDConfig root", path, "ICDConfig"});
    }

    schema::SchemaConfig config;
    auto files = root.child("Files");
    if (!files) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing Files element", path, "Files"});
    }

    for (auto file_info : files.children("FileInfo")) {
        schema::SchemaFileEntry entry;
        auto name = file_info.child("Name");
        auto child_path = file_info.child("Path");
        if (!name || !child_path) {
            return tl::make_unexpected(Error{ErrorCode::schema_error, "missing config file field", path, "FileInfo"});
        }

        if (auto id = parse_optional_int(file_info, "ID")) {
            entry.id = *id;
        }
        entry.logical_name = name.child_value();
        if (auto description = file_info.child("Description")) {
            entry.description = description.child_value();
        }
        entry.path = child_path.child_value();
        if (auto type = parse_optional_int(file_info, "Type")) {
            entry.type = parse_frame_type(*type);
        }
        if (auto order = parse_optional_int(file_info, "ByteOrder")) {
            entry.order = parse_byte_order(*order);
        }
        entry.format = Format::xml;
        config.files.push_back(std::move(entry));
    }

    return config;
}

tl::expected<schema::SchemaFrameDef, Error> parse_xml_frame(const std::filesystem::path& path) {
    auto doc_result = load_xml_document(path);
    if (!doc_result) {
        return tl::make_unexpected(doc_result.error());
    }

    auto root = doc_result->child("ICDData");
    if (!root) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing ICDData root", path, "ICDData"});
    }

    auto name = root.child("Name");
    auto data = root.child("Data");
    if (!name || !data) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing frame content", path, "ICDData"});
    }

    schema::SchemaFrameDef frame;
    frame.name = name.child_value();

    for (auto item : data.children("Item")) {
        auto node = parse_node(item, path);
        if (!node) {
            return tl::make_unexpected(node.error());
        }
        frame.roots.push_back(std::move(*node));
    }

    if (frame.roots.empty()) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "frame has no root items", path, frame.name});
    }

    return frame;
}

} // namespace icd::format
