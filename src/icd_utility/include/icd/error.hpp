#pragma once

#include <icd/export.hpp>

#include <filesystem>
#include <string>

namespace icd {

enum class ErrorCode {
    invalid_argument,
    io_error,
    parse_error,
    schema_error,
    duplicate_frame_id,
    duplicate_frame_name,
    invalid_node,
    unsupported_format,
    not_found
};

struct ICD_UTILITY_API Error {
    ErrorCode code {};
    std::string message;
    std::filesystem::path file;
    std::string path_hint;
};

} // namespace icd
