#include "json_serializer.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

} // namespace icd::format
