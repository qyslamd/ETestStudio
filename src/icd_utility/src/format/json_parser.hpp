#pragma once

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <filesystem>

#include <icd/error.hpp>

#include "../schema/schema.hpp"

namespace icd {

class Repository;

namespace format {

tl::expected<schema::SchemaConfig, Error> parse_json_config(const std::filesystem::path& path);
tl::expected<schema::SchemaFrameDef, Error> parse_json_frame(const std::filesystem::path& path);

// Deserialize .eproto JSON format back into a Repository
tl::expected<icd::Repository, Error> deserialize_repository(const std::filesystem::path& path);
tl::expected<icd::Repository, Error> deserialize_repository(const nlohmann::json& j);

} // namespace format
} // namespace icd
