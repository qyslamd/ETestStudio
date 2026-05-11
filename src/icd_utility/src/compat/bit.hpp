#pragma once

#include <cstring>
#include <type_traits>

namespace icd {

template <typename To, typename From>
std::enable_if_t<
    sizeof(To) == sizeof(From) && std::is_trivially_copyable_v<To> && std::is_trivially_copyable_v<From>,
    To>
bit_cast(const From& src) noexcept {
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

} // namespace icd
