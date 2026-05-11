#pragma once

#include <icd/export.hpp>
#include <icd/frame.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <compat/span.hpp>

namespace icd {

class ICD_UTILITY_API Repository {
public:
    Repository() = default;
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
    Repository(Repository&&) noexcept = default;
    Repository& operator=(Repository&&) noexcept = default;

    icd::span<const std::unique_ptr<Frame>> frames() const noexcept;

    const Frame* find(int id) const noexcept;
    const Frame* find(std::string_view name) const noexcept;
    const Node* find(std::string_view frame_name, std::string_view node_name) const noexcept;

    void add_frame(std::unique_ptr<Frame> frame);

private:
    std::vector<std::unique_ptr<Frame>> frames_;
    std::unordered_map<int, Frame*> frames_by_id_;
    std::unordered_map<std::string, Frame*> frames_by_name_;
};

} // namespace icd
