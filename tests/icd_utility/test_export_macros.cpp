#include <gtest/gtest.h>
#include <icd/export.hpp>
#include <icd/loader.hpp>
#include <type_traits>

TEST(ExportMacrosTest, CompileTimeChecks) {
    static_assert(!std::is_copy_constructible_v<icd::Node>);
    static_assert(!std::is_copy_assignable_v<icd::Node>);
    static_assert(!std::is_move_constructible_v<icd::Node>);
    static_assert(!std::is_move_assignable_v<icd::Node>);
    static_assert(!std::is_copy_constructible_v<icd::Frame>);
    static_assert(!std::is_copy_assignable_v<icd::Frame>);
    static_assert(std::is_move_constructible_v<icd::Frame>);
    static_assert(std::is_move_assignable_v<icd::Frame>);
    static_assert(!std::is_copy_constructible_v<icd::Repository>);
    static_assert(!std::is_copy_assignable_v<icd::Repository>);
    static_assert(std::is_move_constructible_v<icd::Repository>);
    static_assert(std::is_move_assignable_v<icd::Repository>);
    [[maybe_unused]] auto init_fn = &icd::Loader::init;
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
