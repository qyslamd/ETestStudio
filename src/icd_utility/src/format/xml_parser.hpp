#pragma once

#include <tl/expected.hpp>

#include <filesystem>

#include <icd/error.hpp>

#include "../schema/schema.hpp"

namespace icd::format {

tl::expected<schema::SchemaConfig, Error> parse_xml_config(const std::filesystem::path& path);
tl::expected<schema::SchemaFrameDef, Error> parse_xml_frame(const std::filesystem::path& path);

} // namespace icd::format
