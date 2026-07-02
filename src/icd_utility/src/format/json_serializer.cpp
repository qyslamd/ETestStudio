#include "json_serializer.hpp"

#include "type_mapping.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <icd/file_entry.hpp>
#include <icd/node.hpp>
#include <icd/frame.hpp>
#include <icd/repository.hpp>

namespace icd::format {
namespace {

using json = nlohmann::json;

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

void update_max_bits(const Node& node, int& max_bits) {
    int node_end = (node.offset() * 8) + node.bit_offset() + node.bit_width();
    if (node_end > max_bits) {
        max_bits = node_end;
    }
    for (const auto& child : node.children()) {
        update_max_bits(*child, max_bits);
    }
}

json serialize_node(const Node& node) {
    json obj;

    obj["name"]        = std::string(node.name());
    obj["description"] = std::string(node.description());
    obj["offset"]      = node.offset();
    obj["startBit"]    = node.bit_offset();
    obj["bitWidth"]    = node.bit_width();
    obj["valueType"]   = value_type_to_string(node.value_type());
    obj["tag"]         = static_cast<int>(node.tag());

    // Attrs
    json attrs_obj;
    const auto& attrs = node.attrs();
    attrs_obj["systemName"]     = attrs.system_name;
    attrs_obj["groupName"]      = attrs.group_name;
    attrs_obj["unit"]           = attrs.unit;
    attrs_obj["valueTextList"]  = attrs.value_text_list;
    attrs_obj["scaleFormula"]   = attrs.scale_formula;
    attrs_obj["scaleConveror"]  = attrs.scale_convertor;
    attrs_obj["linkTo"]         = attrs.link_to;
    attrs_obj["isScaled"]       = attrs.is_scaled;

    if (attrs.scale_a.has_value()) attrs_obj["scaleA"] = *attrs.scale_a;
    if (attrs.scale_b.has_value()) attrs_obj["scaleB"] = *attrs.scale_b;
    if (attrs.min.has_value())     attrs_obj["min"]    = *attrs.min;
    if (attrs.max.has_value())     attrs_obj["max"]    = *attrs.max;

    obj["attrs"] = std::move(attrs_obj);

    // Children
    if (!node.children().empty()) {
        json children = json::array();
        for (const auto& child : node.children()) {
            children.push_back(serialize_node(*child));
        }
        obj["children"] = std::move(children);
    }

    return obj;
}

json serialize_frame_def(const Frame& frame) {
    json obj;

    obj["id"]          = frame.id();
    obj["name"]        = std::string(frame.name());
    obj["description"] = std::string(frame.description());

    // Map FrameType
    switch (frame.type()) {
    case FrameType::cmd:      obj["type"] = "cmd";      break;
    case FrameType::data:     obj["type"] = "data";     break;
    case FrameType::data_cmd: obj["type"] = "dataCfg";  break;
    }

    // Map ByteOrder
    obj["byteOrder"] = (frame.order() == ByteOrder::little_endian)
                           ? "littleEndian"
                           : "bigEndian";

    // Calculate frame length from max node extent
    int max_bits = 0;
    for (const auto& root : frame.roots()) {
        update_max_bits(*root, max_bits);
    }
    obj["length"] = (max_bits + 7) / 8;  // Ceiling divide to bytes

    // Nodes
    json nodes = json::array();
    for (const auto& root : frame.roots()) {
        nodes.push_back(serialize_node(*root));
    }
    obj["nodes"] = std::move(nodes);

    return obj;
}

tl::expected<void, Error> write_json(const json& document, const std::filesystem::path& path) {
    std::ofstream stream(path);
    if (!stream.is_open()) {
        return tl::make_unexpected(
            Error{ErrorCode::io_error, "failed to open file for writing", path, {}});
    }
    try {
        stream << document.dump(2);
    } catch (const std::exception& ex) {
        return tl::make_unexpected(
            Error{ErrorCode::io_error, ex.what(), path, {}});
    }
    return {};
}

} // namespace

nlohmann::json serialize_repository_to_json(const Repository& repo) {
    json document;
    document["version"] = "1.0";

    json frames = json::array();
    for (const auto& frame : repo.frames()) {
        frames.push_back(serialize_frame_def(*frame));
    }
    document["frames"] = std::move(frames);

    return document;
}

tl::expected<void, Error> serialize_repository(const std::filesystem::path& path, const Repository& repo) {
    return write_json(serialize_repository_to_json(repo), path);
}

tl::expected<void, Error> serialize_frame(const std::filesystem::path& path, const Frame& frame) {
    json document;
    document["version"] = "1.0";

    json frames = json::array();
    frames.push_back(serialize_frame_def(frame));
    document["frames"] = std::move(frames);

    return write_json(document, path);
}

// ============================================================================
// Legacy JSON format (flat attrs, "childs" array, legacy type names)
// ============================================================================

namespace {

json serialize_legacy_node(const Node& node) {
    json obj;

    obj["name"]        = std::string(node.name());
    obj["description"] = std::string(node.description());
    obj["offset"]      = node.offset();
    obj["startBit"]    = node.bit_offset();
    obj["bitWidth"]    = node.bit_width();
    obj["type"]        = value_type_to_legacy_json_string(node.value_type());
    obj["tag"]         = tag_to_legacy_int(node.tag());
    obj["isScaled"]    = node.attrs().is_scaled ? 1 : 0;

    // Flat attributes
    const auto& attrs = node.attrs();
    if (!attrs.system_name.empty())     obj["systemName"]     = attrs.system_name;
    if (!attrs.group_name.empty())      obj["groupName"]      = attrs.group_name;
    if (!attrs.unit.empty())            obj["unit"]            = attrs.unit;
    if (!attrs.value_text_list.empty()) obj["valueTextList"]   = attrs.value_text_list;
    if (!attrs.scale_formula.empty())   obj["scaleFormula"]    = attrs.scale_formula;
    if (!attrs.scale_convertor.empty()) obj[kScaleConverorKey] = attrs.scale_convertor;
    if (!attrs.link_to.empty())         obj["linkTo"]          = attrs.link_to;

    if (attrs.scale_a.has_value()) obj["scaleA"] = *attrs.scale_a;
    if (attrs.scale_b.has_value()) obj["scaleB"] = *attrs.scale_b;
    if (attrs.min.has_value())     obj["min"]    = *attrs.min;
    if (attrs.max.has_value())     obj["max"]    = *attrs.max;

    // Legacy format uses "childs" (not "children")
    if (!node.children().empty()) {
        json childs = json::array();
        for (const auto& child : node.children()) {
            childs.push_back(serialize_legacy_node(*child));
        }
        obj["childs"] = std::move(childs);
    }

    return obj;
}

} // anonymous namespace

tl::expected<void, Error> serialize_json_config(const std::filesystem::path& path,
                                                  const std::vector<FrameFileInfo>& file_entries) {
    json document;
    document["version"] = "1.0";

    json files = json::array();
    for (const auto& entry : file_entries) {
        json fi;
        fi["name"]      = entry.name;
        fi["path"]      = entry.path;
        fi["id"]        = entry.id;
        if (!entry.description.empty()) {
            fi["description"] = entry.description;
        }

        // Legacy integer encoding for FrameType/ByteOrder
        int type_int = 1;
        switch (entry.type) {
        case FrameType::cmd:      type_int = 2; break;
        case FrameType::data_cmd: type_int = 4; break;
        default:                  type_int = 1; break;
        }
        fi["type"] = type_int;

        fi["byteOrder"] = (entry.order == ByteOrder::big_endian) ? 1 : 0;
        fi["enable"] = entry.enable;
        fi["wordType"] = entry.word_type;

        files.push_back(std::move(fi));
    }
    document["files"] = std::move(files);

    // Atomically write via temp file + rename
    auto tmp_path = path;
    tmp_path += ".tmp";
    auto save_result = write_json(document, tmp_path);
    if (!save_result) {
        return save_result;
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec) {
        std::filesystem::remove(tmp_path, ec);
        return tl::make_unexpected(
            Error{ErrorCode::io_error, "failed to rename config file: " + ec.message(), path, {}});
    }

    return {};
}

tl::expected<void, Error> serialize_json_frame_file(const std::filesystem::path& path,
                                                      const Frame& frame) {
    json document;
    document["version"] = "1.0";
    document["name"]    = std::string(frame.name());

    json data = json::array();
    for (const auto& root_node : frame.roots()) {
        data.push_back(serialize_legacy_node(*root_node));
    }
    document["data"] = std::move(data);

    return write_json(document, path);
}

} // namespace icd::format
