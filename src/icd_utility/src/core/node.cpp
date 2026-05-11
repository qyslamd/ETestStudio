#include <icd/node.hpp>

#include <icd/frame.hpp>

#include <algorithm>
#include <array>
#include <compat/bit.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace icd {
namespace {

tl::unexpected<Error> make_decode_error(std::string message, std::string_view path_hint) {
    return tl::make_unexpected(Error{ErrorCode::invalid_argument, std::move(message), {}, std::string(path_hint)});
}

tl::expected<std::uint64_t, Error>
extract_bits(icd::span<const std::byte> frame_bytes, int offset, int bit_offset, int bit_width) {
    if (offset < 0 || bit_offset < 0 || bit_width <= 0) {
        return make_decode_error("invalid node bit range", "Node::decode.range");
    }
    if (bit_width > 64) {
        return make_decode_error("bit width exceeds 64 bits", "Node::decode.range");
    }

    const auto start_bit = static_cast<std::size_t>(offset) * 8u + static_cast<std::size_t>(bit_offset);
    const auto end_bit = start_bit + static_cast<std::size_t>(bit_width);
    const auto total_bits = frame_bytes.size() * 8u;
    if (end_bit > total_bits) {
        return make_decode_error("frame buffer too small for node decode", "Node::decode.range");
    }

    std::uint64_t value = 0;
    for (int i = 0; i < bit_width; ++i) {
        const auto source_bit = start_bit + static_cast<std::size_t>(i);
        const auto byte_index = source_bit / 8u;
        const auto bit_index = source_bit % 8u;
        const auto bit = (std::to_integer<std::uint8_t>(frame_bytes[byte_index]) >> bit_index) & 0x1u;
        value |= static_cast<std::uint64_t>(bit) << i;
    }

    return value;
}

tl::expected<std::uint64_t, Error>
read_integer_bits(icd::span<const std::byte> frame_bytes, int offset, int bit_offset, int bit_width, ByteOrder frame_order) {
    if (bit_width <= 0 || bit_width > 64) {
        return make_decode_error("invalid integer bit width", "Node::decode.integer");
    }

    if (bit_offset == 0 && bit_width % 8 == 0) {
        const auto byte_count = static_cast<std::size_t>(bit_width / 8);
        if (offset < 0 || static_cast<std::size_t>(offset) + byte_count > frame_bytes.size()) {
            return make_decode_error("frame buffer too small for integer decode", "Node::decode.integer");
        }

        std::uint64_t value = 0;
        if (frame_order == ByteOrder::little_endian) {
            for (std::size_t i = 0; i < byte_count; ++i) {
                value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(frame_bytes[static_cast<std::size_t>(offset) + i]))
                         << (i * 8u);
            }
        } else {
            for (std::size_t i = 0; i < byte_count; ++i) {
                value <<= 8u;
                value |= std::to_integer<std::uint8_t>(frame_bytes[static_cast<std::size_t>(offset) + i]);
            }
        }
        return value;
    }

    return extract_bits(frame_bytes, offset, bit_offset, bit_width);
}

std::int64_t sign_extend(std::uint64_t value, int bit_width) {
    if (bit_width >= 64) {
        return static_cast<std::int64_t>(value);
    }

    const auto sign_bit = static_cast<std::uint64_t>(1) << (bit_width - 1);
    if ((value & sign_bit) == 0) {
        return static_cast<std::int64_t>(value);
    }

    const auto extend_mask = ~((static_cast<std::uint64_t>(1) << bit_width) - 1u);
    return static_cast<std::int64_t>(value | extend_mask);
}

tl::expected<std::vector<std::byte>, Error>
read_bytes(icd::span<const std::byte> frame_bytes, int offset, int bit_offset, int bit_width) {
    if (bit_offset != 0 || bit_width <= 0 || bit_width % 8 != 0) {
        return make_decode_error("bytes/string decode requires byte alignment", "Node::decode.bytes");
    }

    const auto byte_count = static_cast<std::size_t>(bit_width / 8);
    if (offset < 0 || static_cast<std::size_t>(offset) + byte_count > frame_bytes.size()) {
        return make_decode_error("frame buffer too small for bytes decode", "Node::decode.bytes");
    }

    const auto begin = frame_bytes.begin() + offset;
    return std::vector<std::byte>(begin, begin + byte_count);
}

tl::expected<void, Error>
write_bytes(icd::span<std::byte> frame_bytes, int offset, int bit_offset, int bit_width, icd::span<const std::byte> value) {
    if (bit_offset != 0 || bit_width <= 0 || bit_width % 8 != 0) {
        return make_decode_error("bytes/string write requires byte alignment", "Node::set_value.bytes");
    }

    const auto byte_count = static_cast<std::size_t>(bit_width / 8);
    if (value.size() != byte_count) {
        return make_decode_error("bytes/string write requires exact fixed length", "Node::set_value.bytes");
    }
    if (offset < 0 || static_cast<std::size_t>(offset) + byte_count > frame_bytes.size()) {
        return make_decode_error("frame buffer too small for bytes write", "Node::set_value.bytes");
    }

    std::copy(value.begin(), value.end(), frame_bytes.begin() + offset);
    return {};
}

tl::expected<void, Error>
write_integer_bits(icd::span<std::byte> frame_bytes, int offset, int bit_offset, int bit_width, ByteOrder frame_order, std::uint64_t value) {
    if (bit_width <= 0 || bit_width > 64) {
        return make_decode_error("invalid integer bit width", "Node::set_value.integer");
    }

    if (bit_offset == 0 && bit_width % 8 == 0) {
        const auto byte_count = static_cast<std::size_t>(bit_width / 8);
        if (offset < 0 || static_cast<std::size_t>(offset) + byte_count > frame_bytes.size()) {
            return make_decode_error("frame buffer too small for integer write", "Node::set_value.integer");
        }

        if (frame_order == ByteOrder::little_endian) {
            for (std::size_t i = 0; i < byte_count; ++i) {
                frame_bytes[static_cast<std::size_t>(offset) + i] = std::byte((value >> (i * 8u)) & 0xFFu);
            }
        } else {
            for (std::size_t i = 0; i < byte_count; ++i) {
                const auto shift = (byte_count - 1u - i) * 8u;
                frame_bytes[static_cast<std::size_t>(offset) + i] = std::byte((value >> shift) & 0xFFu);
            }
        }

        return {};
    }

    if (offset < 0 || bit_offset < 0) {
        return make_decode_error("invalid node bit range", "Node::set_value.range");
    }

    const auto start_bit = static_cast<std::size_t>(offset) * 8u + static_cast<std::size_t>(bit_offset);
    const auto end_bit = start_bit + static_cast<std::size_t>(bit_width);
    if (end_bit > frame_bytes.size() * 8u) {
        return make_decode_error("frame buffer too small for bit write", "Node::set_value.range");
    }

    for (int i = 0; i < bit_width; ++i) {
        const auto target_bit = start_bit + static_cast<std::size_t>(i);
        const auto byte_index = target_bit / 8u;
        const auto bit_index = target_bit % 8u;
        auto current = std::to_integer<std::uint8_t>(frame_bytes[byte_index]);
        const auto bit = (value >> i) & 0x1u;
        if (bit != 0) {
            current = static_cast<std::uint8_t>(current | (1u << bit_index));
        } else {
            current = static_cast<std::uint8_t>(current & ~(1u << bit_index));
        }
        frame_bytes[byte_index] = std::byte(current);
    }

    return {};
}

tl::expected<void, Error> ensure_signed_fits(std::int64_t value, int bit_width, std::string_view path_hint) {
    if (bit_width <= 0 || bit_width > 64) {
        return make_decode_error("invalid signed bit width", path_hint);
    }
    if (bit_width == 64) {
        return {};
    }

    const auto min_value = -(static_cast<std::int64_t>(1) << (bit_width - 1));
    const auto max_value = (static_cast<std::int64_t>(1) << (bit_width - 1)) - 1;
    if (value < min_value || value > max_value) {
        return make_decode_error("signed value does not fit target bit width", path_hint);
    }
    return {};
}

tl::expected<void, Error> ensure_unsigned_fits(std::uint64_t value, int bit_width, std::string_view path_hint) {
    if (bit_width <= 0 || bit_width > 64) {
        return make_decode_error("invalid unsigned bit width", path_hint);
    }
    if (bit_width == 64) {
        return {};
    }

    const auto max_value = (static_cast<std::uint64_t>(1) << bit_width) - 1u;
    if (value > max_value) {
        return make_decode_error("unsigned value does not fit target bit width", path_hint);
    }
    return {};
}

template <typename T>
tl::expected<double, Error>
read_floating(icd::span<const std::byte> frame_bytes, int offset, int bit_offset, int bit_width, ByteOrder frame_order) {
    constexpr auto expected_bits = static_cast<int>(sizeof(T) * 8u);
    if (bit_offset != 0 || bit_width != expected_bits) {
        return make_decode_error("floating-point decode requires standard byte-aligned width", "Node::decode.float");
    }

    const auto raw = read_integer_bits(frame_bytes, offset, bit_offset, bit_width, frame_order);
    if (!raw) {
        return tl::make_unexpected(raw.error());
    }

    if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
        const auto bits = static_cast<std::uint32_t>(*raw);
        return static_cast<double>(icd::bit_cast<float>(bits));
    } else {
        const auto bits = static_cast<std::uint64_t>(*raw);
        return icd::bit_cast<double>(bits);
    }
}

} // namespace

Node::Node(std::string name,
           std::string description,
           int offset,
           int bit_offset,
           int bit_width,
           ValueType value_type,
           Tag tag,
           NodeAttrs attrs)
    : name_(std::move(name)),
      description_(std::move(description)),
      offset_(offset),
      bit_offset_(bit_offset),
      bit_width_(bit_width),
      value_type_(value_type),
      tag_(tag),
      attrs_(std::move(attrs)),
      frame_(nullptr),
      parent_(nullptr),
      modified_(false) {}

std::string_view Node::name() const noexcept { return name_; }
std::string_view Node::description() const noexcept { return description_; }
int Node::offset() const noexcept { return offset_; }
int Node::bit_offset() const noexcept { return bit_offset_; }
int Node::bit_width() const noexcept { return bit_width_; }
ValueType Node::value_type() const noexcept { return value_type_; }
Tag Node::tag() const noexcept { return tag_; }
const NodeAttrs& Node::attrs() const noexcept { return attrs_; }
const Node* Node::parent() const noexcept { return parent_; }
icd::span<const std::unique_ptr<Node>> Node::children() const noexcept { return children_; }

const Node* Node::find(std::string_view name) const noexcept {
    if (name_ == name) {
        return this;
    }
    for (const auto& child : children_) {
        if (const auto* found = child->find(name)) {
            return found;
        }
    }
    return nullptr;
}

tl::expected<const NodeValue*, Error> Node::get_value() {
    if (modified_) {
        auto resolved = resolve_value();
        if (!resolved) {
            return tl::make_unexpected(resolved.error());
        }
    }

    if (!value_.has_value()) {
        return make_decode_error("node value is not available", "Node::get_value");
    }

    return &*value_;
}

tl::expected<const NodeValue*, Error> Node::resolve_value() {
    if (frame_ == nullptr || !frame_->has_decode_buffer_) {
        return make_decode_error("frame decode buffer is not available", "Node::resolve_value");
    }

    auto decoded = decode(frame_->decode_buffer_, frame_->order());
    if (!decoded) {
        return tl::make_unexpected(decoded.error());
    }

    value_ = std::move(*decoded);
    modified_ = false;
    return &*value_;
}

tl::expected<void, Error> Node::set_value(NodeValue value) {
    if (frame_ == nullptr || !frame_->has_decode_buffer_) {
        return make_decode_error("frame decode buffer is not available", "Node::set_value");
    }

    auto buffer = icd::span<std::byte>(frame_->decode_buffer_);

    switch (value_type_) {
    case ValueType::boolean: {
        if (!std::holds_alternative<bool>(value) || bit_width_ != 1) {
            return make_decode_error("boolean node requires bool with width 1", "Node::set_value.boolean");
        }
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), std::get<bool>(value) ? 1u : 0u);
        if (!result) return result;
        break;
    }
    case ValueType::word: {
        if (!std::holds_alternative<std::uint16_t>(value) || bit_width_ != 16) {
            return make_decode_error("word node requires uint16_t with width 16", "Node::set_value.word");
        }
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), std::get<std::uint16_t>(value));
        if (!result) return result;
        break;
    }
    case ValueType::smallint: {
        if (!std::holds_alternative<std::int16_t>(value) || bit_width_ != 16) {
            return make_decode_error("smallint node requires int16_t with width 16", "Node::set_value.smallint");
        }
        auto signed_value = static_cast<std::int64_t>(std::get<std::int16_t>(value));
        auto fits = ensure_signed_fits(signed_value, bit_width_, "Node::set_value.smallint");
        if (!fits) return fits;
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), static_cast<std::uint64_t>(signed_value));
        if (!result) return result;
        break;
    }
    case ValueType::longword: {
        if (!std::holds_alternative<std::uint32_t>(value) || bit_width_ != 32) {
            return make_decode_error("longword node requires uint32_t with width 32", "Node::set_value.longword");
        }
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), std::get<std::uint32_t>(value));
        if (!result) return result;
        break;
    }
    case ValueType::integer: {
        if (!std::holds_alternative<std::int32_t>(value) || bit_width_ != 32) {
            return make_decode_error("integer node requires int32_t with width 32", "Node::set_value.integer");
        }
        auto signed_value = static_cast<std::int64_t>(std::get<std::int32_t>(value));
        auto fits = ensure_signed_fits(signed_value, bit_width_, "Node::set_value.integer");
        if (!fits) return fits;
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), static_cast<std::uint64_t>(signed_value));
        if (!result) return result;
        break;
    }
    case ValueType::byte_: {
        if (!std::holds_alternative<std::uint64_t>(value) || bit_width_ != 8) {
            return make_decode_error("byte_ node requires uint64_t with width 8", "Node::set_value.byte");
        }
        auto raw = std::get<std::uint64_t>(value);
        auto fits = ensure_unsigned_fits(raw, bit_width_, "Node::set_value.byte");
        if (!fits) return fits;
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), raw);
        if (!result) return result;
        break;
    }
    case ValueType::shortint: {
        if (!std::holds_alternative<std::int64_t>(value) || bit_width_ != 8) {
            return make_decode_error("shortint node requires int64_t with width 8", "Node::set_value.shortint");
        }
        auto signed_value = std::get<std::int64_t>(value);
        auto fits = ensure_signed_fits(signed_value, bit_width_, "Node::set_value.shortint");
        if (!fits) return fits;
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), static_cast<std::uint64_t>(signed_value));
        if (!result) return result;
        break;
    }
    case ValueType::ulong_: {
        if (!std::holds_alternative<std::uint64_t>(value) || bit_width_ != 64) {
            return make_decode_error("ulong_ node requires uint64_t with width 64", "Node::set_value.ulong");
        }
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), std::get<std::uint64_t>(value));
        if (!result) return result;
        break;
    }
    case ValueType::single: {
        if (!std::holds_alternative<double>(value) || bit_offset_ != 0 || bit_width_ != 32) {
            return make_decode_error("single node requires double with byte-aligned width 32", "Node::set_value.single");
        }
        const auto raw = icd::bit_cast<std::uint32_t>(static_cast<float>(std::get<double>(value)));
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), raw);
        if (!result) return result;
        break;
    }
    case ValueType::double_: {
        if (!std::holds_alternative<double>(value) || bit_offset_ != 0 || bit_width_ != 64) {
            return make_decode_error("double_ node requires double with byte-aligned width 64", "Node::set_value.double");
        }
        const auto raw = icd::bit_cast<std::uint64_t>(std::get<double>(value));
        auto result = write_integer_bits(buffer, offset_, bit_offset_, bit_width_, frame_->order(), raw);
        if (!result) return result;
        break;
    }
    case ValueType::bytes: {
        if (!std::holds_alternative<std::vector<std::byte>>(value)) {
            return make_decode_error("bytes node requires byte vector value", "Node::set_value.bytes");
        }
        const auto& bytes = std::get<std::vector<std::byte>>(value);
        auto result = write_bytes(buffer, offset_, bit_offset_, bit_width_, bytes);
        if (!result) return result;
        break;
    }
    case ValueType::string_: {
        if (!std::holds_alternative<std::string>(value)) {
            return make_decode_error("string_ node requires string value", "Node::set_value.string");
        }
        const auto& string_value = std::get<std::string>(value);
        std::vector<std::byte> bytes;
        bytes.reserve(string_value.size());
        for (unsigned char ch : string_value) {
            bytes.push_back(std::byte{ch});
        }
        auto result = write_bytes(buffer, offset_, bit_offset_, bit_width_, bytes);
        if (!result) return result;
        break;
    }
    case ValueType::unknown:
        return make_decode_error("set_value does not support this node type yet", "Node::set_value.type");
    }

    value_ = std::move(value);
    modified_ = false;
    mark_children_modified();
    return {};
}

bool Node::modified() const noexcept { return modified_; }

void Node::add_child(std::unique_ptr<Node> child) {
    child->parent_ = this;
    children_.push_back(std::move(child));
}

void Node::set_frame(Frame* frame) noexcept {
    frame_ = frame;
    for (const auto& child : children_) {
        child->set_frame(frame);
    }
}

void Node::mark_children_modified() noexcept {
    for (const auto& child : children_) {
        child->modified_ = true;
        child->mark_children_modified();
    }
}

tl::expected<NodeValue, Error>
Node::decode(icd::span<const std::byte> frame_bytes, ByteOrder frame_order) const {
    switch (value_type_) {
    case ValueType::boolean: {
        if (bit_width_ != 1) {
            return make_decode_error("boolean nodes must have width 1", "Node::decode.boolean");
        }
        auto raw = extract_bits(frame_bytes, offset_, bit_offset_, bit_width_);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{static_cast<bool>(*raw != 0)};
    }
    case ValueType::byte_: {
        if (bit_width_ != 8) {
            return make_decode_error("byte_ nodes must have width 8", "Node::decode.byte");
        }
        auto raw = read_integer_bits(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{static_cast<std::uint64_t>(*raw)};
    }
    case ValueType::word: {
        if (bit_width_ != 16) {
            return make_decode_error("word nodes must have width 16", "Node::decode.word");
        }
        auto raw = read_integer_bits(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{static_cast<std::uint16_t>(*raw)};
    }
    case ValueType::smallint: {
        if (bit_width_ != 16) {
            return make_decode_error("smallint nodes must have width 16", "Node::decode.smallint");
        }
        auto raw = read_integer_bits(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{static_cast<std::int16_t>(sign_extend(*raw, bit_width_))};
    }
    case ValueType::longword: {
        if (bit_width_ != 32) {
            return make_decode_error("longword nodes must have width 32", "Node::decode.longword");
        }
        auto raw = read_integer_bits(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{static_cast<std::uint32_t>(*raw)};
    }
    case ValueType::integer: {
        if (bit_width_ != 32) {
            return make_decode_error("integer nodes must have width 32", "Node::decode.integer");
        }
        auto raw = read_integer_bits(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{static_cast<std::int32_t>(sign_extend(*raw, bit_width_))};
    }
    case ValueType::ulong_: {
        if (bit_width_ != 64) {
            return make_decode_error("ulong_ nodes must have width 64", "Node::decode.ulong");
        }
        auto raw = read_integer_bits(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{static_cast<std::uint64_t>(*raw)};
    }
    case ValueType::shortint: {
        if (bit_width_ != 8) {
            return make_decode_error("shortint nodes must have width 8", "Node::decode.shortint");
        }
        auto raw = read_integer_bits(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!raw) {
            return tl::make_unexpected(raw.error());
        }
        return NodeValue{sign_extend(*raw, bit_width_)};
    }
    case ValueType::single: {
        auto value = read_floating<float>(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!value) {
            return tl::make_unexpected(value.error());
        }
        return NodeValue{*value};
    }
    case ValueType::double_: {
        auto value = read_floating<double>(frame_bytes, offset_, bit_offset_, bit_width_, frame_order);
        if (!value) {
            return tl::make_unexpected(value.error());
        }
        return NodeValue{*value};
    }
    case ValueType::bytes: {
        auto bytes = read_bytes(frame_bytes, offset_, bit_offset_, bit_width_);
        if (!bytes) {
            return tl::make_unexpected(bytes.error());
        }
        return NodeValue{std::move(*bytes)};
    }
    case ValueType::string_: {
        auto bytes = read_bytes(frame_bytes, offset_, bit_offset_, bit_width_);
        if (!bytes) {
            return tl::make_unexpected(bytes.error());
        }

        std::string value;
        value.reserve(bytes->size());
        for (const auto byte : *bytes) {
            value.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
        }
        return NodeValue{std::move(value)};
    }
    case ValueType::unknown:
        return make_decode_error("unknown value type cannot be decoded", "Node::decode.unknown");
    }

    return make_decode_error("unsupported value type", "Node::decode.type");
}

} // namespace icd
