#pragma once

#include <icd/export.hpp>
#include <icd/error.hpp>
#include <icd/types.hpp>

#include <tl/expected.hpp>

#include <memory>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <compat/span.hpp>

namespace icd {

class Frame;

class ICD_UTILITY_API Node {
public:
    Node(std::string name,
         std::string description,
         int offset,
         int bit_offset,
         int bit_width,
         ValueType value_type,
         Tag tag,
         NodeAttrs attrs);

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    std::string_view name() const noexcept;
    std::string_view description() const noexcept;

    int offset() const noexcept;
    int bit_offset() const noexcept;
    int bit_width() const noexcept;

    ValueType value_type() const noexcept;
    Tag tag() const noexcept;

    const NodeAttrs& attrs() const noexcept;

    const Node* parent() const noexcept;
    icd::span<const std::unique_ptr<Node>> children() const noexcept;

    const Node* find(std::string_view name) const noexcept;

    tl::expected<const NodeValue*, Error> get_value();
    tl::expected<const NodeValue*, Error> resolve_value();
    tl::expected<void, Error> set_value(NodeValue value);
    bool modified() const noexcept;

    tl::expected<NodeValue, Error>
    decode(icd::span<const std::byte> frame_bytes, ByteOrder frame_order) const;

    void add_child(std::unique_ptr<Node> child);

private:
    void set_frame(Frame* frame) noexcept;
    void mark_children_modified() noexcept;

    std::string name_;
    std::string description_;
    int offset_;
    int bit_offset_;
    int bit_width_;
    ValueType value_type_;
    Tag tag_;
    NodeAttrs attrs_;
    Frame* frame_;
    Node* parent_;
    std::optional<NodeValue> value_;
    bool modified_;
    std::vector<std::unique_ptr<Node>> children_;

    friend class Frame;
};

} // namespace icd
