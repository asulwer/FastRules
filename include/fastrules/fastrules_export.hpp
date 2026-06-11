#pragma once

// Cross-platform DLL/shared library export macros
#ifdef _WIN32
    #ifdef FASTRULES_BUILDING_SHARED
        #define FASTRULES_API __declspec(dllexport)
    #else
        #define FASTRULES_API __declspec(dllimport)
    #endif
    #define FASTRULES_LOCAL
#else
    #if __GNUC__ >= 4
        #define FASTRULES_API __attribute__((visibility("default")))
        #define FASTRULES_LOCAL __attribute__((visibility("hidden")))
    #else
        #define FASTRULES_API
        #define FASTRULES_LOCAL
    #endif
#endif

// C API functions are always exported when building shared
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #ifdef FASTRULES_BUILDING_SHARED
        #define FASTRULES_C_API __declspec(dllexport)
    #else
        #define FASTRULES_C_API __declspec(dllimport)
    #endif
#else
    #define FASTRULES_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
}
#endif
