#pragma once

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <filesystem>
#include <vector>

#include <icd/error.hpp>
#include <icd/file_entry.hpp>

namespace icd {

class Frame;
class Repository;
class Node;

namespace format {

// Serialize full repository to JSON object (for undo snapshots, etc.)
nlohmann::json serialize_repository_to_json(const Repository& repo);

// Serialize full repository (as .eproto JSON format)
tl::expected<void, Error> serialize_repository(const std::filesystem::path& path, const Repository& repo);

// Serialize single frame (for internal use)
tl::expected<void, Error> serialize_frame(const std::filesystem::path& path, const Frame& frame);

// Legacy ICDConfig JSON serialization
tl::expected<void, Error> serialize_json_config(const std::filesystem::path& path,
                                                  const std::vector<FrameFileInfo>& file_entries);

// Legacy JSON frame file serialization
tl::expected<void, Error> serialize_json_frame_file(const std::filesystem::path& path,
                                                      const Frame& frame);

} // namespace format
} // namespace icd
