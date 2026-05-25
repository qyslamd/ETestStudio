#include "builder.hpp"

#include <icd/frame.hpp>
#include <icd/node.hpp>

#include <string>
#include <unordered_set>
#include <utility>

namespace icd::schema {
namespace {

tl::expected<std::unique_ptr<Node>, Error> build_node(const SchemaNodeDef& schema_node) {
    if (schema_node.name.empty()) {
        return tl::make_unexpected(Error{ErrorCode::invalid_node,
                                     "node name cannot be empty",
                                     {},
                                     "SchemaNodeDef.name"});
    }

    auto node = std::make_unique<Node>(
        schema_node.name,
        schema_node.description,
        schema_node.offset,
        schema_node.bit_offset,
        schema_node.bit_width,
        schema_node.value_type,
        schema_node.tag,
        schema_node.attrs);

    for (const auto& child_schema : schema_node.children) {
        auto child = build_node(child_schema);
        if (!child) {
            return tl::make_unexpected(child.error());
        }
        node->add_child(std::move(*child));
    }

    return node;
}

} // namespace

tl::expected<Repository, Error> build_repository(const SchemaConfig& config) {
    Repository repository;
    std::unordered_set<int> seen_ids;
    std::unordered_set<std::string> seen_names;

    for (const auto& frame_schema : config.frames) {
        if (!seen_ids.insert(frame_schema.id).second) {
            return tl::make_unexpected(Error{ErrorCode::duplicate_frame_id,
                                         "duplicate frame id",
                                         {},
                                         frame_schema.name});
        }
        if (!seen_names.insert(frame_schema.name).second) {
            return tl::make_unexpected(Error{ErrorCode::duplicate_frame_name,
                                         "duplicate frame name",
                                         {},
                                         frame_schema.name});
        }

        auto frame = std::make_unique<Frame>(
            frame_schema.id,
            frame_schema.name,
            frame_schema.description,
            frame_schema.type,
            frame_schema.order);

        for (const auto& root_schema : frame_schema.roots) {
            auto root = build_node(root_schema);
            if (!root) {
                return tl::make_unexpected(root.error());
            }
            frame->add_root(std::move(*root));
        }

        repository.add_frame(std::move(frame));
    }

    return repository;
}

} // namespace icd::schema
