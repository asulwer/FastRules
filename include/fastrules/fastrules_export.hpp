#pragma once

// Cross-platform DLL/shared library export macros for the C++ core library.
// The public C API and its export macro (FASTRULES_C_API) live in fastrules.h.
#ifdef FASTRULES_STATIC
    #define FASTRULES_API
    #define FASTRULES_LOCAL
#else
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
#endif
