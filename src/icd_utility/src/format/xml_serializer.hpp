#pragma once

#include <tl/expected.hpp>

#include <filesystem>
#include <vector>

#include <icd/error.hpp>
#include <icd/file_entry.hpp>
#include <icd/repository.hpp>

namespace icd {

class Frame;

namespace format {

// Deserialize .eprotox XML format into Repository
tl::expected<Repository, Error> deserialize_xml_repository(const std::filesystem::path& path);

// Serialize Repository as .eprotox XML format
tl::expected<void, Error> serialize_xml_repository(const std::filesystem::path& path, const Repository& repo);

// Legacy ICDConfig XML serialization
tl::expected<void, Error> serialize_xml_config(const std::filesystem::path& path,
                                                const std::vector<FrameFileInfo>& file_entries);

// Legacy XML frame file serialization (ICDData format)
tl::expected<void, Error> serialize_xml_frame_file(const std::filesystem::path& path,
                                                    const Frame& frame);

} // namespace format
} // namespace icd
