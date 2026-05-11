#include <icd/repository.hpp>

#include <string>
#include <utility>

namespace icd {

icd::span<const std::unique_ptr<Frame>> Repository::frames() const noexcept { return frames_; }

const Frame* Repository::find(int id) const noexcept {
    if (const auto it = frames_by_id_.find(id); it != frames_by_id_.end()) {
        return it->second;
    }
    return nullptr;
}

const Frame* Repository::find(std::string_view name) const noexcept {
    if (const auto it = frames_by_name_.find(std::string(name)); it != frames_by_name_.end()) {
        return it->second;
    }
    return nullptr;
}

const Node* Repository::find(std::string_view frame_name, std::string_view node_name) const noexcept {
    if (const auto* frame = find(frame_name)) {
        return frame->find(node_name);
    }
    return nullptr;
}

void Repository::add_frame(std::unique_ptr<Frame> frame) {
    auto* frame_ptr = frame.get();
    frames_by_id_.emplace(frame_ptr->id(), frame_ptr);
    frames_by_name_.emplace(std::string(frame_ptr->name()), frame_ptr);
    frames_.push_back(std::move(frame));
}

} // namespace icd
