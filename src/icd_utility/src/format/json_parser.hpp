#pragma once

#include <tl/expected.hpp>

#include <filesystem>

#include <icd/error.hpp>

#include "../schema/schema.hpp"

namespace icd::format {

tl::expected<schema::SchemaConfig, Error> parse_json_config(const std::filesystem::path& path);
tl::expected<schema::SchemaFrameDef, Error> parse_json_frame(const std::filesystem::path& path);

} // namespace icd::format
