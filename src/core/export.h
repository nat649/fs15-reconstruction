#pragma once
#if defined(_WIN32) && !defined(FS15_CORE_STATIC)
# if defined(FS15_CORE_BUILD)
#  define FS15_CORE_API __declspec(dllexport)
# else
#  define FS15_CORE_API __declspec(dllimport)
# endif
#elif defined(__GNUC__) && !defined(FS15_CORE_STATIC)
# define FS15_CORE_API __attribute__((visibility("default")))
#else
# define FS15_CORE_API
#endif
