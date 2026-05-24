#include "ThemeState.h"

namespace etest::core::common {

namespace {
bool g_dark_mode = true;
}

bool isDarkTheme() { return g_dark_mode; }
void setDarkTheme(bool dark) { g_dark_mode = dark; }

}  // namespace etest::core::common
