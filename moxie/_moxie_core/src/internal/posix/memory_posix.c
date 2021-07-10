#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <execinfo.h>
#include <sys/mman.h>

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
#if defined(_SC_PAGESIZE)
    return (size_t) sysconf(_SC_PAGESIZE);
#else
    return (size_t) getpagesize();
#endif
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
    int         access = PROT_NONE;
    int          flags = MAP_PRIVATE | MAP_ANONYMOUS;
    
    if (access_flags & MEM_ACCESS_FLAG_READ) {
        access = PROT_READ;
    }
    if (access_flags & MEM_ACCESS_FLAG_WRITE) {
        access = PROT_READ | PROT_WRITE;
    }
    if ((p = mmap(NULL, commit_size, access, flags, -1, 0)) != MAP_FAILED) {
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
    int access   = PROT_NONE;
    if (address == NULL && region_size == 0) {
        return true;
    }
    if (address == NULL || region_size == 0) {
        assert(region_size != 0);
        assert(address != NULL);
        errno = EINVAL;
        return false;
    }
    if (access_flags & MEM_ACCESS_FLAG_READ) {
        access = PROT_READ;
    }
    if (access_flags & MEM_ACCESS_FLAG_WRITE) {
        access = PROT_READ | PROT_WRITE;
    }
    return mprotect(address, region_size, access) == 0;
}

bool
mem_vmm_release
(
    void      *address, 
    size_t region_size
)
{
    if (address != NULL) {
        if (region_size != 0) {
            int res = munmap(address, region_size);
            return(res == 0);
        } else {
            assert(region_size != 0);
            errno  = EINVAL;
            return false;
        }
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
    void *p = NULL;
    int ret = 0;

    if (alignment < sizeof(void*)) {
        /* posix_memalign only supports alignments of at least pointer-width */
        alignment = sizeof(void*);
    }
    if ((alignment & (alignment-1)) != 0) {
        assert(0 && "alignment must be a power of two");
        return NULL;
    }
    if ((ret = posix_memalign(&p, alignment, min_size_bytes)) == 0) {
        return p;
    } else {
        assert(ret != EINVAL && "Invalid value for alignment argument");
        return NULL;
    }
}

void
mem_heap_release
(
    void *address
)
{
    free(address);
}

