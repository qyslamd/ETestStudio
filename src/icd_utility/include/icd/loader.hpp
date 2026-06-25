#pragma once

#include <tl/expected.hpp>

#include <filesystem>
#include <vector>

#include <icd/export.hpp>
#include <icd/error.hpp>
#include <icd/file_entry.hpp>
#include <icd/repository.hpp>
#include <icd/types.hpp>

namespace icd {

struct ICD_UTILITY_API LoadResult {
    Repository repository;
    std::filesystem::path config_path;
    Format format {};                                   // 始终为 xml 或 json，不会是 auto_detect
    std::vector<FrameFileInfo> file_entries;            // 每帧的文件路径和元数据
};

class ICD_UTILITY_API Loader {
public:
    static tl::expected<Repository, Error>
    init(const std::filesystem::path& config_path,
         Format format = Format::auto_detect);

    static tl::expected<LoadResult, Error>
    init_with_metadata(const std::filesystem::path& config_path,
                       Format format = Format::auto_detect);
};

} // namespace icd
