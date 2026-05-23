#include "json_parser.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <utility>

namespace icd::format {
namespace {

using json = nlohmann::json;

tl::expected<json, Error> load_json_document(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return tl::make_unexpected(Error{ErrorCode::io_error, "failed to open json file", path, {}});
    }

    try {
        json document;
        stream >> document;
        return document;
    } catch (const json::parse_error& ex) {
        return tl::make_unexpected(Error{ErrorCode::parse_error, ex.what(), path, {}});
    } catch (const std::exception& ex) {
        return tl::make_unexpected(Error{ErrorCode::parse_error, ex.what(), path, {}});
    }
}

tl::expected<int, Error> required_int(const json& object, const char* key, const std::filesystem::path& path) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing required integer field", path, key});
    }
    return it->get<int>();
}

tl::expected<float, Error> optional_float(const json& object, const char* key, const std::filesystem::path& path) {
    auto it = object.find(key);
    if (it == object.end()) {
        return tl::make_unexpected(Error{ErrorCode::not_found, "optional float not found", path, key});
    }
    if (!it->is_number()) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "invalid float field", path, key});
    }
    return it->get<float>();
}

ValueType parse_value_type(std::string_view type) noexcept {
    if (type == "byte") return ValueType::byte_;
    if (type == "bytes") return ValueType::bytes;
    if (type == "word") return ValueType::word;
    if (type == "float") return ValueType::single;
    if (type == "double") return ValueType::double_;
    if (type == "string") return ValueType::string_;
    if (type == "int") return ValueType::integer;
    if (type == "int16") return ValueType::shortint;
    if (type == "smallint") return ValueType::smallint;
    return ValueType::unknown;
}

tl::expected<schema::SchemaNodeDef, Error> parse_node(const json& item, const std::filesystem::path& path) {
    schema::SchemaNodeDef node;

    auto offset = required_int(item, "offset", path);
    if (!offset) return tl::make_unexpected(offset.error());
    node.offset = *offset;

    auto start_bit = required_int(item, "startBit", path);
    if (!start_bit) return tl::make_unexpected(start_bit.error());
    node.bit_offset = *start_bit;

    auto bit_width = required_int(item, "bitWidth", path);
    if (!bit_width) return tl::make_unexpected(bit_width.error());
    node.bit_width = *bit_width;

    if (!item.contains("type") || !item["type"].is_string() ||
        !item.contains("name") || !item["name"].is_string() ||
        !item.contains("description") || !item["description"].is_string() ||
        !item.contains("isScaled") || !item["isScaled"].is_number_integer()) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing required node field", path, "item"});
    }

    node.value_type = parse_value_type(item["type"].get<std::string>());
    node.name = item["name"].get<std::string>();
    node.description = item["description"].get<std::string>();
    node.attrs.is_scaled = (item["isScaled"].get<int>() != 0);

    if (auto it = item.find("groupName"); it != item.end() && it->is_string()) node.attrs.group_name = it->get<std::string>();
    if (auto it = item.find("systemName"); it != item.end() && it->is_string()) node.attrs.system_name = it->get<std::string>();
    if (auto it = item.find("unit"); it != item.end() && it->is_string()) node.attrs.unit = it->get<std::string>();
    if (auto it = item.find("valueTextList"); it != item.end() && it->is_string()) node.attrs.value_text_list = it->get<std::string>();
    if (auto it = item.find("scaleFormula"); it != item.end() && it->is_string()) node.attrs.scale_formula = it->get<std::string>();
    if (auto it = item.find("scaleConveror"); it != item.end() && it->is_string()) node.attrs.scale_convertor = it->get<std::string>();
    if (auto it = item.find("linkTo"); it != item.end() && it->is_string()) node.attrs.link_to = it->get<std::string>();
    if (auto it = item.find("tag"); it != item.end() && it->is_number_integer()) node.tag = static_cast<Tag>(it->get<int>());

    if (auto value = optional_float(item, "scaleA", path); value.has_value()) node.attrs.scale_a = *value;
    if (auto value = optional_float(item, "scaleB", path); value.has_value()) node.attrs.scale_b = *value;
    if (auto value = optional_float(item, "min", path); value.has_value()) node.attrs.min = *value;
    if (auto value = optional_float(item, "max", path); value.has_value()) node.attrs.max = *value;

    if (auto it = item.find("childs"); it != item.end()) {
        if (!it->is_array()) {
            return tl::make_unexpected(Error{ErrorCode::schema_error, "childs must be an array", path, node.name});
        }
        for (const auto& child_item : *it) {
            auto child = parse_node(child_item, path);
            if (!child) return tl::make_unexpected(child.error());
            node.children.push_back(std::move(*child));
        }
    }

    return node;
}

} // namespace

tl::expected<schema::SchemaConfig, Error> parse_json_config(const std::filesystem::path& path) {
    auto document = load_json_document(path);
    if (!document) {
        return tl::make_unexpected(document.error());
    }

    if (!document->contains("files") || !(*document)["files"].is_array()) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing files array", path, "files"});
    }

    schema::SchemaConfig config;
    for (const auto& file_info : (*document)["files"]) {
        if (!file_info.contains("name") || !file_info["name"].is_string() ||
            !file_info.contains("path") || !file_info["path"].is_string()) {
            return tl::make_unexpected(Error{ErrorCode::schema_error, "missing config file field", path, "files"});
        }

        schema::SchemaFileEntry entry;
        if (auto it = file_info.find("id"); it != file_info.end() && it->is_number_integer()) {
            entry.id = it->get<int>();
        }
        entry.logical_name = file_info["name"].get<std::string>();
        if (auto it = file_info.find("description"); it != file_info.end() && it->is_string()) {
            entry.description = it->get<std::string>();
        }
        entry.path = file_info["path"].get<std::string>();
        if (auto it = file_info.find("type"); it != file_info.end() && it->is_number_integer()) {
            switch (it->get<int>()) {
            case 2:
                entry.type = FrameType::cmd;
                break;
            case 4:
                entry.type = FrameType::data_cmd;
                break;
            default:
                entry.type = FrameType::data;
                break;
            }
        }
        if (auto it = file_info.find("byteOrder"); it != file_info.end() && it->is_number_integer()) {
            entry.order = (it->get<int>() == 1) ? ByteOrder::big_endian : ByteOrder::little_endian;
        }
        entry.format = Format::json;
        config.files.push_back(std::move(entry));
    }

    if (config.files.empty()) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "config contains no file entries", path, "files"});
    }

    return config;
}

tl::expected<schema::SchemaFrameDef, Error> parse_json_frame(const std::filesystem::path& path) {
    auto document = load_json_document(path);
    if (!document) {
        return tl::make_unexpected(document.error());
    }

    if (!document->contains("name") || !(*document)["name"].is_string() ||
        !document->contains("data") || !(*document)["data"].is_array()) {
        return tl::make_unexpected(Error{ErrorCode::schema_error, "missing frame content", path, "frame"});
    }

    schema::SchemaFrameDef frame;
    frame.name = (*document)["name"].get<std::string>();

    for (const auto& item : (*document)["data"]) {
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
