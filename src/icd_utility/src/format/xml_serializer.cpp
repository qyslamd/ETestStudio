#include "xml_serializer.hpp"

#include "type_mapping.hpp"
#include "xml_util.hpp"

#include <pugixml.hpp>

#include <icd/file_entry.hpp>
#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace icd::format {
namespace {

// ── Helper: parse a required integer child element ──────────────
tl::expected<int, Error> parse_required_int(const pugi::xml_node& node,
                                            const char* child_name,
                                            const std::filesystem::path& file) {
    auto child = node.child(child_name);
    if (!child) {
        return tl::make_unexpected(
            Error{ErrorCode::schema_error, "missing required integer field", file, child_name});
    }
    try {
        return std::stoi(child.child_value());
    } catch (...) {
        return tl::make_unexpected(
            Error{ErrorCode::schema_error, "invalid integer value", file, child_name});
    }
}

// ── Helper: parse a required string child element ───────────────
tl::expected<std::string, Error> parse_required_string(const pugi::xml_node& node,
                                                       const char* child_name,
                                                       const std::filesystem::path& file) {
    auto child = node.child(child_name);
    if (!child) {
        return tl::make_unexpected(
            Error{ErrorCode::schema_error, "missing required string field", file, child_name});
    }
    return std::string(child.child_value());
}

// ── Helper: parse an optional string child element ─────────────
std::string parse_optional_string(const pugi::xml_node& node, const char* child_name) {
    auto child = node.child(child_name);
    if (!child) {
        return {};
    }
    return child.child_value();
}

// ── Helper: parse an optional float child element ──────────────
std::optional<float> parse_optional_float(const pugi::xml_node& node, const char* child_name) {
    auto child = node.child(child_name);
    if (!child) {
        return std::nullopt;
    }
    try {
        return std::stof(child.child_value());
    } catch (...) {
        return std::nullopt;
    }
}

// ── Helper: parse IsScaled (supports "true"/"false" and "0"/"1") ─
bool parse_is_scaled(const pugi::xml_node& node, const std::filesystem::path& file) {
    auto child = node.child("IsScaled");
    if (!child) {
        return false;
    }
    std::string val = child.child_value();
    if (val == "true" || val == "1") {
        return true;
    }
    return false;
}

// ── Recursive node deserialization ─────────────────────────────
tl::expected<std::unique_ptr<Node>, Error> deserialize_node(const pugi::xml_node& item,
                                                            const std::filesystem::path& file) {
    // Required fields
    auto name = parse_required_string(item, "Name", file);
    if (!name) return tl::make_unexpected(name.error());

    auto description = parse_required_string(item, "Description", file);
    if (!description) return tl::make_unexpected(description.error());

    auto offset = parse_required_int(item, "Offset", file);
    if (!offset) return tl::make_unexpected(offset.error());

    auto start_bit = parse_required_int(item, "StartBit", file);
    if (!start_bit) return tl::make_unexpected(start_bit.error());

    auto bit_width = parse_required_int(item, "BitWidth", file);
    if (!bit_width) return tl::make_unexpected(bit_width.error());

    auto type_str = parse_required_string(item, "ValueType", file);
    if (!type_str) return tl::make_unexpected(type_str.error());

    // ValueType uses .eproto type system strings
    ValueType value_type = value_type_from_string(*type_str);
    if (value_type == ValueType::unknown && *type_str != "unknown") {
        // Try legacy XML format fallback
        value_type = value_type_from_legacy_xml_string(*type_str);
    }

    // Tag is direct integer
    Tag tag = Tag::none;
    if (auto tag_child = item.child("Tag")) {
        try {
            tag = static_cast<Tag>(std::stoi(tag_child.child_value()));
        } catch (...) {
            // Default to none
        }
    }

    // Parse Attrs wrapper
    NodeAttrs attrs;
    if (auto attrs_node = item.child("Attrs")) {
        attrs.system_name     = parse_optional_string(attrs_node, "SystemName");
        attrs.group_name      = parse_optional_string(attrs_node, "GroupName");
        attrs.unit            = parse_optional_string(attrs_node, "Unit");
        attrs.value_text_list = parse_optional_string(attrs_node, "ValueTextList");
        attrs.scale_formula   = parse_optional_string(attrs_node, "ScaleFormula");
        attrs.scale_convertor = parse_optional_string(attrs_node, kScaleConverorKey);
        attrs.link_to         = parse_optional_string(attrs_node, "LinkTo");
        attrs.is_scaled       = parse_is_scaled(attrs_node, file);

        if (auto v = parse_optional_float(attrs_node, "ScaleA")) attrs.scale_a = v;
        if (auto v = parse_optional_float(attrs_node, "ScaleB")) attrs.scale_b = v;
        if (auto v = parse_optional_float(attrs_node, "Min"))    attrs.min = v;
        if (auto v = parse_optional_float(attrs_node, "Max"))    attrs.max = v;
    }

    auto node = std::make_unique<Node>(
        *name, *description, *offset, *start_bit, *bit_width,
        value_type, tag, std::move(attrs));

    // Parse Children
    if (auto children_node = item.child("Children")) {
        for (auto child_item : children_node.children("Item")) {
            auto child = deserialize_node(child_item, file);
            if (!child) return tl::make_unexpected(child.error());
            node->add_child(std::move(*child));
        }
    }

    return node;
}

tl::expected<std::unique_ptr<Node>, Error> deserialize_legacy_node(const pugi::xml_node& item,
                                                                   const std::filesystem::path& file) {
    auto name = parse_required_string(item, "Name", file);
    if (!name) return tl::make_unexpected(name.error());

    auto description = parse_required_string(item, "Description", file);
    if (!description) return tl::make_unexpected(description.error());

    auto offset = parse_required_int(item, "Offset", file);
    if (!offset) return tl::make_unexpected(offset.error());

    auto start_bit = parse_required_int(item, "StartBit", file);
    if (!start_bit) return tl::make_unexpected(start_bit.error());

    auto bit_width = parse_required_int(item, "BitWidth", file);
    if (!bit_width) return tl::make_unexpected(bit_width.error());

    auto type_str = parse_required_string(item, "Type", file);
    if (!type_str) return tl::make_unexpected(type_str.error());

    NodeAttrs attrs;
    attrs.system_name     = parse_optional_string(item, "SystemName");
    attrs.group_name      = parse_optional_string(item, "GroupName");
    attrs.unit            = parse_optional_string(item, "Unit");
    attrs.value_text_list = parse_optional_string(item, "ValueTextList");
    attrs.scale_formula   = parse_optional_string(item, "ScaleFormula");
    attrs.scale_convertor = parse_optional_string(item, kScaleConverorKey);
    attrs.link_to         = parse_optional_string(item, "LinkTo");
    attrs.is_scaled       = parse_is_scaled(item, file);

    if (auto v = parse_optional_float(item, "ScaleA")) attrs.scale_a = v;
    if (auto v = parse_optional_float(item, "ScaleB")) attrs.scale_b = v;
    if (auto v = parse_optional_float(item, "Min"))    attrs.min = v;
    if (auto v = parse_optional_float(item, "Max"))    attrs.max = v;

    Tag tag = Tag::none;
    if (auto tag_child = item.child("Tag")) {
        try {
            tag = tag_from_legacy_int(std::stoi(tag_child.child_value()));
        } catch (...) {
            tag = Tag::none;
        }
    }

    auto node = std::make_unique<Node>(
        *name, *description, *offset, *start_bit, *bit_width,
        value_type_from_legacy_xml_string(*type_str), tag, std::move(attrs));

    if (auto childs_node = item.child("Childs")) {
        for (auto child_item : childs_node.children("Item")) {
            auto child = deserialize_legacy_node(child_item, file);
            if (!child) return tl::make_unexpected(child.error());
            node->add_child(std::move(*child));
        }
    }

    return node;
}
// ── Calculate frame length from max node bit extent ────────────
void update_max_bits(const Node& node, int& max_bits) {
    int node_end = (node.offset() * 8) + node.bit_offset() + node.bit_width();
    if (node_end > max_bits) {
        max_bits = node_end;
    }
    for (const auto& child : node.children()) {
        update_max_bits(*child, max_bits);
    }
}

// ── Recursive node serialization ───────────────────────────────
void serialize_node(const Node& node, pugi::xml_node& parent) {
    auto item = parent.append_child("Item");

    item.append_child("Name").text()        = std::string(node.name()).c_str();
    item.append_child("Description").text() = std::string(node.description()).c_str();
    item.append_child("Offset").text()      = node.offset();
    item.append_child("StartBit").text()    = node.bit_offset();
    item.append_child("BitWidth").text()    = node.bit_width();
    item.append_child("ValueType").text()   = value_type_to_string(node.value_type()).c_str();
    item.append_child("Tag").text()         = static_cast<int>(node.tag());

    // Serialize Attrs
    auto attrs_node = item.append_child("Attrs");
    const auto& attrs = node.attrs();
    attrs_node.append_child("SystemName").text()     = attrs.system_name.c_str();
    attrs_node.append_child("GroupName").text()      = attrs.group_name.c_str();
    attrs_node.append_child("Unit").text()            = attrs.unit.c_str();
    attrs_node.append_child("ValueTextList").text()   = attrs.value_text_list.c_str();
    attrs_node.append_child("ScaleFormula").text()    = attrs.scale_formula.c_str();
    attrs_node.append_child(kScaleConverorKey).text() = attrs.scale_convertor.c_str();
    attrs_node.append_child("LinkTo").text()          = attrs.link_to.c_str();
    attrs_node.append_child("IsScaled").text()        = attrs.is_scaled ? "true" : "false";

    if (attrs.scale_a.has_value()) attrs_node.append_child("ScaleA").text() = *attrs.scale_a;
    if (attrs.scale_b.has_value()) attrs_node.append_child("ScaleB").text() = *attrs.scale_b;
    if (attrs.min.has_value())     attrs_node.append_child("Min").text()    = *attrs.min;
    if (attrs.max.has_value())     attrs_node.append_child("Max").text()    = *attrs.max;

    // Serialize Children
    if (!node.children().empty()) {
        auto children_node = item.append_child("Children");
        for (const auto& child : node.children()) {
            serialize_node(*child, children_node);
        }
    }
}

// ── Serialize a single frame to XML ────────────────────────────
void serialize_frame(const Frame& frame, pugi::xml_node& parent) {
    auto frame_node = parent.append_child("Frame");

    frame_node.append_child("ID").text()          = frame.id();
    frame_node.append_child("Name").text()        = std::string(frame.name()).c_str();
    frame_node.append_child("Description").text() = std::string(frame.description()).c_str();
    frame_node.append_child("Type").text()        = frame_type_to_string(frame.type()).c_str();
    frame_node.append_child("ByteOrder").text()   = byte_order_to_string(frame.order()).c_str();

    // Calculate Length from max node bit extent
    int max_bits = 0;
    for (const auto& root : frame.roots()) {
        update_max_bits(*root, max_bits);
    }
    int length = (max_bits + 7) / 8;  // Ceiling divide to bytes
    frame_node.append_child("Length").text() = length;

    // Serialize Nodes
    auto nodes_node = frame_node.append_child("Nodes");
    for (const auto& root : frame.roots()) {
        serialize_node(*root, nodes_node);
    }
}

} // anonymous namespace

// ── Deserialize .eprotox XML into Repository ───────────────────
tl::expected<Repository, Error> deserialize_xml_repository(const std::filesystem::path& path) {
    auto doc_result = load_xml_document(path);
    if (!doc_result) {
        return tl::make_unexpected(doc_result.error());
    }

    const auto& doc = *doc_result;

    Repository repo;

    if (auto legacy_root = doc.child("ICDData")) {
        auto name = legacy_root.child("Name");
        auto data = legacy_root.child("Data");
        if (!name || !data) {
            return tl::make_unexpected(
                Error{ErrorCode::schema_error, "missing frame content", path, "ICDData"});
        }

        auto frame = std::make_unique<Frame>(
            0, name.child_value(), std::string{}, FrameType::data, ByteOrder::little_endian);
        for (auto item : data.children("Item")) {
            auto node_result = deserialize_legacy_node(item, path);
            if (!node_result) return tl::make_unexpected(node_result.error());
            frame->add_root(std::move(*node_result));
        }
        if (frame->roots().empty()) {
            return tl::make_unexpected(
                Error{ErrorCode::schema_error, "frame has no root items", path, std::string(frame->name())});
        }
        repo.add_frame(std::move(frame));
        return repo;
    }

    // Check root element
    auto root = doc.child("ICDProtocol");
    if (!root) {
        return tl::make_unexpected(
            Error{ErrorCode::schema_error, "missing ICDProtocol root element", path, "ICDProtocol"});
    }

    for (auto frame_elem : root.children("Frame")) {
        // Parse required fields
        auto id_result = parse_required_int(frame_elem, "ID", path);
        if (!id_result) return tl::make_unexpected(id_result.error());

        auto name_result = parse_required_string(frame_elem, "Name", path);
        if (!name_result) return tl::make_unexpected(name_result.error());

        auto desc_result = parse_required_string(frame_elem, "Description", path);
        if (!desc_result) return tl::make_unexpected(desc_result.error());

        // Type (string) - optional, default to data
        FrameType type = FrameType::data;
        if (auto type_child = frame_elem.child("Type")) {
            type = frame_type_from_string(type_child.child_value());
        }

        // ByteOrder (string) - optional, default to littleEndian
        ByteOrder order = ByteOrder::little_endian;
        if (auto order_child = frame_elem.child("ByteOrder")) {
            order = byte_order_from_string(order_child.child_value());
        }

        auto frame = std::make_unique<Frame>(
            *id_result, *name_result, *desc_result, type, order);

        // Parse Nodes
        if (auto nodes_elem = frame_elem.child("Nodes")) {
            for (auto item : nodes_elem.children("Item")) {
                auto node_result = deserialize_node(item, path);
                if (!node_result) return tl::make_unexpected(node_result.error());
                frame->add_root(std::move(*node_result));
            }
        }

        repo.add_frame(std::move(frame));
    }

    return repo;
}

// ── Serialize Repository as .eprotox XML ───────────────────────
tl::expected<void, Error> serialize_xml_repository(const std::filesystem::path& path,
                                                    const Repository& repo) {
    pugi::xml_document doc;

    // Add XML declaration
    auto declaration = doc.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "UTF-8";

    // Root element
    auto root = doc.append_child("ICDProtocol");
    root.append_attribute("version") = "1.0";

    // Serialize each frame
    for (const auto& frame : repo.frames()) {
        serialize_frame(*frame, root);
    }

    // Save via std::ofstream (CJK path support)
    return save_xml_document(doc, path);
}

// ============================================================================
// Legacy XML format (ICDData root, byte/word/dword type system)
// ============================================================================

namespace {

void serialize_legacy_node(const Node& node, pugi::xml_node& parent) {
    auto item = parent.append_child("Item");

    item.append_child("Name").text()        = std::string(node.name()).c_str();
    item.append_child("Description").text() = std::string(node.description()).c_str();
    item.append_child("Offset").text()      = node.offset();
    item.append_child("StartBit").text()    = node.bit_offset();
    item.append_child("BitWidth").text()    = node.bit_width();
    item.append_child("Type").text()        = value_type_to_legacy_xml_string(node.value_type()).c_str();
    item.append_child("Tag").text()         = tag_to_legacy_int(node.tag());
    item.append_child("IsScaled").text()    = node.attrs().is_scaled ? 1 : 0;

    // Flat attributes (no Attrs wrapper in legacy format)
    const auto& attrs = node.attrs();
    if (!attrs.system_name.empty())     item.append_child("SystemName").text()     = attrs.system_name.c_str();
    if (!attrs.group_name.empty())      item.append_child("GroupName").text()      = attrs.group_name.c_str();
    if (!attrs.unit.empty())            item.append_child("Unit").text()            = attrs.unit.c_str();
    if (!attrs.value_text_list.empty()) item.append_child("ValueTextList").text()   = attrs.value_text_list.c_str();
    if (!attrs.scale_formula.empty())   item.append_child("ScaleFormula").text()    = attrs.scale_formula.c_str();
    if (!attrs.scale_convertor.empty()) item.append_child(kScaleConverorKey).text() = attrs.scale_convertor.c_str();
    if (!attrs.link_to.empty())         item.append_child("LinkTo").text()          = attrs.link_to.c_str();

    if (attrs.scale_a.has_value()) item.append_child("ScaleA").text() = *attrs.scale_a;
    if (attrs.scale_b.has_value()) item.append_child("ScaleB").text() = *attrs.scale_b;
    if (attrs.min.has_value())     item.append_child("Min").text()    = *attrs.min;
    if (attrs.max.has_value())     item.append_child("Max").text()    = *attrs.max;

    // Legacy format uses <Childs> (not <Children>)
    if (!node.children().empty()) {
        auto childs_node = item.append_child("Childs");
        for (const auto& child : node.children()) {
            serialize_legacy_node(*child, childs_node);
        }
    }
}

} // anonymous namespace

tl::expected<void, Error> serialize_xml_config(const std::filesystem::path& path,
                                                 const std::vector<FrameFileInfo>& file_entries) {
    pugi::xml_document doc;

    auto declaration = doc.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "UTF-8";

    auto root = doc.append_child("ICDConfig");
    auto files = root.append_child("Files");

    for (const auto& entry : file_entries) {
        auto fi = files.append_child("FileInfo");
        fi.append_child("Name").text()        = entry.name.c_str();
        fi.append_child("Path").text()        = entry.path.c_str();
        fi.append_child("ID").text()          = entry.id;
        if (!entry.description.empty()) {
            fi.append_child("Description").text() = entry.description.c_str();
        }

        // Legacy integer encoding for FrameType/ByteOrder
        int type_int = 1; // data
        switch (entry.type) {
        case FrameType::cmd:      type_int = 2; break;
        case FrameType::data_cmd: type_int = 4; break;
        default:                  type_int = 1; break;
        }
        fi.append_child("Type").text() = type_int;

        int order_int = (entry.order == ByteOrder::big_endian) ? 1 : 0;
        fi.append_child("ByteOrder").text() = order_int;
    }

    // Atomically write via temp file + rename
    auto tmp_path = path;
    tmp_path += ".tmp";
    auto save_result = save_xml_document(doc, tmp_path);
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

tl::expected<void, Error> serialize_xml_frame_file(const std::filesystem::path& path,
                                                     const Frame& frame) {
    pugi::xml_document doc;

    auto declaration = doc.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "UTF-8";

    auto root = doc.append_child("ICDData");
    root.append_child("Name").text() = std::string(frame.name()).c_str();

    auto data = root.append_child("Data");
    for (const auto& root_node : frame.roots()) {
        serialize_legacy_node(*root_node, data);
    }

    return save_xml_document(doc, path);
}

} // namespace icd::format
