#pragma once

#include <string>

#include <icd/frame.hpp>
#include <icd/node.hpp>

namespace etest::protocal::utils {

std::string valueTypeToString(icd::ValueType vt);
const char* valueTypeName(icd::ValueType vt);
icd::ValueType valueTypeFromName(const std::string& name);

const char* tagName(icd::Tag tag);
icd::Tag tagFromName(const std::string& name);

int frameTypeIndex(icd::FrameType ft);
icd::FrameType frameTypeFromIndex(int idx);

int byteOrderIndex(icd::ByteOrder bo);
icd::ByteOrder byteOrderFromIndex(int idx);

}  // namespace etest::protocal::utils
