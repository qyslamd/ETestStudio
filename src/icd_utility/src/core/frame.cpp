#include <icd/frame.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

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

void Frame::setId(int id) { id_ = id; }
void Frame::setName(std::string_view name) { name_ = std::string(name); }
void Frame::setDescription(std::string_view description) { description_ = std::string(description); }
void Frame::setType(FrameType type) { type_ = type; }
void Frame::setOrder(ByteOrder order) { order_ = order; }

bool Frame::remove_root(std::size_t index) {
    if (index >= roots_.size()) {
        return false;
    }
    // Find all nodes in this subtree and remove from flat index
    auto& root = roots_[index];
    std::vector<Node*> to_remove;
    to_remove.push_back(root.get());
    for (std::size_t i = 0; i < to_remove.size(); ++i) {
        for (const auto& child : to_remove[i]->children()) {
            to_remove.push_back(const_cast<Node*>(child.get()));
        }
    }
    for (auto* node : to_remove) {
        auto it = std::find(nodes_.begin(), nodes_.end(), node);
        if (it != nodes_.end()) {
            nodes_.erase(it);
        }
    }
    roots_.erase(roots_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void Frame::index_subtree(Node& node) noexcept {
    node.set_frame(this);
    nodes_.push_back(&node);
    for (const auto& child : node.children()) {
        index_subtree(*const_cast<Node*>(child.get()));
    }
}

} // namespace icd
