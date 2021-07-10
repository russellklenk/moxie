/**
 * memory.h: Defines custom memory allocation routines and abstracts 
 * away platform differences in memory allocations. Two types of simple
 * allocators are exposed; a non-growable linear allocator and a growable
 * arena allocator.
 */
#ifndef __MOXIE_CORE_MEMORY_H__
#define __MOXIE_CORE_MEMORY_H__

#pragma once

#ifndef MOXIE_CORE_NO_INCLUDES
#   include <stddef.h>
#   include <stdint.h>
#   include <stdbool.h>
#endif
/**
 * Return the byte alignment of a given type.
 */
#ifdef _MSC_VER
#   define MEM_PLATFORM_alignof_type(_type)                                    \
    __alignof(_type)
#else
#   define MEM_PLATFORM_alignof_type(_type)                                    \
    __alignof__(_type)
#endif

#ifndef MEMORY_UTILITY_MACROS
#   define MEMORY_UTILITY_MACROS
/**
 * Specify a size as a number of Kilobytes (1KB = 1024 bytes).
 * The calculation results in a uint32_t specifying the number of bytes.
 */
#   define SIZE_KB(_kilobytes)                                                 \
    (1024U * (_kilobytes))

/**
 * Specify a size as a number of Megabytes (1MB = 1024 Kilobytes).
 * The calculation results in a uint64_t specifying the number of bytes.
 */
#   define SIZE_MB(_megabytes)                                                 \
    (1024ULL * 1024ULL * (_megabytes))

/**
 * Specify a size as a number of Gigabytes (1GB = 1024 Megabytes).
 * The calculation results in a uint64_t specifying the number of bytes.
 */
#   define SIZE_GB(_gigabytes)                                                 \
    (1024ULL * 1024ULL * 1024ULL * (_gigabytes))

/**
 * Calculate the size of a type, in bytes.
 */
#   define mem_size_of(_type)                                                  \
    ((size_t) sizeof(_type))

/**
 * Calculate the alignment of a type, in bytes.
 */
#   define mem_align_of(_type)                                                 \
    ((size_t) MEM_PLATFORM_alignof_type(_type))

/**
 * Calculate the number of elements in a fixed-length array. For example:
 * float my_array[4];
 * size_t const  num = mem_count_of(my_array); // num = 4
 */
#   define mem_count_of(_fixed_len_array)                                      \
    ((size_t)(mem_size_of(_fixed_len_array)/mem_size_of(_fixed_len_array[0])))

/**
 * Calculate the byte offset of a field within a structure, accounting for padding.
 */
#   define mem_offset_of(_type, _field)                                        \
    ((size_t) offsetof(_type, _field))

/**
 * Round a quantity up to an even multiple of a given power-of-two alignment.
 * If the given quantity is already an even multiple of alignment, it is unchanged.
 */
#   define mem_align_up(_quantity, _alignment)                                 \
    (((_quantity) + ((_alignment)-1)) & ~((_alignment)-1))

/**
 * For a given address, compute the address aligned for a particular type.
 * If the address is already aligned correctly, it is returned unchanged.
 */
#   define mem_align_for(_address, _type)                                      \
    ((void*)((((uintptr_t)(_address)) + (mem_align_of(_type)-1)) & ~(mem_align_of(_type)-1)))

/**
 * For a given address, compute the address aligned to a given power-of-two alignment.
 * If the address is already aligned correctly, it is returned unchanged.
 */
#   define mem_align_raw(_address, _alignment)                                 \
    ((void*)((((uintptr_t)(_address)) + ((_alignment)-1)) & ~((_alignment)-1)))

/**
 * Determine whether a given value is an even multiple of a power-of-two.
 */
#   define mem_aligned_to(_value, _alignment)                                  \
    ((((uintptr_t)(_value)) & ((_alignment)-1)) == 0)

/**
 * Calculate the worst-case number of bytes needed to hold an instance of a given type with the correct alignment.
 */
#   define mem_max_allocation_size_for_type(_type)                             \
    (mem_size_of(_type) + (mem_align_of(_type)-1))

/**
 * Calculate the worst-case number of bytes needed to hold a fixed-size array of instances of the given type with the correct alignment.
 */
#   define mem_max_allocation_size_for_array(_type, _count)                    \
    ((mem_size_of(_type) * (_count)) + (mem_align_of(_type)-1))

/**
 * Construct a tag value given four ASCII bytes.
 */
#   define mem_tag(_a, _b, _c, _d)                                             \
    (((mem_tag_t)(uint8_t)(_a)      ) |                                        \
     ((mem_tag_t)(uint8_t)(_b) <<  8) |                                        \
     ((mem_tag_t)(uint8_t)(_c) << 16) |                                        \
     ((mem_tag_t)(uint8_t)(_d) << 24))

/**
 * Swap the bytes in a two-byte value.
 */
#   define mem_byte_swap2(_v)                                                  \
    ( (((_v) >> 8) & 0x00ff) |                                                 \
      (((_v) << 8) & 0xff00) )

/**
 * Swap the bytes in a four-byte value.
 */
#   define mem_byte_swap4(_v)                                                  \
    ( (((_v) >> 24) & 0x000000ff) |                                            \
      (((_v) >>  8) & 0x0000ff00) |                                            \
      (((_v) <<  8) & 0x00ff0000) |                                            \
      (((_v) << 24) & 0xff000000) )

/**
 * Swap the bytes in an eight-byte value.
 */
#   define mem_byte_swap8(_v)                                                  \
    ( (((_v) >> 56) & 0x00000000000000ffULL) |                                 \
      (((_v) >> 40) & 0x000000000000ff00ULL) |                                 \
      (((_v) >> 24) & 0x0000000000ff0000ULL) |                                 \
      (((_v) >>  8) & 0x00000000ff000000ULL) |                                 \
      (((_v) <<  8) & 0x000000ff00000000ULL) |                                 \
      (((_v) << 24) & 0x0000ff0000000000ULL) |                                 \
      (((_v) << 40) & 0x00ff000000000000ULL) |                                 \
      (((_v) << 56) & 0xff00000000000000ULL) )
#endif /* MEMORY_UTILITY_MACROS */

#ifndef MEMORY_ALLOCATOR_ACCESSORS
#   define MEMORY_ALLOCATOR_ACCESSORS
/**
 * Retrieve a nul-terminated UTF-8 or ASCII string specifying the name of the memory allocator.
 */
#   define mem_allocator_name(_alloc)                                          \
    (_alloc)->allocator_name

/**
 * Retrieve the tag value (a four character code) associated with the memory allocator.
 */
#   define mem_allocator_tag(_arena)                                           \
    (_alloc)->allocator_tag

/**
 * Retrieve the flags describing the attributes of the memory allocator.
 * These are one or more bitwise OR'd values of the mem_allocator_flags_e enumeration.
 */
#   define mem_allocator_flags(_arena)                                         \
    (_alloc)->allocator_flags

/**
 * Retrieve the high watermark value for the memory allocator, representing the maximum number of bytes ever allocated.
 */
#   define mem_allocator_watermark(_alloc)                                     \
    (_alloc)->high_watermark

/**
 * Retrieve the address of the first addressable byte of a chunk of memory.
 */
#   define mem_chunk_begin(_chunk)                                             \
    ((uint8_t*)(_chunk)->memory_start)

/**
 * Retrieve the address of one byte past the last allocated byte of memory within a chunk.
 */
#   define mem_chunk_end(_chunk)                                               \
    (((uint8_t*)(_chunk)->memory_start) + (_chunk)->next_offset)

/**
 * Retrieve the address of one byte past the last addressable byte of memory in a chunk.
 */
#   define mem_chunk_limit(_chunk)                                             \
    (((uint8_t*)(_chunk)->memory_start) + (_chunk)->maximum_offset)

/**
 * Retrieve the number of bytes currently allocated within a memory chunk.
 */
#   define mem_chunk_bytes_used(_chunk)                                        \
    ((size_t)(_chunk)->next_offset)

/**
 * Retrieve the number of bytes that are not allocated within a memory chunk.
 */
#   define mem_chunk_bytes_free(_chunk)                                        \
    ((size_t)(_chunk)->maximum_offset - (_chunk)->next_offset)

/**
 * Retrieve the total capacity of a memory chunk.
 */
#   define mem_chunk_bytes_total(_chunk)                                       \
    ((size_t)(_chunk)->maximum_offset)
#endif /* MEMORY_ALLOCATOR_ACCESSORS */

#ifndef MEMORY_ALLOCATION_HELPERS
#   define MEMORY_ALLOCATION_HELPERS
/**
 * Allocate a single instance of a given type from the process heap.
 * @param _type The typename of the value to allocator storage for, for example, int.
 * @return A pointer to the allocated instance, or NULL if the request could not be satisfied.
 */
#   define mem_heap_alloc_type(_type)                                          \
    ((_type*) mem_heap_allocate(mem_size_of(_type), mem_align_of(_type)))

/**
 * Allocate a contiguous array of instances of a given type from the process heap.
 * @param _type The typename of the value to allocator storage for, for example, int.
 * @param _count The number of instances to allocate.
 * @return A pointer to the first instance in the array, or NULL if the request could not be satisfied.
 */
#   define mem_heap_alloc_array(_type, _count)                                 \
    ((_type*) mem_heap_allocate(mem_size_of(_type)*(_count), mem_align_of(_type)))

/**
 * Free a memory block allocated from the process heap.
 * @param _address The address of the memory block to free.
 */
#   define mem_heap_free(_address)                                             \
    (void) mem_heap_release((_address))

/**
 * Allocate a single instance of a given type from a memory allocator.
 * @param _alloc The mem_allocator_t from which the storage will be allocated.
 * @param _type The typename of the value to allocator storage for, for example, int.
 * @return A pointer to the allocated instance, or NULL if the request could not be satisfied.
 */
#   define mem_allocate_type(_alloc, _type)                                    \
    ((_type*) mem_allocator_alloc(_alloc, mem_size_of(_type), mem_align_of(_type)))

/**
 * Allocate a contiguous array of instances of a given type from a memory allocator.
 * @param _alloc The mem_allocator_t from which the storage will be allocated.
 * @param _type The typename of the value to allocator storage for, for example, int.
 * @param _count The number of instances to allocate.
 * @return A pointer to the first instance in the array, or NULL if the request could not be satisfied.
 */
#   define mem_allocate_array(_alloc, _type, _count)                           \
    ((_type*) mem_allocator_alloc(_alloc, mem_size_of(_type)*(_count), mem_align_of(_type)))
#endif /* MEMORY_ALLOCATION_HELPERS */

typedef uint32_t mem_tag_t;                                                    /* Hold a memory tag (four character code) value. */

typedef enum  mem_allocation_flags_e {                                         /* Flags that can be bitwise OR'd to describe the attributes of a raw memory allocation. */
    MEM_ALLOCATION_FLAGS_NONE                                   = (0UL <<  0), /* No flags are specified. */
   
    MEM_ALLOCATION_FLAG_LOCAL                                   = (1UL <<  1), /* The memory is only visible to the local process. */
    MEM_ALLOCATION_FLAG_SHARED                                  = (1UL <<  2), /* The memory is visible to other processes via a shared mapping. */

    MEM_ALLOCATION_FLAG_HEAP                                    = (1UL <<  3), /* The memory comes from the process heap. */
    MEM_ALLOCATION_FLAG_VIRTUAL                                 = (1UL <<  4), /* The memory comes from the system virtual memory manager. */
    MEM_ALLOCATION_FLAG_EXTERNAL                                = (1UL <<  5), /* The memory is externally-owned. */

    MEM_ALLOCATION_FLAG_GROWABLE                                = (1UL << 31)  /* The memory chunk is growable. */
} mem_allocation_flags_e;

typedef enum  mem_access_flags_e {                                             /* Flags that can be bitwise OR'd to describe the access types for a memory block. */
    MEM_ACCESS_FLAGS_NONE                                       = (0UL <<  0), /* The memory does not support reading or writing. */
    MEM_ACCESS_FLAG_READ                                        = (1UL <<  0), /* The memory supports reading. */
    MEM_ACCESS_FLAG_WRITE                                       = (1UL <<  1), /* The memory supports writing. */
    MEM_ACCESS_FLAG_RDWR                                        =              /* The memory supports reading and writing. */
        MEM_ACCESS_FLAG_READ  | 
        MEM_ACCESS_FLAG_WRITE
} mem_access_flags_e;

typedef struct mem_chunk_t {                                                   /* Represents a single chunk of memory, corresponding to a single underlying allocation. */
    struct mem_chunk_t    *next;                                               /* A pointer to the next chunk in the chain, or NULL. */
    uint8_t       *memory_start;                                               /* A pointer to the first addressable byte of the allocation. */
    uint64_t        next_offset;                                               /* The byte offset of the first unused byte within the chunk. */
    uint64_t     maximum_offset;                                               /* The total size of the chunk, in bytes. */
} mem_chunk_t;

typedef struct mem_allocator_t {                                               /* The state associated with a linear memory allocator. */
    struct mem_chunk_t    *tail;                                               /* A pointer to the chunk being allocated from. */
    struct mem_chunk_t    *head;                                               /* A pointer to the chunk used to initialize the allocator. May be the same as tail. */
    char const  *allocator_name;                                               /* A pointer to a nul-terminated, UTF-8 encoded string literal specifying a name for the allocator. */
    uint64_t         chunk_size;                                               /* The actual capacity of each chunk, in bytes. All chunks have the same capacity. */
    uint64_t     high_watermark;                                               /* The maximum value of the total number of bytes allocated from this allocator across the allocator lifetime. */
    uint32_t  allocator_version;                                               /* An integer value incremented each time an allocation is successfully made from the allocator. */
    uint32_t    allocator_flags;                                               /* A combination of one or more bitwise-OR'd values of the mem_allocation_flags_e enumeration. */
    uint32_t       access_flags;                                               /* A combination of one or more bitwise-OR'd values of the mem_access_flags_e enumeration. */
    uint32_t         guard_size;                                               /* The number of bytes allocated at the end of each chunk and acting as a no-mans land. */
    uint32_t          page_size;                                               /* The number of bytes in a single virtual memory page. */
    mem_tag_t     allocator_tag;                                               /* A four-character code value associated with the allocator and used for debugging purposes. */
} mem_allocator_t;

typedef struct mem_reservation_t {                                             /* Stores the state associated with a memory reservation. */
    struct mem_chunk_t   *chunk;                                               /* The tail chunk at the point in time when the reservation was made. */
    uint64_t             offset;                                               /* The byte offset within the chunk at the time of the reservation. */
    uint64_t             length;                                               /* The length of the reservation, in bytes. */
    mem_tag_t               tag;                                               /* The tag of the allocator from which the reservation was made. */
    uint32_t            version;                                               /* The version number of the allocator at the time the reservation was made. */
} mem_reservation_t;

typedef struct mem_marker_t {                                                  /* Stores the state associated with a linear allocator at a given point in time.*/
    struct mem_chunk_t   *chunk;                                               /* The tail chunk at the point in time when the marker was obtained. */
    uint64_t             offset;                                               /* The byte offset within the chunk at the point in time when the marker was obtained. */
    mem_tag_t               tag;                                               /* The tag of the allocator from which the marker was obtained. */
    uint32_t            version;                                               /* The version number of the allocator at the time the marker was obtained. */
} mem_marker_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copy bytes between non-overlapping memory regions.
 * The implementation of this function is platform-specific.
 * @param dst Pointer to the first byte of the memory region to write to.
 * @param src Pointer to the first byte of the memory region to read from.
 * @param num The number of bytes to copy.
 */
extern void
mem_copy
(
    void       * __restrict dst, 
    void const * __restrict src, 
    size_t                  num
);

/**
 * Copy bytes between possibly overlapping memory regions.
 * The implementation of this function is platform-specific.
 * @param dst Pointer to the first byte of the memory region to write to.
 * @param src Pointer to the first byte of the memory region to read from.
 * @param num The number of bytes to copy.
 */
extern void
mem_move
(
    void       *dst, 
    void const *src, 
    size_t      num
);

/**
 * Zero a memory region.
 * Do not use this function to zero security-critical data.
 * The implementation of this function is platform-specific.
 * @param mem Pointer to the first byte of the memory region to zero.
 * @param num The number of bytes to which zeroes will be written.
 */
extern void
mem_zero
(
    void  *mem, 
    size_t num
);

/**
 * Compares bytes in two memory regions.
 * Do not use this function to compare security-critical data.
 * The implementation of this function is platform-specific.
 * @param rgn1 Pointer to the start of the first memory region.
 * @param rgn2 Pointer to the start of the second memory region.
 * @param num The maximum number of bytes to read from rgn1 and rgn2.
 * @return zero of num bytes are equal in rgn1 and rgn2, less than zero if a non matching byte is found in rgn1 that is smaller than the corresponding byte in rgn2, or greater than zero if a non matching byte is found in rgn1 that is larger then the corresponding byte in rgn2.
 */
extern int
mem_compare
(
    void const *rgn1, 
    void const *rgn2, 
    size_t       num
);

/**
 * Retrieve the size of a single page in the system virtual memory manager.
 * The implementation of this function is platform-specific.
 * @return The number of bytes in a virtual memory page.
 */
extern size_t
mem_page_size
(
    void
);

/**
 * Allocate memory from the system virtual memory manager.
 * This both reserves and commits a block of process address space.
 * The implementation of this function is platform-specific.
 * @param min_size_bytes The minimum number of bytes to commit. The actual number will always be an even multiple of the system page size.
 * @param access_flags One or more bitwise OR'd values of the mem_access_flags_e enumeration specifying the allowable access types for the address space region.
 * @param o_actual_size On return, the actual number of of bytes committed will be stored here.
 * @return A pointer to the start of the committed region, or NULL of the allocation failed.
 */
extern void*
mem_vmm_allocate
(
    size_t min_size_bytes, 
    uint32_t access_flags, 
    size_t *o_actual_size
);

/**
 * Change the access protections for a region of process address space.
 * The implementation of this function is platform-specific.
 * @param address The address representing the start of the region whose access flags will be updated.
 * @param region_size The size of the memory region, in bytes. Note that access protections are updated at page granularity.
 * @param access_flags One or more bitwise OR'd values of the mem_access_flags_e enumeration specifying the allowable access types for the address space region.
 * @return true if the access protections were updated, or false if an error occurred.
 */
extern bool
mem_vmm_protect
(
    void         *address, 
    size_t    region_size,
    uint32_t access_flags 
);

/**
 * Decommit and release a range of process address space.
 * The implementation of this function is platform-specific.
 * @param address The address representing the start of the range to decommit and release.
 * @param region_size The number of bytes to decommit and release, starting from the given address.
 * @return true if the region was released, or false if an error occurred.
 */
extern bool
mem_vmm_release
(
    void      *address, 
    size_t region_size
);

/**
 * Allocate memory from the process heap.
 * The implementation of this function is platform-specific.
 * @param min_size_bytes The minimum required size of the returned memory block.
 * @param alignment A non-zero power of two specifying the required alignment of the returned memory block, in bytes.
 * @return A pointer to the start of the memory block, or NULL if allocation failed.
 */
extern void*
mem_heap_allocate
(
    size_t min_size_bytes, 
    size_t      alignment
);

/**
 * Release a heap memory block back to the process heap from which it was allocated.
 * The implementation of this function is platform-specific.
 * @param address The address of the memory block to release, returned by a prior call to mem_heap_allocate.
 */
extern void
mem_heap_release
(
    void *address
);

/**
 * Initialize a memory chunk instance using externally-allocated memory.
 * @param chunk The memory chunk instance to initialize.
 * @param memory The underlying memory block backing the chunk.
 * @param length The length of the memory block, in bytes.
 */
extern void
mem_chunk_init
(
    struct mem_chunk_t *chunk, 
    void              *memory, 
    size_t             length
);

/**
 * Allocate and initialize a heap or virtual memory-backed memory chunk.
 * @param chunk_size The size of the memory block to allocate, in bytes.
 * @param guard_size The number of bytes of no-mans land space to allocate at the end of the chunk. This value may be zero.
 * @param alignment The desired alignment of the allocated memory block, in bytes.
 * @param flags One or more bitwise OR'd values of the mem_allocation_flags_e enumeration describing the traits of the memory region.
 * @param access One or more bitwise OR'd values of the mem_access_flags_e enumeration describing the allowable access for the memory region.
 * @return A pointer to the chunk argument, or NULL if the allocation fails.
 */
extern struct mem_chunk_t*
mem_chunk_allocate
(
    size_t chunk_size,
    size_t guard_size,
    size_t  alignment,
    uint32_t    flags, 
    uint32_t   access
);

/**
 * Free memory associated with a memory chunk. All subsequent chunks in the chain are also released.
 * @param chunk The head of the memory chunk chain to release.
 * @param flags One or more bitwise OR'd values of the mem_allocation_flags_e enumeration describing the traits of the memory region.
 */
extern void
mem_chunk_release
(
    struct mem_chunk_t *chunk, 
    uint32_t            flags
);

/**
 * Allocate storage and use it to initialize a memory allocator.
 * @param alloc The memory allocator structure to initialize.
 * @param chunk_size The size of the initial memory allocation, in bytes.
 * @param guard_size The size of the guard region, in bytes. This value may be zero.
 * @param alignment The desired alignment of the allocated region, in bytes. Specify 0 to use the default alignment (16 bytes); otherwise, this must be a power-of-two.
 * @param flags A combination of one or more bitwise OR'd values from the mem_allocation_flags_e enumeration.
 * @param access A combination of one or more bitwise OR'd values from the mem_access_flags_e enumeration.
 * @param name A nul-terminated, UTF-8 encoded string specifying a name for the allocator.
 * @param tag A four byte value used to identify the allocator in memory.
 * @return A pointer to the input alloc structure, or NULL if an error occurred.
 */
extern struct mem_allocator_t*
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
);

/**
 * Initialize a memory allocator with externally-managed storage.
 * @param alloc The memory allocator structure to initialize.
 * @param memory The externally-managed memory block to sub-allocate from.
 * @param length The maximum number of bytes to return from the externally-managed memory block.
 * @param base_flags A combination of one or more bitwise OR'd values from the mem_allocation_flags_e enumeration.
 * @param access A combination of one or more bitwise OR'd values from the mem_access_flags_e enumeration.
 * @param name A nul-terminated, UTF-8 encoded string specifying a name for the allocator.
 * @param tag A four byte value used to identify the allocator in memory.
 * @return A pointer to the input alloc structure, or NULL if an error occurred.
 */
extern struct mem_allocator_t*
mem_allocator_create_with_memory
(
    struct mem_allocator_t *alloc,
    void                  *memory,
    size_t                 length,
    uint32_t           base_flags,
    uint32_t               access,
    char const              *name,
    mem_tag_t                 tag
);

/**
 * Initialize a memory allocator with memory taken from another allocator.
 * @param alloc The memory allocator structure to initialize.
 * @param parent The memory allocator from which storage will be allocated.
 * @param length The number of bytes to allocate from the parent allocator and use to initialize the sub-allocator.
 * @param name A nul-terminated, UTF-8 encoded string specifying a name for the allocator.
 * @param tag A four byte value used to identify the allocator in memory.
 * @return A pointer to the input alloc structure, or NULL if an error occurred.
 */
extern struct mem_allocator_t*
mem_allocator_create_suballocator
(
    struct mem_allocator_t  *alloc, 
    struct mem_allocator_t *parent,
    size_t                  length,
    char const               *name,
    mem_tag_t                  tag
);

/**
 * Free resources associated with a memory allocator instance.
 * For allocators with externally-managed storage, the underlying storage is not freed here.
 * @param alloc The memory allocator to delete.
 */
extern void
mem_allocator_delete
(
    struct mem_allocator_t *alloc
);

/**
 * Obtain a marker representing the state of the allocator at the time of the call.
 * @param alloc The memory allocator for which the marker should be returned.
 */
extern struct mem_marker_t
mem_allocator_mark
(
    struct mem_allocator_t *alloc
);

/**
 * Allocate memory.
 * @param alloc The memory allocator from which the allocation will be obtained.
 * @param length The minimum number of bytes to allocate.
 * @param alignment The required alignment of the returned address, in bytes. This must be a power of two greater than zero.
 * @return A pointer to the start of the memory block, or NULL if the request could not be satisfied.
 */
extern void*
mem_allocator_alloc
(
    struct mem_allocator_t *alloc, 
    size_t                 length,
    size_t              alignment
);

/**
 * Reset the state of a memory allocator, invalidating all allocations.
 * @param alloc The memory allocator to reset.
 */
extern void
mem_allocator_reset
(
    struct mem_allocator_t *alloc
);

/**
 * Roll back the state of a memory allocator, invalidating all allocations made past a given point in time.
 * @param alloc The memory allocator to reset.
 * @param marker A previously obtained marker representing the state of the allocator at a point in time. If this value is NULL, the call is equivalent to mem_allocator_reset.
 */
extern void
mem_allocator_reset_to_marker
(
    struct mem_allocator_t *alloc, 
    struct mem_marker_t   *marker
);

/**
 * Reserve a contiguous block of space, indicating that up to a maximum number of bytes will be allocated.
 * The mem_allocator_commit function can be used to commit the portion of the reservation actually needed.
 * This is useful for writing variable-length data with a fixed maximum size to a memory block.
 * @param alloc The memory allocator from which the reservation should be made.
 * @param res The reservation record to populate, which can be passed to mem_allocator_commit or mem_allocator_cancel.
 * @param reserve_bytes The number of bytes to reserve for use.
 * @param alignment The required alignment of the returned address, in bytes. This must be a power of two greater than zero.
 * @return A pointer to the start of the reserved region, or NULL if the reservation request cannot be satisfied.
 */
extern void*
mem_allocator_reserve
(
    struct mem_allocator_t *alloc, 
    struct mem_reservation_t *res,
    size_t          reserve_bytes,
    size_t              alignment
);

/**
 * Commit a prior reservation. If no allocations have been made from the allocator since the reservation was made, the unused portion of the reserved space will be freed.
 * @param alloc The memory allocator from which the reservation was made.
 * @param res The reservation record populated by mem_allocator_reserve for the reservation.
 * @param start The address returned by mem_allocator_reserve for the reservation.
 * @param bytes_used The number of bytes actually used from the reservation. If this value is 0, the reservation is cancelled.
 * @return The address of the start of the reservation.
 */
extern void*
mem_allocator_commit
(
    struct mem_allocator_t *alloc, 
    struct mem_reservation_t *res,
    void                   *start,
    size_t             bytes_used
);

/**
 * Cancel a prior reservation. If no allocations have been made from the allocator since the reservation was made, the reserved space is freed.
 * @param alloc The memory allocator from which the reservation was made.
 * @param res The reservation record populated by mem_allocator_reserve for the reservation.
 * @param start The address returned by mem_allocator_reserve for the reservation.
 */
extern void
mem_allocator_cancel_reservation
(
    struct mem_allocator_t *alloc,
    struct mem_reservation_t *res,
    void                   *start
);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* __MOXIE_CORE_MEMORY_H__ */
