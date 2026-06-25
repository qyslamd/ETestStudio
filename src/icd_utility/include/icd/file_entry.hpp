#pragma once

#include <icd/export.hpp>
#include <icd/types.hpp>

#include <string>

namespace icd {

struct ICD_UTILITY_API FrameFileInfo {
    int id {0};
    std::string name;
    std::string description;
    std::string path;               // 相对于 config 文件的路径
    FrameType type {FrameType::data};
    ByteOrder order {ByteOrder::little_endian};
    Format format {Format::xml};
};

} // namespace icd
