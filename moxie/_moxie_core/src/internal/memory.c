#include <errno.h>
#include <assert.h>

#include "internal/memory.h"

static const size_t     DEFAULT_ALIGNMENT = 16;
static const char      *UNNAMED_ALLOCATOR = "(Unnamed mem_allocator)";
static const mem_tag_t UNTAGGED_ALLOCATOR = mem_tag('N','O','N','E');

#ifndef mem_is_power_of_two
#   define mem_is_power_of_two(_value)                                         \
    (((_value) & ((_value)-1)) == 0)
#endif

/**
 * Compute the next power of two value greater than or equal to a given value.
 * @param n The input value.
 * @return The next non-zero power of two greater than or equal to n.
 */
static uint32_t
mem_next_pow2_greater_or_equal
(
    uint32_t n
)
{
    uint32_t v = 1;
    do {
        if (v < n) {
            v <<= 1;
        } else {
            return v;
        }
    } while (v < UINT32_MAX);
    return (1U << 31);
}

/**
 * Perform a consistency check and adjust chunk allocation attributes.
 * @param p_alignment : The address of the argument specifying the desired allocation alignment.
 * @param p_chunk_size: The address of the argument specifying the base allocation size for the chunk.
 * @param p_guard_size: The address of the argument specifying the number of guard bytes to allocate.
 * @param p_page_size : On return, the system page size is stored here.
 * @param p_flags     : A combination of one or more bitwise OR'd values of the mem_allocation_flags_e enumeration.
 */
static void
mem_sanitize_attributes
(
    size_t   *p_alignment , /* in/out */
    size_t   *p_chunk_size, /* in/out */
    size_t   *p_guard_size, /* in/out */
    size_t   *p_page_size , /* out */
    uint32_t *p_flags       /* in/out */
)
{
    assert(p_alignment  != NULL && "Expected non-null p_alignment argument");
    assert(p_chunk_size != NULL && "Expected non-null p_chunk_size argument");
    assert(p_guard_size != NULL && "Expected non-null p_guard_size argument");
    assert(p_page_size  != NULL && "Expected non-null p_page_size argument");
    assert(p_flags      != NULL && "Expected non-null p_flags argument");

    size_t  page_size = mem_page_size();
    size_t  alignment = *p_alignment;
    size_t chunk_size = *p_chunk_size;
    size_t guard_size = *p_guard_size;
    uint32_t    flags = *p_flags; 

    /* Apply default values */
    if (flags == MEM_ALLOCATION_FLAGS_NONE) {
        flags  =(uint32_t)(MEM_ALLOCATION_FLAG_LOCAL | MEM_ALLOCATION_FLAG_HEAP  | MEM_ALLOCATION_FLAG_GROWABLE);
    }
    if (alignment == 0) {
        alignment  = DEFAULT_ALIGNMENT;
    }
    flags |= MEM_ALLOCATION_FLAG_LOCAL; /* Memory always visible to the calling process */

    /* Eliminate invalid flag combinations */
    if (guard_size != 0) {
        /* Guard pages require the use of virtual memory */
        flags |= MEM_ALLOCATION_FLAG_VIRTUAL;
    }
    if (flags & MEM_ALLOCATION_FLAG_SHARED) {
        /* Shared memory requires the use of virtual memory */
        if ((flags & MEM_ALLOCATION_FLAG_VIRTUAL) == 0) {
            flags |= MEM_ALLOCATION_FLAG_VIRTUAL;
        }
    }
    if (flags & MEM_ALLOCATION_FLAG_VIRTUAL) {
        /* Use of virtual memory overrides use of heap memory */
        flags &= ~MEM_ALLOCATION_FLAG_HEAP;
        if (chunk_size != 0) {
            /* Allocations occur in multiples of the system page size */
            chunk_size = mem_align_up(chunk_size, page_size);
        }
        if (guard_size != 0) {
            /* Allocations occur in multiples of the system page size */
            guard_size = mem_align_up(guard_size, page_size);
        }
    }

    *p_alignment  = alignment;
    *p_chunk_size = chunk_size;
    *p_guard_size = guard_size;
    *p_page_size  = page_size;
    *p_flags      = flags;
}

void
mem_chunk_init
(
    struct mem_chunk_t *chunk, 
    void              *memory, 
    size_t             length
)
{
    if (chunk != NULL) {
        if (memory != NULL) {
            chunk->next           = NULL;
            chunk->memory_start   =(uint8_t*) memory;
            chunk->next_offset    = 0;
            chunk->maximum_offset =(uint64_t) length;
        } else {
            assert(length == 0 && "Cannot initialize chunk with NULL memory block of non-zero length");
            chunk->next           = NULL;
            chunk->memory_start   = NULL;
            chunk->next_offset    = 0;
            chunk->maximum_offset = 0;
        }
    }
}

struct mem_chunk_t*
mem_chunk_allocate
(
    size_t chunk_size,
    size_t guard_size,
    size_t  alignment,
    uint32_t    flags, 
    uint32_t   access
)
{
    mem_chunk_t *chunk = NULL;
    void       *memory = NULL;
    size_t   node_size = mem_size_of (mem_chunk_t);
    size_t  node_align = mem_align_of(mem_chunk_t);
    size_t   page_size = 0;

    /* Chunk nodes are always allocated on the system heap */
    mem_sanitize_attributes(&alignment, &chunk_size, &guard_size, &page_size, &flags);
    if ((chunk = (mem_chunk_t*) mem_heap_allocate(node_size, node_align)) == NULL) {
        return NULL;
    }
    if ((chunk_size + guard_size) == 0 || (flags & MEM_ALLOCATION_FLAG_EXTERNAL) != 0) {
        mem_chunk_init(chunk, NULL, 0);
        return chunk;
    }
    /* Allocate the storage the chunk manages */
    if (flags & MEM_ALLOCATION_FLAG_VIRTUAL) {
        memory = mem_vmm_allocate(chunk_size + guard_size, access, NULL);
        if (memory != NULL && guard_size != 0) {
            uint8_t *guard_base =(uint8_t*) memory + chunk_size;
            mem_vmm_protect(guard_base, guard_size, MEM_ACCESS_FLAGS_NONE);
        }
    } else {
        memory = mem_heap_allocate(chunk_size, alignment);
    }

    /* Initialize the chunk fields */
    if (memory != NULL) {
        mem_chunk_init(chunk, memory, chunk_size);
        return chunk;
    } else {
        mem_heap_release(chunk);
        return NULL;
    }
}

void
mem_chunk_release
(
    struct mem_chunk_t *chunk, 
    uint32_t            flags
)
{
    if (chunk != NULL) {
        mem_chunk_t *item = chunk->next;
        while (item != NULL) {
            chunk->next = item->next;
            item ->next = NULL;
            mem_chunk_release(item, flags);
            item = chunk->next;
        }
        if (flags & MEM_ALLOCATION_FLAG_EXTERNAL) {
            mem_chunk_init(chunk, NULL, 0);
            mem_heap_release(chunk);
            return;
        }
        if (flags & MEM_ALLOCATION_FLAG_VIRTUAL) {
            mem_vmm_release(chunk->memory_start, (size_t) chunk->maximum_offset);
            mem_chunk_init(chunk, NULL, 0);
            mem_heap_release(chunk);
            return;
        }
        if (flags & MEM_ALLOCATION_FLAG_HEAP) {
            mem_heap_release(chunk->memory_start);
            mem_chunk_init(chunk, NULL, 0);
            mem_heap_release(chunk);
            return;
        }
    }
}

struct mem_allocator_t*
mem_allocator_create
(
    struct mem_allocator_t *alloc,
    size_t             chunk_size,
    size_t             guard_size,
    size_t              alignment,
    uint32_t                flags,
    uint32_t               access,
    char const              *name,
    mem_tag_t                 tag
)
{
    mem_chunk_t *chunk = NULL;
    size_t   page_size = 0;

    if (alloc == NULL) {
        assert(alloc != NULL && "Expected non-null alloc argument");
        return NULL;
    }
    if (flags & MEM_ALLOCATION_FLAG_EXTERNAL) {
        assert((flags & MEM_ALLOCATION_FLAG_EXTERNAL) == 0);
        return NULL;
    }

    name = name ? name : UNNAMED_ALLOCATOR;
    tag  = tag  ? tag  : UNTAGGED_ALLOCATOR;
    mem_sanitize_attributes(&alignment, &chunk_size, &guard_size, &page_size, &flags);
    if ((chunk = mem_chunk_allocate(chunk_size, guard_size, alignment, flags, access)) != NULL) {
        alloc->tail              = chunk;
        alloc->head              = chunk;
        alloc->allocator_name    = name;
        alloc->chunk_size        =(uint64_t) chunk_size;
        alloc->high_watermark    = 0;
        alloc->allocator_version = 0;
        alloc->allocator_flags   = flags;
        alloc->access_flags      = access;
        alloc->guard_size        =(uint32_t) guard_size;
        alloc->page_size         =(uint32_t) page_size;
        alloc->allocator_tag     = tag;
        return alloc;
    } else {
        alloc->tail              = NULL;
        alloc->head              = NULL;
        alloc->allocator_name    = NULL;
        alloc->chunk_size        = 0;
        alloc->high_watermark    = 0;
        alloc->allocator_version = 0;
        alloc->allocator_flags   = MEM_ALLOCATION_FLAGS_NONE;
        alloc->access_flags      = MEM_ACCESS_FLAGS_NONE;
        alloc->guard_size        = 0;
        alloc->page_size         = 0;
        alloc->allocator_tag     = 0;
        return NULL;
    }
}

struct mem_allocator_t*
mem_allocator_create_with_memory
(
    struct mem_allocator_t *alloc,
    void                  *memory,
    size_t                 length,
    uint32_t           base_flags,
    uint32_t               access,
    char const              *name,
    mem_tag_t                 tag
)
{
    mem_chunk_t *chunk = NULL;
    size_t  chunk_size = length;
    size_t  guard_size = 0;
    size_t   page_size = 0;
    size_t   alignment = 1;
    uint32_t     flags = base_flags | MEM_ALLOCATION_FLAG_EXTERNAL;

    if (alloc == NULL) {
        assert(alloc != NULL && "Expected non-null alloc argument");
        return NULL;
    }
    if (memory == NULL && length != 0) {
        assert(memory != NULL && "Invalid memory block supplied");
        goto return_invalid_allocator;
    }
    if (flags &  MEM_ALLOCATION_FLAG_GROWABLE) {
        flags &=~MEM_ALLOCATION_FLAG_GROWABLE;
    }

    name = name ? name : UNNAMED_ALLOCATOR;
    tag  = tag  ? tag  : UNTAGGED_ALLOCATOR;
    mem_sanitize_attributes(&alignment, &chunk_size, &guard_size, &page_size, &flags);
    if ((chunk = mem_chunk_allocate(chunk_size, guard_size, alignment, flags, access)) != NULL) {
        chunk->memory_start      =(uint8_t*) memory;
        chunk->maximum_offset    =(uint64_t) length;
        alloc->tail              = chunk;
        alloc->head              = chunk;
        alloc->allocator_name    = name;
        alloc->chunk_size        =(uint64_t) length;
        alloc->high_watermark    = 0;
        alloc->allocator_version = 0;
        alloc->allocator_flags   = flags;
        alloc->access_flags      = access;
        alloc->guard_size        =(uint32_t) guard_size;
        alloc->page_size         =(uint32_t) page_size;
        alloc->allocator_tag     = tag;
        return alloc;
    }

return_invalid_allocator:
    alloc->tail              = NULL;
    alloc->head              = NULL;
    alloc->allocator_name    = NULL;
    alloc->chunk_size        = 0;
    alloc->high_watermark    = 0;
    alloc->allocator_version = 0;
    alloc->allocator_flags   = MEM_ALLOCATION_FLAGS_NONE;
    alloc->access_flags      = MEM_ACCESS_FLAGS_NONE;
    alloc->guard_size        = 0;
    alloc->page_size         = 0;
    alloc->allocator_tag     = 0;
    return NULL;
}

struct mem_allocator_t*
mem_allocator_create_suballocator
(
    struct mem_allocator_t  *alloc, 
    struct mem_allocator_t *parent,
    size_t                  length,
    char const               *name,
    mem_tag_t                  tag
)
{
    void *memory = NULL;
    size_t align = DEFAULT_ALIGNMENT;

    if (alloc == NULL) {
        assert(alloc != NULL && "Expected non-null alloc argument");
        return NULL;
    }
    if (parent == NULL) {
        assert(parent != NULL && "Expected non-null parent argument");
        goto return_invalid_allocator;
    }

    if ((memory = mem_allocator_alloc(parent, length, align)) != NULL) {
        return mem_allocator_create_with_memory(alloc, memory, length, parent->allocator_flags, parent->access_flags, name, tag);
    }

return_invalid_allocator:
    alloc->tail              = NULL;
    alloc->head              = NULL;
    alloc->allocator_name    = NULL;
    alloc->chunk_size        = 0;
    alloc->high_watermark    = 0;
    alloc->allocator_version = 0;
    alloc->allocator_flags   = MEM_ALLOCATION_FLAGS_NONE;
    alloc->access_flags      = MEM_ACCESS_FLAGS_NONE;
    alloc->guard_size        = 0;
    alloc->page_size         = 0;
    alloc->allocator_tag     = 0;
    return NULL;
}

void
mem_allocator_delete
(
    struct mem_allocator_t *alloc
)
{
    if (alloc != NULL) {
        mem_chunk_release(alloc->head, alloc->allocator_flags);
        alloc->head       = NULL;
        alloc->tail       = NULL;
        alloc->chunk_size = 0;
    }
}

struct mem_marker_t
mem_allocator_mark
(
    struct mem_allocator_t *alloc
)
{
    mem_marker_t marker = {
        .chunk   = alloc->tail, 
        .offset  = alloc->tail->next_offset,
        .tag     = alloc->allocator_tag,
        .version = alloc->allocator_version
    }; return marker;
}

void*
mem_allocator_alloc
(
    struct mem_allocator_t *alloc, 
    size_t                 length,
    size_t              alignment
)
{
    for ( ; ; ) {
        mem_chunk_t      *source = alloc->tail;
        uint8_t    *base_address = source->memory_start + source->next_offset;
        uintptr_t   address_uint =(uintptr_t) base_address;
        uint8_t *aligned_address =(uint8_t *) mem_align_up(address_uint, alignment);
        size_t       align_bytes =(size_t   )(aligned_address - base_address);
        size_t       alloc_bytes = length  +  align_bytes;
        size_t        new_offset = source->next_offset + alloc_bytes;

        if (new_offset <= source->maximum_offset) {
            /* Bump the offset to account for the allocated space */
            source->next_offset = new_offset;
            /* Bump the high watermark if necessary */
            if (new_offset > alloc->high_watermark) {
                alloc->high_watermark = new_offset;
            }
            return aligned_address;
        }

        /* Else, the request exceeds the capacity of the chunk */
        if (alloc->allocator_flags & MEM_ALLOCATION_FLAG_GROWABLE) {
            /* Allocate and append a new chunk from which the allocation will be satisfied */
            mem_chunk_t *new_source  = NULL;
            size_t       chunk_size  = alloc->chunk_size;
            size_t       chunk_align = DEFAULT_ALIGNMENT;
            if (chunk_align < alignment) {
                chunk_align = alignment;
            }
            if (chunk_size < length) {
                chunk_size = length  + alignment;
            }
            if ((new_source  = mem_chunk_allocate(chunk_size, alloc->guard_size, chunk_align, alloc->allocator_flags, alloc->access_flags)) != NULL) {
                source->next = new_source;
                alloc->tail  = new_source;
                continue;
            }
        }

        /* The allocation cannot be satisfied */
        return NULL;
    }
}

void
mem_allocator_reset
(
    struct mem_allocator_t *alloc
)
{
    if (alloc != NULL && alloc->head != NULL) {
        /* Keep the head chunk; release everything else */
        mem_chunk_release(alloc->head->next, alloc->allocator_flags);
        alloc->head->next_offset = 0;
        alloc->head->next =  NULL;
        alloc->tail = alloc->head;
        alloc->allocator_version++;
    }
}

void
mem_allocator_reset_to_marker
(
    struct mem_allocator_t *alloc, 
    struct mem_marker_t   *marker
)
{
    if (marker != NULL && marker->tag == alloc->allocator_tag) {
        /* Roll back the allocator state */
        mem_chunk_release(marker->chunk->next, alloc->allocator_flags);
        marker->chunk->next        = NULL;
        marker->chunk->next_offset = marker->offset;
        alloc->tail                = marker->chunk;
        alloc->allocator_version   = marker->version;
        return;
    } 
    if (marker == NULL) {
        /* Reset the allocator completely */
        mem_allocator_reset(alloc);
        return;
    }
    if (marker->tag != alloc->allocator_tag) {
        assert(marker->tag == alloc->allocator_tag && "Marker passed to allocator other than that from which it was obtained");
    }
}

void*
mem_allocator_reserve
(
    struct mem_allocator_t *alloc, 
    struct mem_reservation_t *res,
    size_t          reserve_bytes,
    size_t              alignment
)
{
    assert(alloc != NULL && "Expected non-null alloc argument");
    assert(res   != NULL && "Expected non-null reservation argument");
    for ( ; ; ) {
        mem_chunk_t      *source = alloc->tail;
        uint8_t    *base_address = source->memory_start + source->next_offset;
        uintptr_t   address_uint =(uintptr_t) base_address;
        uint8_t *aligned_address =(uint8_t *) mem_align_up(address_uint, alignment);
        size_t       align_bytes =(size_t   )(aligned_address - base_address);
        size_t       alloc_bytes = reserve_bytes + align_bytes;
        size_t        new_offset = source->next_offset + alloc_bytes;

        if (new_offset <= source->maximum_offset) {
            /* Populate the reservation record */
            res->chunk  = source;
            res->offset = source->next_offset;
            res->length = alloc_bytes;
            res->tag    = alloc->allocator_tag;
            res->version= alloc->allocator_version + 1;
            /* Bump the offset to account for the reserved space */
            source->next_offset = new_offset;
            /* Bump the high watermark if necessary */
            if (new_offset > alloc->high_watermark) {
                alloc->high_watermark = new_offset;
            }
            alloc->allocator_version++;
            return aligned_address;
        }

        /* Else, the reservation exceeds the capacity of the chunk */
        if (alloc->allocator_flags & MEM_ALLOCATION_FLAG_GROWABLE) {
            /* Allocate and append a new chunk from which the reservation will be satisfied */
            mem_chunk_t *new_source  = NULL;
            size_t       chunk_size  = alloc->chunk_size;
            size_t       chunk_align = DEFAULT_ALIGNMENT;
            if (chunk_align < alignment) {
                chunk_align = alignment;
            }
            if (chunk_size < reserve_bytes) {
                chunk_size = reserve_bytes  + alignment;
            }
            if ((new_source  = mem_chunk_allocate(chunk_size, alloc->guard_size, chunk_align, alloc->allocator_flags, alloc->access_flags)) != NULL) {
                source->next = new_source;
                alloc->tail  = new_source;
                continue;
            }
        }

        /* The reservation cannot be satisfied */
        res->chunk   = NULL;
        res->offset  = 0;
        res->length  = 0;
        res->tag     = 0;
        res->version = 0;
        return NULL;
    }
}

void*
mem_allocator_commit
(
    struct mem_allocator_t *alloc, 
    struct mem_reservation_t *res,
    void                   *start,
    size_t             bytes_used
)
{
    assert(alloc != NULL && "Expected non-null alloc argument");
    assert(res   != NULL && "Expected non-null reservation argument");
    assert(res->tag == alloc->allocator_tag && "Reservation passed to wrong allocator");
    assert(res->length >= bytes_used && "Number of bytes used exceeds reservation size");
    if (res->tag == alloc->allocator_tag) {
        if (start != NULL && bytes_used != 0 && res->version == alloc->allocator_version && bytes_used <= res->length) {
            /* Part of the reserved space can be potentially be freed up */
            mem_chunk_t      *source = res->chunk;
            uint8_t     *res_address =(uint8_t*) start;
            uint8_t    *base_address = source->memory_start  + res->offset;
            size_t         pad_bytes =(size_t  )(res_address - base_address);
            size_t        new_offset = res->offset + pad_bytes + bytes_used;
            assert(new_offset  <= source->maximum_offset);
            if (new_offset <= source->maximum_offset) {
                source->next_offset = new_offset;
            }
        } else if (bytes_used == 0 && res->version == alloc->allocator_version) {
            /* Cancel the reservation, free the entire reserved space */
            mem_chunk_t *source = res->chunk;
            assert(res->offset <= source->maximum_offset);
            if (res->offset <= source->maximum_offset) {
                source->next_offset = res->offset;
            }
        } /* Else, an allocation was made prior to the reservation being committed */
    }
    return start;
}

void
mem_allocator_cancel_reservation
(
    struct mem_allocator_t *alloc,
    struct mem_reservation_t *res,
    void                   *start
)
{
    (void) mem_allocator_commit(alloc, res, start, 0);
}
