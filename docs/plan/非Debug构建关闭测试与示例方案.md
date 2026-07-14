# 非 Debug 构建关闭测试与示例方案

## 目标

Release/RelWithDebInfo 模式下，关闭单元测试和示例程序的编译，减少构建时间与产物体积。

## 现状

| 组件 | CMake 选项 | 默认值 |
|---|---|---|
| 单元测试（gtest） | `BUILD_TESTING` | ON |
| 第三方库测试 | `BUILD_TESTS_THIRD_PARTY` | ON |
| 示例程序（eye-tracking 等） | `BUILD_ETEST_EXAMPLES` | ON |

所有 Preset 均未显式设置这些选项，Debug/Release/RelWithDebInfo 全部编译。

## 改动方案

只改 `CMakePresets.json`，向非 Debug 的 Preset 中追加 cacheVariables：

| Preset | 追加的变量 |
|---|---|
| `ninja-release` | `BUILD_TESTING=OFF` |
| | `BUILD_TESTS_THIRD_PARTY=OFF` |
| | `BUILD_ETEST_EXAMPLES=OFF` |
| `ninja-relwithdebinfo` | 同上 |
| `windows-release` | 同上（一致性） |

通过 Preset 继承链，`ninja-release-x64/x86` 和 `ninja-relwithdebinfo-x64/x86` 自动获得这些值，无需逐个修改。

### 不动的内容

- `BUILD_MOCK_PLUGINS` — mock 设备插件与 Debug/Release 无关
- `BUILD_ETEST_TOOLS` — 附属编辑器需随发布版本编译
- Debug Preset（`ninja-debug`、`windows-debug`）— 保持现状

## 注意事项

### CTest / testPresets 失效

`BUILD_TESTING=OFF` 时，`include(CTef)` 和 `enable_testing()` 均不执行，**整个 CTest 设施完全不可用**。`CMakePresets.json` 中关联非 Debug 构建的 6 个 testPreset：

- `ninja-release-test`
- `ninja-release-x64-test` / `ninja-release-x86-test`
- `ninja-relwithdebinfo-test`
- `ninja-relwithdebinfo-x64-test` / `ninja-relwithdebinfo-x86-test`

改动后若执行 `ctest --preset ninja-release-test`，会报 0 tests 退出码 0，可能误导使用者误以为测试通过。

**处理**：这些 testPreset 保留不动（改动不会报错），但在 Release/RelWithDebInfo 下不支持 ctest。CI 或脚本中若有对应步骤，需改为仅在 Debug 下执行。

### 缓存覆盖

已有 build 目录的本地机器上，`CMakeCache.txt` 中缓存的旧值（`BUILD_TESTING=ON`）优先级高于 Preset 新值。解决方式（任选其一）：

1. **Preset 后追加 -D**（推荐，最快）：
   ```
   cmake --preset ninja-release -DBUILD_TESTING=OFF -DBUILD_ETEST_EXAMPLES=OFF -DBUILD_TESTS_THIRD_PARTY=OFF
   ```
   `-D` 会覆盖缓存中的旧值，不会触发全量重新编译。

2. **删除 CMakeCache.txt 后重新 configure**：
   ```
   del build\ninja-release-x86\CMakeCache.txt
   cmake --preset ninja-release-x86
   ```
   只删缓存文件，不删 build 目录，已编译的 .obj 保留，增量编译即可。

## 文件修改清单

- `CMakePresets.json` — 三个 Preset 的 `cacheVariables` 块各加三行
