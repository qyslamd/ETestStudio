#pragma once

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <filesystem>

#include <icd/error.hpp>

namespace icd {

class Repository;
class Frame;
class Node;

namespace format {

// Serialize full repository to JSON object (for undo snapshots, etc.)
nlohmann::json serialize_repository_to_json(const Repository& repo);

// Serialize full repository (as .eproto JSON format)
tl::expected<void, Error> serialize_repository(const std::filesystem::path& path, const Repository& repo);

// Serialize single frame (for internal use)
tl::expected<void, Error> serialize_frame(const std::filesystem::path& path, const Frame& frame);

} // namespace format
} // namespace icd
