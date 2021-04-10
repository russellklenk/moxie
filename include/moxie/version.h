/**
 * version.h: Functions for retrieving library version information.
 */
#ifndef __MOXIE_CORE_VERSION_H__
#define __MOXIE_CORE_VERSION_H__

#pragma once

#ifndef MOXIE_NO_INCLUDES
#   include <stddef.h>
#   include <stdint.h>
#endif

typedef struct moxie_core_version_info_t {
    char const        *version_string;                                         /* A nul-terminated, UTF-8 encoded string representing the formatted library version. */
    char const         *compiler_name;                                         /* A nul-terminated, UTF-8 encoded string specifying the name of the compiler used to build the library. */
    char const         *platform_name;                                         /* A nul-terminated, UTF-8 encoded string specifying the name of the library target platform. */
    char const    *cpu_endianess_name;                                         /* A nul-terminated, UTF-8 encoded string specifying the default endianess of the library. */
    char const *cpu_architecture_name;                                         /* A nul-terminated, UTF-8 encoded string specifying the name of the CPU architecture the library was built for. */
    int32_t             version_major;                                         /* The major version component of the library version. */
    int32_t             version_minor;                                         /* The minor version component of the library version. */
    int32_t             version_patch;                                         /* The patch version component of the library version. */
    int32_t               compiler_id;                                         /* One of the PLATFORM_COMPILER_x values from platform.h indicating the compiler the library was built with. */
    int32_t               platform_id;                                         /* One of the PLATFORM_TARGET_x values from platform.h indicating the target platform the library was built for. */
    int32_t          cpu_endianess_id;                                         /* One of the PLATFORM_ENDIANESS_x values from platform.h indicating the byte order the library assumes as the default. */
    int32_t       cpu_architecture_id;                                         /* One of the PLATFORM_ARCHITECTURE_x values from platform.h indicating the processor architecture the library was built for. */
    uint32_t    runtime_warning_flags;                                         /* One or more bitwise OR'd values of the runtime_warning_flags_e enumeration. */
} moxie_core_version_info_t;

#ifndef MOXIE_CORE_VERSION_INFO_UNKNOWN
#   define MOXIE_CORE_VERSION_INFO_UNKNOWN                                     \
    {                                                                          \
        "Unknown", /* version_string                                        */ \
        "Unknown", /* compiler_name                                         */ \
        "Unknown", /* platform_name                                         */ \
        "Unknown", /* cpu_endianess_name                                    */ \
        "Unknown", /* cpu_architecture_name                                 */ \
                0, /* version_major                                         */ \
                0, /* version_minor                                         */ \
                0, /* version_patch                                         */ \
                0, /* compiler_id                                           */ \
                0, /* platform_id                                           */ \
                0, /* cpu_endianess_id                                      */ \
                0, /* cpu_architecture_id                                   */ \
                0  /* runtime_warning_flags                                 */ \
    }
#endif

typedef enum  runtime_warning_flags_e {
    RUNTIME_WARNING_FLAGS_NONE                                   = (0U <<  0), /* No runtime warnings are reported. */
    RUNTIME_WARNING_FLAG_ENDIANESS_MISMATCH                      = (1U <<  0), /* The compile-time detected endianess does not match the runtime-detected endianess. */
} runtime_warning_flags_e;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize a moxie_coe_version_info_t structure to "unknown" version information.
 * @param out_version: A pointer to the structure to populate.
 */
extern void
moxie_core_version_info_unknown
(
    struct moxie_core_version_info_t *out_version
);

/**
 * Retrieve the version information for the library.
 * @param out_version: A pointer to the structure to populate.
 */
extern void
moxie_core_version
(
    struct moxie_core_version_info_t *out_version
);

/**
 * Retrieve a string description of the library version.
 * @return A nul-terminated, UTF-8 encoded string specifying the library version. Do not attempt to modify or free the returned string.
 */
extern char const*
moxie_core_version_string
(
    void
);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* __MOXIE_CORE_VERSION_H__ */
