#pragma once

#include <icd/export.hpp>
#include <icd/types.hpp>

#include <cstdint>
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

    // 保留 ICDConfig.xsd 中 FileInfo 的扩展元数据，用于写回保真。
    // 历史样本中 Enable 缺省视为启用（true），WordType 缺省为 0。
    bool enable {true};
    unsigned int word_type {0};
};

} // namespace icd
