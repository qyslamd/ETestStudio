#pragma once

#include <tl/expected.hpp>

#include <filesystem>

#include <icd/export.hpp>
#include <icd/error.hpp>
#include <icd/repository.hpp>
#include <icd/types.hpp>

namespace icd {

class ICD_UTILITY_API Loader {
public:
    static tl::expected<Repository, Error>
    init(const std::filesystem::path& config_path,
         Format format = Format::auto_detect);
};

} // namespace icd
