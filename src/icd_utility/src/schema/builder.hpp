#pragma once

#include <tl/expected.hpp>

#include <icd/error.hpp>
#include <icd/repository.hpp>

#include "schema.hpp"

namespace icd::schema {

tl::expected<Repository, Error> build_repository(const SchemaConfig& config);

} // namespace icd::schema
