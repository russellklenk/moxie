#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <Windows.h>

#include "internal/memory.h"

void
mem_copy
(
    void       * __restrict dst, 
    void const * __restrict src, 
    size_t                  num
)
{
    (void) memcpy(dst, src, num);
}

void
mem_move
(
    void       *dst, 
    void const *src, 
    size_t      num
)
{
    (void) memmove(dst, src, num);
}

void
mem_zero
(
    void  *mem, 
    size_t num
)
{
    (void) memset(mem, 0, num);
}

int
mem_compare
(
    void const *rgn1, 
    void const *rgn2, 
    size_t       num
)
{
    return memcmp(rgn1, rgn2, num);
}

size_t
mem_page_size
(
    void
)
{
    SYSTEM_INFO sysinfo;
    GetNativeSystemInfo(&sysinfo);
    return (size_t) sysinfo.dwPageSize;
}

void*
mem_vmm_allocate
(
    size_t min_size_bytes, 
    uint32_t access_flags, 
    size_t *o_actual_size
)
{
    size_t   page_size = mem_page_size();
    size_t commit_size = mem_align_up(min_size_bytes, page_size);
    void            *p = NULL;
    DWORD        flags = MEM_RESERVE | MEM_COMMIT;
    DWORD       access;
    
    if (access_flags & MEM_ACCESS_FLAG_WRITE) {
        access = PAGE_READWRITE;
    } else if (access_flags & MEM_ACCESS_FLAG_READ) {
        access = PAGE_READONLY;
    } else {
        access = PAGE_NOACCESS;
    }
    if ((p = VirtualAlloc(NULL, commit_size, flags, access)) != NULL) {
        if (o_actual_size) {
           *o_actual_size = commit_size;
        } return p;
    } else {
        if (o_actual_size) {
           *o_actual_size = 0;
        } return NULL;
    }
}

bool
mem_vmm_protect
(
    void         *address, 
    size_t    region_size,
    uint32_t access_flags 
)
{
    DWORD new_access = PAGE_NOACCESS;
    DWORD old_access = PAGE_NOACCESS;
    if (address == NULL && region_size == 0) {
        return true;
    }
    if (address == NULL || region_size == 0) {
        assert(region_size != 0);
        assert(address != NULL);
        errno = EINVAL;
        return false;
    }
    if (access_flags & MEM_ACCESS_FLAG_WRITE) {
        new_access = PAGE_READWRITE;
    } else if (access_flags & MEM_ACCESS_FLAG_READ) {
        new_access = PAGE_READONLY;
    } else {
        new_access = PAGE_NOACCESS;
    }
    return VirtualProtect(address, region_size, new_access, &old_access) != FALSE;
}

bool
mem_vmm_release
(
    void      *address, 
    size_t region_size
)
{
    if (address != NULL) {
        UNREFERENCED_PARAMETER(region_size);
        return VirtualFree(address, 0, MEM_RELEASE) != FALSE;
    } else {
        return true;
    }
}

void*
mem_heap_allocate
(
    size_t min_size_bytes, 
    size_t      alignment
)
{
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    if ((alignment & (alignment-1)) != 0) {
        assert(0 && "alignment must be a power of two");
        return NULL;
    }
    return _aligned_malloc(min_size_bytes, alignment);
}

void
mem_heap_release
(
    void *address
)
{
    _aligned_free(address);
}

