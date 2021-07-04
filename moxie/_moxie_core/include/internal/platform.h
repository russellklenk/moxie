/**
 * platform.h: Detect the active compiler, target platform, processor architecture and endianess.
 */
#ifndef MOXIE_CORE_PLATFORM_H
#define MOXIE_CORE_PLATFORM_H

#pragma once

#ifndef PLATFORM_TARGET_CONSTANTS
#   define PLATFORM_TARGET_CONSTANTS
#   define PLATFORM_TARGET_UNKNOWN                                             0
#   define PLATFORM_TARGET_iOS                                                 1
#   define PLATFORM_TARGET_ANDROID                                             2
#   define PLATFORM_TARGET_WIN_LEGACY                                          3
#   define PLATFORM_TARGET_WIN_MODERN                                          4
#   define PLATFORM_TARGET_MACOS                                               5
#   define PLATFORM_TARGET_LINUX                                               6
#endif

#ifndef PLATFORM_COMPILER_CONSTANTS
#   define PLATFORM_COMPILER_CONSTANTS
#   define PLATFORM_COMPILER_UNKNOWN                                           0
#   define PLATFORM_COMPILER_MSVC                                              1
#   define PLATFORM_COMPILER_GNUC                                              2
#   define PLATFORM_COMPILER_CLANG                                             3
#endif

#ifndef PLATFORM_ARCHITECTURE_CONSTANTS
#   define PLATFORM_ARCHITECTURE_CONSTANTS
#   define PLATFORM_ARCHITECTURE_UNKNOWN                                       0
#   define PLATFORM_ARCHITECTURE_X86_32                                        1
#   define PLATFORM_ARCHITECTURE_X86_64                                        2
#   define PLATFORM_ARCHITECTURE_ARM_32                                        3
#   define PLATFORM_ARCHITECTURE_ARM_64                                        4
#   define PLATFORM_ARCHITECTURE_PPC                                           5
#endif

#ifndef PLATFORM_ENDIANESS_CONSTANTS
#   define PLATFORM_ENDIANESS_CONSTANTS
#   define PLATFORM_ENDIANESS_UNKNOWN                                          0
#   define PLATFORM_ENDIANESS_LSB_FIRST                                        1
#   define PLATFORM_ENDIANESS_MSB_FIRST                                        2
#endif

#ifndef PLATFORM_TARGET_COMPILER
#   define PLATFORM_TARGET_COMPILER                                            PLATFORM_COMPILER_UNKNOWN
#   define PLATFORM_TARGET_COMPILER_NAME                                       "Unknown"
#endif

#ifndef PLATFORM_TARGET_PLATFORM
#   define PLATFORM_TARGET_PLATFORM                                            PLATFORM_TARGET_UNKNOWN
#   define PLATFORM_TARGET_PLATFORM_NAME                                       "Unknown"
#endif

#ifndef PLATFORM_TARGET_ARCHITECTURE
#   define PLATFORM_TARGET_ARCHITECTURE                                        PLATFORM_ARCHITECTURE_UNKNOWN
#   define PLATFORM_TARGET_ARCHITECTURE_NAME                                   "Unknown"
#endif

#ifndef PLATFORM_TARGET_ENDIANESS
#   define PLATFORM_TARGET_ENDIANESS                                           PLATFORM_ENDIANESS_UNKNOWN
#   define PLATFORM_TARGET_ENDIANESS_NAME                                      "Unknown"
#endif

#ifndef PLATFORM_SYSTEM_ENDIANESS
#   if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#       define PLATFORM_SYSTEM_ENDIANESS                                       PLATFORM_ENDIANESS_MSB_FIRST
#   else
#       define PLATFORM_SYSTEM_ENDIANESS                                       PLATFORM_ENDIANESS_LSB_FIRST
#   endif
#endif

#if PLATFORM_SYSTEM_ENDIANESS == PLATFORM_ENDIANESS_LSB_FIRST
#   undef  PLATFORM_TARGET_ENDIANESS
#   undef  PLATFORM_TARGET_ENDIANESS_NAME
#   define PLATFORM_TARGET_ENDIANESS                                           PLATFORM_ENDIANESS_LSB_FIRST
#   define PLATFORM_TARGET_ENDIANESS_NAME                                      "Little Endian"
#elif PLATFORM_SYSTEM_ENDIANESS == PLATFORM_ENDIANESS_MSB_FIRST
#   undef  PLATFORM_TARGET_ENDIANESS
#   undef  PLATFORM_TARGET_ENDIANESS_NAME
#   define PLATFORM_TARGET_ENDIANESS                                           PLATFORM_ENDIANESS_MSB_FIRST
#   define PLATFORM_TARGET_ENDIANESS_NAME                                      "Big Endian"
#else
#   error  platform.h: Failed to detect system endianess. Update endianess detection or manually define PLATFORM_SYSTEM_ENDIANESS.
#endif

#if PLATFORM_TARGET_COMPILER == PLATFORM_COMPILER_UNKNOWN
#   if   defined(_MSC_VER)
#       undef  PLATFORM_TARGET_COMPILER
#       undef  PLATFORM_TARGET_COMPILER_NAME
#       define PLATFORM_TARGET_COMPILER                                        PLATFORM_COMPILER_MSVC
#       define PLATFORM_TARGET_COMPILER_NAME                                   "MSVC"
#   elif defined(__clang__)
#       undef  PLATFORM_TARGET_COMPILER
#       undef  PLATFORM_TARGET_COMPILER_NAME
#       define PLATFORM_TARGET_COMPILER                                        PLATFORM_COMPILER_CLANG
#       define PLATFORM_TARGET_COMPILER_NAME                                   "Clang"
#   elif defined(__GNUC__)
#       undef  PLATFORM_TARGET_COMPILER
#       undef  PLATFORM_TARGET_COMPILER_NAME
#       define PLATFORM_TARGET_COMPILER                                        PLATFORM_COMPILER_GNUC
#       define PLATFORM_TARGET_COMPILER_NAME                                   "GNU"
#   else
#       error  platform.h: Failed to detect target compiler. Update compiler detection.
#   endif
#endif

#if PLATFORM_TARGET_ARCHITECTURE == PLATFORM_ARCHITECTURE_UNKNOWN
#   if   defined(__aarch64__) || defined(_M_ARM64)
#       undef  PLATFORM_TARGET_ARCHITECTURE
#       undef  PLATFORM_TARGET_ARCHITECTURE_NAME
#       define PLATFORM_TARGET_ARCHITECTURE                                    PLATFORM_ARCHITECTURE_ARM_64
#       define PLATFORM_TARGET_ARCHITECTURE_NAME                               "ARM64"
#   elif defined(__arm__) || defined(_M_ARM)
#       undef  PLATFORM_TARGET_ARCHITECTURE
#       undef  PLATFORM_TARGET_ARCHITECTURE_NAME
#       define PLATFORM_TARGET_ARCHITECTURE                                    PLATFORM_ARCHITECTURE_ARM_32
#       define PLATFORM_TARGET_ARCHITECTURE_NAME                               "ARM32"
#   elif defined(__amd64__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#       undef  PLATFORM_TARGET_ARCHITECTURE
#       undef  PLATFORM_TARGET_ARCHITECTURE_NAME
#       define PLATFORM_TARGET_ARCHITECTURE                                    PLATFORM_ARCHITECTURE_X86_64
#       define PLATFORM_TARGET_ARCHITECTURE_NAME                               "x86_64"
#   elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86) || defined(_X86_)
#       undef  PLATFORM_TARGET_ARCHITECTURE
#       undef  PLATFORM_TARGET_ARCHITECTURE_NAME
#       define PLATFORM_TARGET_ARCHITECTURE                                    PLATFORM_ARCHITECTURE_X86_32
#       define PLATFORM_TARGET_ARCHITECTURE_NAME                               "x86"
#   elif defined(__ppc__) || defined(__powerpc__) || defined(__PPC__)
#       undef  PLATFORM_TARGET_ARCHITECTURE
#       undef  PLATFORM_TARGET_ARCHITECTURE_NAME
#       define PLATFORM_TARGET_ARCHITECTURE                                    PLATFORM_ARCHITECTURE_PPC
#       define PLATFORM_TARGET_ARCHITECTURE_NAME                               "PowerPC"
#   else
#       error  platform.h: Failed to detect target architecture. Update architecture detection.
#   endif
#endif

#if PLATFORM_TARGET_PLATFORM == PLATFORM_TARGET_UNKNOWN
#   if   defined(ANDROID)
#       undef  PLATFORM_TARGET_PLATFORM
#       undef  PLATFORM_TARGET_PLATFORM_NAME
#       define PLATFORM_TARGET_PLATFORM                                        PLATFORM_TARGET_ANDROID
#       define PLATFORM_TARGET_PLATFORM_NAME                                   "Android"
#   elif defined(__APPLE__)
#       include <TargetConditionals.h>
#       if   defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
#           undef  PLATFORM_TARGET_PLATFORM
#           undef  PLATFORM_TARGET_PLATFORM_NAME
#           define PLATFORM_TARGET_PLATFORM                                    PLATFORM_TARGET_iOS
#           define PLATFORM_TARGET_PLATFORM_NAME                               "iOS"
#       else
#           undef  PLATFORM_TARGET_PLATFORM
#           undef  PLATFORM_TARGET_PLATFORM_NAME
#           define PLATFORM_TARGET_PLATFORM                                    PLATFORM_TARGET_MACOS
#           define PLATFORM_TARGET_PLATFORM_NAME                               "macOS"
#       endif
#   elif defined(_WIN32) || defined(_WIN64) || defined(__cplusplus_winrt) || defined(__CYGWIN__)
#       if   defined(__cplusplus_winrt)
#           undef  PLATFORM_TARGET_PLATFORM
#           undef  PLATFORM_TARGET_PLATFORM_NAME
#           define PLATFORM_TARGET_PLATFORM                                    PLATFORM_TARGET_WIN_MODERN
#           define PLATFORM_TARGET_PLATFORM_NAME                               "WinRT/UWP"
#       else
#           undef  PLATFORM_TARGET_PLATFORM
#           undef  PLATFORM_TARGET_PLATFORM_NAME
#           define PLATFORM_TARGET_PLATFORM                                    PLATFORM_TARGET_WIN_LEGACY
#           define PLATFORM_TARGET_PLATFORM_NAME                               "WinNative"
#       endif
#   elif defined(__linux__) || defined(__gnu_linux__)
#       undef  PLATFORM_TARGET_PLATFORM
#       undef  PLATFORM_TARGET_PLATFORM_NAME
#       define PLATFORM_TARGET_PLATFORM                                        PLATFORM_TARGET_LINUX
#       define PLATFORM_TARGET_PLATFORM_NAME                                   "Linux"
#   else
#       error  platform.h: Failed to detect target platform. Update platform detection.
#   endif
#endif

#  if PLATFORM_TARGET_COMPILER == PLATFORM_COMPILER_MSVC
#   define PLATFORM_NEVER_INLINE                                               __declspec(noinline)
#   define PLATFORM_FORCE_INLINE                                               __forceinline
#   define PLATFORM_STRUCT_ALIGN(_alignment)                                   __declspec(align(_alignment))
#   define PLATFORM_ALIGN_OF(_type)                                            __alignof(_type)
#   define PLATFORM_SIZE_OF(_type)                                             sizeof(_type)
#   define PLATFORM_OFFSET_OF(_type, _field)                                   offsetof(_type, _field)
#   define PLATFORM_UNUSED_PARAM(_var)                                         (void)(_var)
#   define PLATFORM_UNUSED_LOCAL(_var)                                         (void)(_var)
#   ifdef __cplusplus
#       define PLATFORM_INLINE                                                 inline
#   else
#       define PLATFORM_INLINE                                                 
#   endif
#elif PLATFORM_TARGET_COMPILER != PLATFORM_COMPILER_UNKNOWN
#   define PLATFORM_NEVER_INLINE                                               __attribute__((noinline))
#   define PLATFORM_FORCE_INLINE                                               __attribute__((always_inline))
#   define PLATFORM_STRUCT_ALIGN(_alignment)                                   __attribute__((aligned(_alignment)))
#   define PLATFORM_ALIGN_OF(_type)                                            __alignof__(_type)
#   define PLATFORM_SIZE_OF(_type)                                             sizeof(_type)
#   define PLATFORM_OFFSET_OF(_type, _field)                                   offsetof(_type, _field)
#   define PLATFORM_UNUSED_PARAM(_var)                                         (void)(sizeof(_var))
#   define PLATFORM_UNUSED_LOCAL(_var)                                         (void)(sizeof(_var))
#   ifdef __cplusplus
#       define PLATFORM_INLINE                                                 inline
#   else
#       define PLATFORM_INLINE                                                 
#   endif
#endif

#endif /* MOXIE_CORE_PLATFORM_H */
