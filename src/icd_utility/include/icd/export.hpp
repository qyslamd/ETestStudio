#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#    if defined(ICD_UTILITY_STATIC)
#        define ICD_UTILITY_API
#        define ICD_UTILITY_LOCAL
#    elif defined(ICD_UTILITY_BUILDING_DLL)
#        define ICD_UTILITY_API __declspec(dllexport)
#        define ICD_UTILITY_LOCAL
#    elif defined(ICD_UTILITY_SHARED)
#        define ICD_UTILITY_API __declspec(dllimport)
#        define ICD_UTILITY_LOCAL
#    else
#        define ICD_UTILITY_API
#        define ICD_UTILITY_LOCAL
#    endif
#else
#    if defined(__GNUC__) && __GNUC__ >= 4
#        define ICD_UTILITY_API __attribute__((visibility("default")))
#        define ICD_UTILITY_LOCAL __attribute__((visibility("hidden")))
#    else
#        define ICD_UTILITY_API
#        define ICD_UTILITY_LOCAL
#    endif
#endif
