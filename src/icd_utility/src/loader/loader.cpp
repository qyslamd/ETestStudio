#include <icd/loader.hpp>

#include "../format/json_parser.hpp"
#include "../format/xml_parser.hpp"
#include "../schema/builder.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace icd {
namespace {

Format detect_format(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (extension == ".xml") return Format::xml;
    if (extension == ".json") return Format::json;
    return Format::auto_detect;
}

tl::expected<schema::SchemaConfig, Error> parse_config(const std::filesystem::path& path, Format format) {
    switch (format) {
    case Format::xml:
        return format::parse_xml_config(path);
    case Format::json:
        return format::parse_json_config(path);
    case Format::auto_detect:
    default:
        return tl::make_unexpected(Error{ErrorCode::unsupported_format, "unsupported config format", path, {}});
    }
}

tl::expected<schema::SchemaFrameDef, Error> parse_frame(const std::filesystem::path& path, Format format) {
    switch (format) {
    case Format::xml:
        return format::parse_xml_frame(path);
    case Format::json:
        return format::parse_json_frame(path);
    case Format::auto_detect:
    default:
        return tl::make_unexpected(Error{ErrorCode::unsupported_format, "unsupported frame format", path, {}});
    }
}

} // namespace

tl::expected<Repository, Error> Loader::init(const std::filesystem::path& config_path, Format format) {
    const auto effective_format = (format == Format::auto_detect) ? detect_format(config_path) : format;
    if (effective_format == Format::auto_detect) {
        return tl::make_unexpected(Error{ErrorCode::unsupported_format, "unsupported config format", config_path, {}});
    }

    auto config = parse_config(config_path, effective_format);
    if (!config) {
        return tl::make_unexpected(config.error());
    }

    schema::SchemaConfig merged;
    merged.files = config->files;

    const auto base_dir = config_path.parent_path();
    for (const auto& file_entry : config->files) {
        const auto frame_path = base_dir / file_entry.path;
        const auto frame_format = (effective_format == Format::auto_detect) ? detect_format(frame_path) : effective_format;
        auto frame = parse_frame(frame_path, frame_format);
        if (!frame) {
            return tl::make_unexpected(frame.error());
        }

        if (file_entry.id.has_value()) {
            frame->id = *file_entry.id;
        }
        if (!file_entry.logical_name.empty()) {
            frame->name = file_entry.logical_name;
        }
        if (!file_entry.description.empty()) {
            frame->description = file_entry.description;
        }
        if (file_entry.type.has_value()) {
            frame->type = *file_entry.type;
        }
        if (file_entry.order.has_value()) {
            frame->order = *file_entry.order;
        }

        merged.frames.push_back(std::move(*frame));
    }

    return schema::build_repository(merged);
}

} // namespace icd
