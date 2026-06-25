#include "xml_util.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace icd::format {

tl::expected<pugi::xml_document, Error> load_xml_document(const std::filesystem::path& path) {
    // pugixml's load_file uses fopen internally, which on Windows cannot handle
    // CJK characters in paths (fopen uses ANSI encoding). Read the file ourselves
    // using std::ifstream (which supports wide paths on MSVC), then parse via load_buffer.
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return tl::make_unexpected(Error{ErrorCode::parse_error, "failed to open file", path, {}});
    }

    auto size = stream.tellg();
    if (size <= 0) {
        return tl::make_unexpected(Error{ErrorCode::parse_error, "file is empty", path, {}});
    }

    stream.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    stream.read(buffer.data(), static_cast<std::streamsize>(size));

    if (!stream) {
        return tl::make_unexpected(Error{ErrorCode::parse_error, "failed to read file", path, {}});
    }

    pugi::xml_document doc;
    const auto result = doc.load_buffer(buffer.data(), buffer.size(), pugi::parse_default, pugi::encoding_auto);
    if (!result) {
        return tl::make_unexpected(Error{ErrorCode::parse_error, result.description(), path, {}});
    }

    return doc;
}

tl::expected<void, Error> save_xml_document(const pugi::xml_document& doc, const std::filesystem::path& path) {
    std::ofstream stream(path);
    if (!stream) {
        return tl::make_unexpected(Error{ErrorCode::io_error, "failed to open file for writing", path, {}});
    }

    // Use pugixml's save() to write to the ofstream.
    // The ofstream handles wide/UTF-8 path encoding on MSVC.
    // NOTE: save() with std::ostream returns void, so check stream state after.
    doc.save(stream, "  ", pugi::format_default, pugi::encoding_utf8);
    if (!stream) {
        return tl::make_unexpected(Error{ErrorCode::io_error, "failed to write XML document", path, {}});
    }

    return {};
}

} // namespace icd::format
