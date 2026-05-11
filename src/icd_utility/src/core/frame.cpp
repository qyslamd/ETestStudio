#include <icd/frame.hpp>

#include <cstddef>
#include <utility>

namespace icd {

Frame::Frame(int id,
             std::string name,
             std::string description,
             FrameType type,
             ByteOrder order)
    : id_(id),
      name_(std::move(name)),
      description_(std::move(description)),
      type_(type),
      order_(order) {}

int Frame::id() const noexcept { return id_; }
std::string_view Frame::name() const noexcept { return name_; }
std::string_view Frame::description() const noexcept { return description_; }
FrameType Frame::type() const noexcept { return type_; }
ByteOrder Frame::order() const noexcept { return order_; }
icd::span<const std::unique_ptr<Node>> Frame::roots() const noexcept { return roots_; }
icd::span<Node* const> Frame::nodes() const noexcept { return nodes_; }

const Node* Frame::find(std::string_view name) const noexcept {
    for (const auto* node : nodes_) {
        if (node->name() == name) {
            return node;
        }
    }
    return nullptr;
}

tl::expected<void, Error> Frame::decode(    icd::span<const std::byte> frame_bytes, DecodeMode mode) {
    decode_buffer_.assign(frame_bytes.begin(), frame_bytes.end());
    has_decode_buffer_ = true;

    if (mode == DecodeMode::lazy) {
        for (auto* node : nodes_) {
            node->modified_ = true;
        }
        return {};
    }

    for (auto* node : nodes_) {
        auto resolved = node->resolve_value();
        if (!resolved) {
            return tl::make_unexpected(resolved.error());
        }
    }

    return {};
}

void Frame::add_root(std::unique_ptr<Node> node) {
    index_subtree(*node);
    roots_.push_back(std::move(node));
}

void Frame::index_subtree(Node& node) noexcept {
    node.set_frame(this);
    nodes_.push_back(&node);
    for (const auto& child : node.children()) {
        index_subtree(*const_cast<Node*>(child.get()));
    }
}

} // namespace icd
