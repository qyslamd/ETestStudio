#pragma once

#include <pugixml.hpp>
#include <tl/expected.hpp>

#include <icd/error.hpp>

#include <filesystem>

namespace icd::format {

// Load XML document with CJK path support.
// pugixml's load_file uses fopen internally, which on Windows cannot handle
// CJK paths. This reads via std::ifstream (wide path support on MSVC).
tl::expected<pugi::xml_document, Error> load_xml_document(const std::filesystem::path& path);

// Write XML document to file with CJK path support (std::ofstream + doc.save).
tl::expected<void, Error> save_xml_document(const pugi::xml_document& doc, const std::filesystem::path& path);

} // namespace icd::format
