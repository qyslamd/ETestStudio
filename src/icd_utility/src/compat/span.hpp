#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace icd {

template <typename T>
class span {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr span() noexcept : data_(nullptr), size_(0) {}

    constexpr span(T* data, size_type size) noexcept : data_(data), size_(size) {}

    template <typename Container,
              typename = decltype(std::declval<Container&>().data())>
    constexpr span(Container& c) noexcept : data_(c.data()), size_(c.size()) {}

    template <typename Container,
              typename = decltype(std::declval<const Container&>().data())>
    constexpr span(const Container& c) noexcept : data_(c.data()), size_(c.size()) {}

    constexpr T* data() const noexcept { return data_; }
    constexpr size_type size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr iterator begin() const noexcept { return data_; }
    constexpr iterator end() const noexcept { return data_ + size_; }

    constexpr const_iterator cbegin() const noexcept { return data_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }

    constexpr reference operator[](size_type idx) const { return data_[idx]; }

    constexpr span first(size_type count) const { return span(data_, count); }
    constexpr span last(size_type count) const { return span(data_ + size_ - count, count); }

private:
    T* data_;
    size_type size_;
};

} // namespace icd
