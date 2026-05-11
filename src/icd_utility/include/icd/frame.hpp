#pragma once

#include <icd/export.hpp>
#include <icd/error.hpp>
#include <icd/node.hpp>

#include <tl/expected.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <compat/span.hpp>

namespace icd {

class ICD_UTILITY_API Frame {
public:
    Frame(int id,
          std::string name,
          std::string description,
          FrameType type,
          ByteOrder order);

    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    Frame(Frame&&) noexcept = default;
    Frame& operator=(Frame&&) noexcept = default;

    int id() const noexcept;
    std::string_view name() const noexcept;
    std::string_view description() const noexcept;

    FrameType type() const noexcept;
    ByteOrder order() const noexcept;

    icd::span<const std::unique_ptr<Node>> roots() const noexcept;
    icd::span<Node* const> nodes() const noexcept;

    const Node* find(std::string_view name) const noexcept;
    tl::expected<void, Error> decode(icd::span<const std::byte> frame_bytes, DecodeMode mode = DecodeMode::eager);

    void add_root(std::unique_ptr<Node> node);

private:
    void index_subtree(Node& node) noexcept;

    int id_;
    std::string name_;
    std::string description_;
    FrameType type_;
    ByteOrder order_;
    std::vector<std::unique_ptr<Node>> roots_;
    std::vector<Node*> nodes_;
    std::vector<std::byte> decode_buffer_;
    bool has_decode_buffer_ {false};

    friend class Node;
};

} // namespace icd
