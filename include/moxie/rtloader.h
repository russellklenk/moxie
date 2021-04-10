/**
 * rtloader.h: Defines some macros to abstract around platform differences in 
 * code that dynamically loads shared objects or dynamic link libraries at 
 * runtime. It is intended to be self-contained and included directly into 
 * runtime loader implementation files. 
 */
#ifndef __MOXIE_CORE_RTLOADER_H__
#define __MOXIE_CORE_RTLOADER_H__

#pragma once

#if defined(_WIN32) || defined(_WIN64) || defined(__cplusplus_winrt) || defined(__CYGWIN__)
#   ifndef MOXIE_CORE_NO_INCLUDES
#       include <Windows.h>
#   endif /* MOXIE_CORE_NO_INCLUDES */
#   ifndef RTLOADER_PLATFORM_TYPES
#       define RTLOADER_PLATFORM_TYPES
#       define RTLOADER_MODULE_INVALID                                         ((rtloader_module_t) NULL)
#       define RTLOADER_SYMBOL_INVALID                                         NULL
#       define RTLOADER_ERROR_FMT                                              "0x%08u"
#       define RTLOADER_ERROR_NONE                                             ((rtloader_error_t ) ERROR_SUCCESS)
#       define rtloader_error_t                                                DWORD
#       define rtloader_module_t                                               HMODULE
#       define rtloader_module_load(_module_id_utf8_cstr)                      ((rtloader_module_t) LoadLibraryA(_module_id_utf8_cstr))
#       define rtloader_module_free(_rtloader_module_handle)                   ((void )FreeLibrary((HMODULE)_rtloader_module_handle))
#       define rtloader_module_rsym(_rtloader_module_handle, _sym_ascii_cstr)  ((void*)GetProcAddress(_rtloader_module_handle, _sym_ascii_cstr))
#       define rtloader_get_error()                                            ((rtloader_error_t)  GetLastError())
#       define rtloader_clr_error()                                            ((void            )  SetLastError(ERROR_SUCCESS))
#   endif /* RTLOADER_PLATFORM_TYPES */
#else /* Assume dlopen/dlsym/dlclose */
#   ifndef MOXIE_CORE_NO_INCLUDES
#       include <dlfcn.h>
#   endif /* MOXIE_CORE_NO_INCLUDES */
#   ifndef RTLOADER_PLATFORM_TYPES
#       define RTLOADER_PLATFORM_TYPES
#       define RTLOADER_MODULE_INVALID                                         ((rtloader_module_t) NULL)
#       define RTLOADER_SYMBOL_INVALID                                         NULL
#       define RTLOADER_ERROR_FMT                                              "%s"
#       define RTLOADER_ERROR_NONE                                             ((rtloader_error_t ) NULL)
#       define rtloader_error_t                                                char*
#       define rtloader_module_t                                               void*
#       define rtloader_module_load(_module_id_utf8_cstr)                      ((rtloader_module_t) dlopen(_module_id_utf8_cstr, RTLD_NOW | RTLD_GLOBAL))
#       define rtloader_module_free(_rtloader_module_handle)                   ((void )dlclose((void*)_rtloader_module_handle))
#       define rtloader_module_rsym(_rtloader_module_handle, _sym_ascii_cstr)  ((void*)dlsym  (_rtloader_module_handle, _sym_ascii_cstr))
#       define rtloader_get_error()                                            ((rtloader_error_t)  dlerror())
#       define rtloader_clr_error()                                            ((void            )  dlerror())
#   endif /* RTLOADER_PLATFORM_TYPES */
#endif

#ifndef RTLOADER_PLATFORM_TYPES
#   error rtloader.h: No platform-specific types for your platform.
#endif

/**
 * Helper macros to make it easy to resolve a symbol from a runtime-loaded module.
 * These depend on some naming conventions:
 * - The function pointer type name must be PFN_name_of_symbol.
 * - There must be a dispatch table with fields PFN_name_of_symbol name_of_symbol.
 * - There must be a defined stub function, with name name_of_symbol_STUB with the same signature as the symbol.
 * The rtloader_resolve macro sets the dispatch table entry for the symbol equal to EITHER the symbol loaded from the module, or to the fallback stub function.
 * For example:
 * - typedef void (*PFN_my_function)(int arg1, int arg2);
 * 
 * - typedef struct my_dispatch_table_t {
 *       PFN_my_function    my_function;
 *   } my_dispatch_table_t;
 * 
 * - static void my_function_STUB(int arg1, int arg2) {
 *       // Do something appropriate in case the function is missing...
 *   }
 * 
 * - rtloader_module_t my_rt_module = RTLOADER_MODULE_INVALID;
 *   my_dispatch_table_t  rt_fnptrs;
 *   if ((my_rt_module = rtloader_module_load("libmymodule.so.1")) == RTLOADER_MODULE_INVALID) {
 *       // failed to load libmymodule.so.1
 *   }
 *   rtloader_resolve(rt_fnptrs, my_rt_module, my_function); // Resolve symbol my_function, fall back to my_function_STUB if it doesn't exist.
 *   // At this point, rt_fnptrs->my_function is guaranteed to point to either the symbol from libmymodule.so.1, or to my_function_STUB.
 */
#ifndef RTLOADER_HELPER_MACROS
#   define RTLOADER_HELPER_MACROS
#   define rtloader_resolve(_dispatch_table, _rtloader_module_handle, _sym)    \
    for ( ; ; ) {                                                              \
        if (((_dispatch_table)->_sym = (PFN_##_sym) rtloader_module_rsym(_rtloader_module_handle, #_sym)) == RTLOADER_SYMBOL_INVALID) { \
             (_dispatch_table)->_sym = _sym##_STUB;                            \
        } break;                                                               \
    }

#   define rtloader_stubsym(_dispatch_table, _sym)                             \
    (_dispatch_table)->_sym = _sym##_STUB

#   define rtloader_symstub(_dispatch_table, _sym)                             \
    (_dispatch_table)->_sym ==_sym##_STUB
#endif

#endif /* __MOXIE_CORE_RTLOADER_H__ */
