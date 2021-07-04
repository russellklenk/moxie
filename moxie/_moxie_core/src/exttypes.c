#include "exttypes.h"

#define MEM_TAG_BUFFER_SIZE                                                    5
#define STRINGIZE_(x)                                                          #x
#define STRINGIZE(x)                                                           STRINGIZE_(x)
#define TYPE_NAME(type)                                                        STRINGIZE(EXTENSION_NAME) "." #type
#define Py_EmptyString                                                         ""
#define Py_Assign(_x)                                                         (Py_XINCREF(_x), (_x))
#define Py_IsNone(_x)                                                        ((_x) == NULL || (PyObject*)(_x) == Py_None)
#define Py_IsNotNone(_x)                                                     ((_x) != NULL && (PyObject*)(_x) != Py_None)

typedef struct mem_allocator_stats_t {                                         /* Information about a memory allocator instance. */
    size_t               watermark;                                            /* The high watermark value of the allocator. */
    size_t              bytes_free;                                            /* The number of bytes unused within the tail chunk. */
    size_t              bytes_used;                                            /* The total number of bytes allocated by the application. */
    size_t              bytes_lost;                                            /* The total number of bytes lost to internal fragmentation. */
    size_t             bytes_guard;                                            /* The total number of bytes allocated to guard pages. */
    size_t             bytes_total;                                            /* The total number of bytes reserved by the allocator. */
    size_t             chunk_count;                                            /* The total number of chunks in the arena. */
} mem_allocator_stats_t;

/**
 * Convert a mem_tag_t (four character code) to a nul-terminated ASCII string.
 * @param dst The destination buffer, which must be at least 5 bytes long.
 * @param tag The tag to format.
 * @return A pointer to the start of the destination buffer.
 */
static char*
mem_tag_to_ascii
(
    char     *dst,
    mem_tag_t tag
)
{
    char *ptr = &dst[0];
    *(uint32_t*) ptr = tag;
    dst[4] = '\0';
    return dst;
}

/**
 * Compute memory usage statistics for a given memory allocator.
 * @param stats The structure to populate.
 * @param alloc The allocator to inspect.
 */
static void
mem_allocator_stats
(
    struct mem_allocator_stats_t *stats,
    struct mem_allocator_t       *alloc
)
{
    struct mem_chunk_t *iter = NULL;
    size_t             nfree = 0;
    size_t             nused = 0;
    size_t             nlost = 0;
    size_t             guard = 0;
    size_t             total = 0;
    size_t            chunks = 0;

    if (stats == NULL) {
        return;
    }
    if (alloc == NULL) {
        stats->watermark   = 0;
        stats->bytes_free  = 0;
        stats->bytes_used  = 0;
        stats->bytes_lost  = 0;
        stats->bytes_guard = 0;
        stats->bytes_total = 0;
        stats->chunk_count = 0;
        return;
    }
    iter = alloc->head;
    while (iter != NULL) {
        chunks  += 1;
        nused   += mem_chunk_bytes_used(iter);
        guard   += alloc->guard_size;
        total   += mem_chunk_bytes_total(iter);
        if (iter  != alloc->tail) {
            nlost += mem_chunk_bytes_free(iter);
            iter   = iter->next;
        } else {
            nfree += mem_chunk_bytes_free(iter);
            break;
        }
    }
    stats->watermark   = alloc->high_watermark;
    stats->bytes_free  = nfree;
    stats->bytes_used  = nused;
    stats->bytes_lost  = nlost;
    stats->bytes_guard = guard;
    stats->bytes_total = total;
    stats->chunk_count = chunks;
}

/**
 * Free resources associated with a `MemoryMarker` instance.
 * @param self The object to delete.
 */
static void
MemoryMarker_dealloc
(
    PyMoxieMemoryMarker *self
)
{
    if (self != NULL) {
        Py_XDECREF(self->allocator_name_str);
        Py_XDECREF(self->allocator_tag_int);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

/**
 * Allocate memory for and default-initialize the fields of a `MemoryMarker` instance.
 * @param type The `MemoryMarkerType` instance.
 * @param args The list of arguments supplied to the `__new__` call.
 * @param kwargs The table of keyword arguments supplied to the `__new__` call.
 * @return A pointer to the new instance, or `None` if the instance initialization failed.
 */
static PyObject*
MemoryMarker_new
(
    PyTypeObject *type, 
    PyObject     *args, 
    PyObject   *kwargs
)
{
    PyMoxieMemoryMarker  *self = NULL;

    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);

    if ((self = (PyMoxieMemoryMarker*) type->tp_alloc(type, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    self->allocator_name_str = Py_Assign(Py_None);
    self->allocator_tag_int  = Py_Assign(Py_None);
    return (PyObject*) self;

cleanup_and_fail:
    MemoryMarker_dealloc(self);
    Py_RETURN_NONE;
}

/**
 * Obtain a `str` representation of a `MemoryMarker` instance.
 * @param self The `MemoryMarker` instance.
 * @return A `str` of the form `ABCD@123abc [2]` where `ABCD` is the FourCC tag of the source allocator, `123abc` is the hexadecimal byte offset of the start of the allocation, and `[2]` is the version of the allocator at the time the marker was obtained.
 */
static PyObject*
MemoryMarker_str
(
    PyMoxieMemoryMarker *self
)
{
    uint8_t *addr = self->marker.chunk->memory_start + self->marker.offset;
    char tag[MEM_TAG_BUFFER_SIZE];
    return PyUnicode_FromFormat("%s %p [%llu] v%u (%S)", mem_tag_to_ascii(tag, self->marker.tag), addr, self->marker.offset, self->marker.version, self->allocator_name_str);
}

/**
 * Obtain a `str` representation of a `MemoryMarker` instance.
 * @param self The `MemoryMarker` instance.
 * @return A `str` describing the marker.
 */
static PyObject*
MemoryMarker_repr
(
    PyMoxieMemoryMarker *self
)
{
    uint8_t *addr = self->marker.chunk->memory_start + self->marker.offset;
    char tag[MEM_TAG_BUFFER_SIZE];
    return PyUnicode_FromFormat("MemoryMarker(allocator=%S, tag=%s, version=%u, offset=%llu, address=%p)", self->allocator_name_str, mem_tag_to_ascii(tag, self->marker.tag), self->marker.version, self->marker.offset, addr);
}

/**
 * Free resources associated with a `MemoryAllocation` instance.
 * @param self The object to delete.
 */
static void
MemoryAllocation_dealloc
(
    PyMoxieMemoryAllocation *self
)
{
    if (self != NULL) {
        Py_XDECREF(self->readonly_bool);
        Py_XDECREF(self->allocator_tag_int);
        Py_XDECREF(self->allocator_name_str);
        Py_XDECREF(self->length_int);
        Py_XDECREF(self->base_address_int);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

/**
 * Allocate memory for and default-initialize the fields of a `MemoryAllocation` instance.
 * @param type The `MemoryAllocationType` instance.
 * @param args The list of arguments supplied to the `__new__` call.
 * @param kwargs The table of keyword arguments supplied to the `__new__` call.
 * @return A pointer to the new instance, or `None` if the instance initialization failed.
 */
static PyObject*
MemoryAllocation_new
(
    PyTypeObject *type, 
    PyObject     *args, 
    PyObject   *kwargs
)
{
    PyMoxieMemoryAllocation *self = NULL;

    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);

    if ((self = (PyMoxieMemoryAllocation*) type->tp_alloc(type, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    self->base_address_int    = Py_Assign(Py_None);
    self->length_int          = Py_Assign(Py_None);
    self->allocator_name_str  = Py_Assign(Py_None);
    self->allocator_tag_int   = Py_Assign(Py_None);
    self->readonly_bool       = Py_Assign(Py_False);
    self->base_address        = NULL;
    self->byte_length         = 0;
    self->tag                 = 0;
    return (PyObject*) self;

cleanup_and_fail:
    MemoryAllocation_dealloc(self);
    Py_RETURN_NONE;
}

/**
 * Obtain a `str` representation of a `MemoryAllocation` instance.
 * @param self The `MemoryAllocation` instance.
 * @return A `str` describing the memory allocation.
 */
static PyObject*
MemoryAllocation_str
(
    PyMoxieMemoryAllocation *self
)
{
    char tag[MEM_TAG_BUFFER_SIZE];
    mem_tag_to_ascii(tag, self->tag);
    return PyUnicode_FromFormat("%s %p, %zu bytes (%S)", tag, self->base_address, self->byte_length, self->allocator_name_str);
}

/**
 * Obtain a `str` representation of a `MemoryAllocation` instance.
 * @param self The `MemoryAllocation` instance.
 * @return A `str` describing the allocation attributes.
 */
static PyObject*
MemoryAllocation_repr
(
    PyMoxieMemoryAllocation *self
)
{
    char tag[MEM_TAG_BUFFER_SIZE];
    mem_tag_to_ascii(tag, self->tag);
    return PyUnicode_FromFormat("MemoryAllocation(address=%p, length=%zu, readonly=%S, source=%s(%S))", self->base_address, self->byte_length, self->readonly_bool, tag, self->allocator_name_str);
}

/**
 * Populate a Python buffer view for an allocated block of memory.
 * @param self The `MemoryAllocation` for which the view is being obtained.
 * @param view The `Py_buffer` view structure to populate.
 * @param flags Bitflags defining the buffer request type.
 * @return Zero if the buffer interface is filled out successfully, or -1 if an error occurred.
 */
static int
MemoryAllocation_getbuffer
(
    PyMoxieMemoryAllocation *self,
    Py_buffer               *view,
    int                     flags
)
{
    int readonly;
    if (self->readonly_bool == Py_True) {
        readonly = 1;
    } else {
        readonly = 0;
    }
    return PyBuffer_FillInfo(view, (PyObject*) self, self->base_address, (Py_ssize_t) self->byte_length, readonly, flags);
}

/**
 * Free resources associated with a `MemoryAllocator` instance.
 * @param self The object to delete.
 */
static void
MemoryAllocator_dealloc
(
    PyMoxieMemoryAllocator *self
)
{
    if (self != NULL) {
        mem_allocator_delete(&self->allocator);
        Py_XDECREF(self->growable_bool);
        Py_XDECREF(self->page_size_int);
        Py_XDECREF(self->allocator_tag_int);
        Py_XDECREF(self->allocator_name_str);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

/**
 * Allocate memory for and default-initialize the fields of a `MemoryAllocator` instance.
 * @param type The `MemoryAllocatorType` instance.
 * @param args The list of arguments supplied to the `__new__` call.
 * @param kwargs The table of keyword arguments supplied to the `__new__` call.
 * @return A pointer to the new instance, or `None` if the instance initialization failed.
 */
static PyObject*
MemoryAllocator_new
(
    PyTypeObject *type, 
    PyObject     *args, 
    PyObject   *kwargs
)
{
    PyMoxieMemoryAllocator *self = NULL;

    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);

    if ((self = (PyMoxieMemoryAllocator*) type->tp_alloc(type, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    memset(&self->allocator, 0, sizeof(mem_allocator_t));
    self->allocator_name_str  = NULL;
    self->allocator_tag_int   = NULL;
    self->page_size_int       = NULL;
    self->growable_bool       = Py_Assign(Py_False);

    if ((self->allocator_name_str = PyUnicode_FromString(Py_EmptyString)) == NULL) {
        goto cleanup_and_fail;
    }
    if ((self->allocator_tag_int  = PyLong_FromUnsignedLong(0UL)) == NULL) {
        goto cleanup_and_fail;
    }
    if ((self->page_size_int      = PyLong_FromUnsignedLong(0UL)) == NULL) {
        goto cleanup_and_fail;
    }
    return (PyObject*) self;

cleanup_and_fail:
    MemoryAllocator_dealloc(self);
    Py_RETURN_NONE;
}

/**
 * Initialize a `MemoryAllocator` instance in a pre-allocated block of memory.
 * @param self The `MemoryAllocator` instance to initialize.
 * @param args The list of arguments supplied to the `__init__` call.
 * @param kwargs The table of keyword arguments supplied to the `__init__` call.
 * @return Zero if the operation was successful or -1 if the operation failed.
 */
static int
MemoryAllocator_init
(
    PyMoxieMemoryAllocator *self, 
    PyObject               *args, 
    PyObject             *kwargs
)
{
    size_t                ALIGNMENT = 16;   /* Everything is aligned for SIMD ops */
    size_t                    guard = 0;    /* Non-zero to allocate a guard page for VMM-allocated regions. */
    PyMoxieMemoryAllocator  *parent = NULL; /* The arena from which the memory should be allocated; may be None. */
    PyObject                  *temp = NULL; /* Used for dropping references. */
    char const                *name = NULL; /* A name associated with the allocator, used for debugging. */
    char const                 *tag = NULL; /* A four-byte tag associated with the allocator, used for debugging. */
    Py_ssize_t               taglen = 0;    /* The length of the tag string, in bytes. */
    Py_ssize_t              namelen = 0;    /* The length of the name string, in bytes. */
    Py_ssize_t               length = 0;    /* The chunk size, or total number of bytes to allocate from the parent */
    int                          vm = 0;    /* Non-zero - allocate using the virtual memory manager */
    int                    growable = 0;    /* Non-zero - create a growable arena, interpret length as chunk size */
    mem_tag_t                  mtag = 0;
    uint32_t                  flags = MEM_ALLOCATION_FLAG_LOCAL;
    uint32_t                 access = MEM_ACCESS_FLAG_RDWR;
    static char const     *kwlist[] = { "length", "vm", "growable", "parent", "name", "tag", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "|$nppO!z#z#:__init__", (char**) kwlist, &length, &vm, &growable, &MemoryAllocatorType, &parent, &name, &namelen, &tag, &taglen) == 0) {
        return -1;
    }
    if (length <= 0) {
        PyErr_SetString(PyExc_ValueError, "The length argument must be greater than zero");
        return -1;
    }
    if (Py_IsNotNone(parent) && (vm > 0 || growable > 0)) {
        PyErr_SetString(PyExc_ValueError, "Do not specify the vm or growable arguments when specifying a parent arena");
        return -1;
    }
    if (tag != NULL && taglen != 4) {
        PyErr_SetString(PyExc_ValueError, "Allocator tag length must be exactly 4 ASCII printable characters");
        return -1;
    }
    if (tag != NULL) {
        mtag = mem_tag(tag[0], tag[1], tag[2], tag[3]);
    } else {
        mtag = 0;
    }
    if (Py_IsNotNone(parent)) { /* Create a sub-arena */
        if (mem_allocator_create_suballocator(&self->allocator, &parent->allocator, (size_t) length, name, mtag) == NULL) {
            PyErr_Format(PyExc_MemoryError, "Failed to initialize sub-allocator %s (%s) of %zu bytes", name, parent->allocator.allocator_name, (size_t) length);
            return -1;
        }
    } else {
        if (vm) {
            flags |= MEM_ALLOCATION_FLAG_VIRTUAL;
            guard  = 1;
        } else {
            flags |= MEM_ALLOCATION_FLAG_HEAP;
            guard  = 0;
        }
        if (growable) {
            flags |= MEM_ALLOCATION_FLAG_GROWABLE;
        }
        if (mem_allocator_create(&self->allocator, (size_t) length, guard, ALIGNMENT, flags, access, name, mtag) == NULL) {
            PyErr_Format(PyExc_MemoryError, "Failed to initialize allocator %s of %zu bytes", name, (size_t) length);
            return -1;
        }
    }

    temp = self->allocator_name_str;
    if (name != NULL) {
        if ((self->allocator_name_str = PyUnicode_FromStringAndSize(name, namelen)) == NULL) {
            Py_XDECREF(temp);
            goto cleanup_and_fail;
        }
    } else {
        if ((self->allocator_name_str = PyUnicode_FromString(Py_EmptyString)) == NULL) {
            Py_XDECREF(temp);
            goto cleanup_and_fail;
        }
    }
    Py_XDECREF(temp);

    temp = self->allocator_tag_int;
    if ((self->allocator_tag_int = PyLong_FromUnsignedLong(self->allocator.allocator_tag)) == NULL) {
        Py_XDECREF(temp);
        goto cleanup_and_fail;
    }
    Py_XDECREF(temp);

    temp = self->page_size_int;
    if ((self->page_size_int = PyLong_FromSize_t(self->allocator.page_size)) == NULL) {
        Py_XDECREF(temp);
        goto cleanup_and_fail;
    }
    Py_XDECREF(temp);

    temp = self->growable_bool;
    if (growable) {
        self->growable_bool = Py_Assign(Py_True);
    } else {
        self->growable_bool = Py_Assign(Py_False);
    }
    Py_XDECREF(temp);
    return 0;

cleanup_and_fail:
    return -1;
}

/**
 * Obtain a `str` representation of a `MemoryAllocator` instance.
 * @param self The `MemoryAllocator` instance.
 * @return A `str` of the form `ABCD@123abc 1234 [2]` where `ABCD` is the FourCC tag of the source allocator, `123abc` is the hexadecimal byte offset of the start of the reservatio, `1234` is the size of the reservation, in bytes, and `[2]` is the version of the allocator at the time the reservation was obtained.
 */
static PyObject*
MemoryAllocator_str
(
    PyMoxieMemoryAllocator *self
)
{
    mem_allocator_stats_t  stats;
    char tag[MEM_TAG_BUFFER_SIZE];
    mem_allocator_stats(&stats, &self->allocator);
    mem_tag_to_ascii(tag, self->allocator.allocator_tag);
    return PyUnicode_FromFormat("%s U:%zu F:%zu T:%zu L:%zu G:%zu C:%zu W:%zu (%S)", tag, stats.bytes_used, stats.bytes_free, stats.bytes_total, stats.bytes_lost, stats.bytes_guard, stats.chunk_count, stats.watermark, self->allocator_name_str);
}

/**
 * Obtain a `str` representation of a `MemoryAllocator` instance.
 * @param self The `MemoryAllocator` instance.
 * @return A `str` describing the allocator attributes.
 */
static PyObject*
MemoryAllocator_repr
(
    PyMoxieMemoryAllocator *self
)
{
    mem_allocator_stats_t  stats;
    char tag[MEM_TAG_BUFFER_SIZE];
    mem_allocator_stats(&stats, &self->allocator);
    mem_tag_to_ascii(tag, self->allocator.allocator_tag);
    return PyUnicode_FromFormat("MemoryAllocator(name=%S, tag=%s, used=%zu, free=%zu, total=%zu, lost=%zu, guard=%zu, chunks=%zu, watermark=%zu, growable=%S)", self->allocator_name_str, tag, stats.bytes_used, stats.bytes_free, stats.bytes_total, stats.bytes_lost, stats.bytes_guard, stats.chunk_count, stats.watermark, self->growable_bool);
}

/**
 * Allocate memory from an existing `MemoryAllocator` instance.
 * @param self The `MemoryAllocator` instance from which the allocation should be obtained.
 * @param args The list of arguments supplied to the `allocate` call.
 * @param kwargs The table of keyword arguments supplied to the `allocate` call.
 * @return A `MemoryAllocation` instance, or `None` if the operation failed.
 */
static PyObject*
MemoryAllocator_allocate
(
    PyMoxieMemoryAllocator *self,
    PyObject               *args, 
    PyObject             *kwargs
)
{
    PyMoxieMemoryAllocation *alloc   = NULL;
    void                     *base   = NULL;
    Py_ssize_t                size   = 0;
    unsigned int             align   = 16;
    mem_marker_t              mark   = mem_allocator_mark(&self->allocator);
    static char const      *kwlist[] = { "size", "alignment", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "n|I:allocate", (char**) kwlist, &size, &align) == 0) {
        Py_RETURN_NONE;
    }
    if (size <= 0) {
        PyErr_SetString(PyExc_ValueError, "The size argument must be greater than zero");
        return NULL; /* Return NULL so the exception gets reported */
    }
    if (align == 0 || (align & (align-1)) != 0 || align > 65536) {
        PyErr_SetString(PyExc_ValueError, "The alignment argument must be a non-zero power of two less than 64KiB");
        return NULL; /* Return NULL so the exception gets reported */
    }
    if ((base = mem_allocator_alloc(&self->allocator, (size_t) size, (size_t) align)) == NULL) {
        Py_RETURN_NONE;
    }
    if ((alloc = (PyMoxieMemoryAllocation*) MemoryAllocationType.tp_alloc(&MemoryAllocationType, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    alloc->base_address_int   = NULL;
    alloc->length_int         = NULL;
    alloc->allocator_name_str = Py_Assign(self->allocator_name_str);
    alloc->allocator_tag_int  = Py_Assign(self->allocator_tag_int);
    alloc->readonly_bool      = NULL;
    alloc->base_address       =(void *) base;
    alloc->byte_length        =(size_t) size;
    alloc->tag                = self->allocator.allocator_tag;

    if ((alloc->base_address_int = PyLong_FromVoidPtr((void*) base)) == NULL) {
        goto cleanup_and_fail;
    }
    if ((alloc->length_int = PyLong_FromSsize_t(size)) == NULL) {
        goto cleanup_and_fail;
    }
    if((self->allocator.access_flags & MEM_ACCESS_FLAG_WRITE) == 0) {
        alloc->readonly_bool = Py_Assign(Py_True);
    } else {
        alloc->readonly_bool = Py_Assign(Py_False);
    }
    Py_INCREF(alloc);
    return (PyObject *)alloc;

cleanup_and_fail:
    mem_allocator_reset_to_marker(&self->allocator, &mark);
    MemoryAllocation_dealloc(alloc);
    Py_RETURN_NONE;
}

/**
 * Obtain a marker representing the state of a `MemoryAllocator` at the current point in time.
 * @param self The `MemoryAllocator` for which the marker should be obtained.
 * @param args Arguments supplied to the function (unused).
 * @return This function always returns `None`.
 */
static PyObject*
MemoryAllocator_mark
(
    PyMoxieMemoryAllocator *self,
    PyObject               *args
)
{
    PyMoxieMemoryMarker *marker = NULL;

    PLATFORM_UNUSED_PARAM(args);
    if ((marker = (PyMoxieMemoryMarker*) MemoryMarkerType.tp_alloc(&MemoryMarkerType, 0)) == NULL) {
        Py_RETURN_NONE;
    }
    marker->marker = mem_allocator_mark(&self->allocator);
    marker->allocator_name_str = Py_Assign(self->allocator_name_str);
    marker->allocator_tag_int  = Py_Assign(self->allocator_tag_int);
    Py_INCREF(marker);
    return (PyObject *)marker;
}

/**
 * Reset a `MemoryAllocator` instance, invalidating all allocations.
 * @param self The `MemoryAllocator` to reset.
 * @param args Arguments supplied to the function (unused).
 * @return This function always returns `None`.
 */
static PyObject*
MemoryAllocator_reset
(
    PyMoxieMemoryAllocator *self,
    PyObject               *args
)
{
    PLATFORM_UNUSED_PARAM(args);
    mem_allocator_reset(&self->allocator);
    Py_RETURN_NONE;
}

/**
 * Reset a `MemoryAllocator` instance to a previously obtained marker, invalidating all allocations made since that point in time.
 * @param self The `MemoryAllocator` to reset.
 * @param args The tuple of arguments supplied to the function, specifically an instance of `MemoryMarker`.
 * @return This function always returns `None`.
 */
static PyObject*
MemoryAllocator_reset_to_marker
(
    PyMoxieMemoryAllocator *self,
    PyObject               *args
)
{
    PyMoxieMemoryMarker *marker = NULL;

    if (PyArg_ParseTuple(args, "O!", &MemoryMarkerType, &marker) < 0) {
        Py_RETURN_NONE;
    }
    mem_allocator_reset_to_marker(&self->allocator, &marker->marker);
    Py_RETURN_NONE;
}

/**
 * Free resources associated with a `JobQueue` instance.
 * @param self The object to delete.
 */
static void
JobQueue_dealloc
(
    PyMoxieJobQueue *self
)
{
    if (self != NULL) {
        Py_XDECREF(self->queue_name_str);
        Py_XDECREF(self->queue_id_int);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

/**
 * Allocate memory for and default-initialize the fields of a `JobQueue` instance.
 * @param type The `JobQueueType` instance.
 * @param args The list of arguments supplied to the `__new__` call.
 * @param kwargs The table of keyword arguments supplied to the `__new__` call.
 * @return A pointer to the new instance, or `None` if the instance initialization failed.
 */
static PyObject*
JobQueue_new
(
    PyTypeObject *type, 
    PyObject     *args, 
    PyObject   *kwargs
)
{
    PyMoxieJobQueue *self = NULL;

    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);

    if ((self = (PyMoxieJobQueue*) type->tp_alloc(type, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    self->queue_name_str = Py_Assign(Py_None);
    self->queue_id_int   = Py_Assign(Py_None);
    return (PyObject*) self;

cleanup_and_fail:
    JobQueue_dealloc(self);
    Py_RETURN_NONE;
}

/**
 * Initialize a `JobQueue` instance in a pre-allocated block of memory.
 * @param self The `JobQueue` instance to initialize.
 * @param args The list of arguments supplied to the `__init__` call.
 * @param kwargs The table of keyword arguments supplied to the `__init__` call.
 * @return Zero if the operation was successful or -1 if the operation failed.
 */
static int
JobQueue_init
(
    PyMoxieJobQueue *self, 
    PyObject        *args, 
    PyObject      *kwargs
)
{
    struct job_queue_t       *queue = NULL;
    PyObject                  *temp = NULL; /* Used for dropping references. */
    char const                *name = NULL; /* A name associated with the queue, used for debugging. */
    Py_ssize_t              namelen = 0;    /* The length of the name string, in bytes. */
    Py_ssize_t                 argc = 0;
    uint32_t                     id = 0;    /* The unique integer identifier of the queue. */
    static char const     *kwlist[] = { "name", "id", NULL };

    if (kwargs == NULL || (argc = PyDict_Size(kwargs)) == 0) {
        PyErr_SetString(PyExc_SyntaxError, "Too few arguments specified to JobQueue.__init__");
        return -1;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "|$z#I:__init__", (char**) kwlist, &name, &namelen, &id) == 0) {
        return -1;
    }
    if (argc < 2) {
        if (id == 0) { /* No id argument specified - hash the name */
            PyObject *name_obj = PyDict_GetItemString(kwargs, "name");
            if (name_obj != NULL) {
                id  = (uint32_t) PyObject_Hash(name_obj);
            } else {
                PyErr_SetString(PyExc_SyntaxError, "JobQueue name argument must be a non-empty string");
                return -1;
            }
        }
    }
    if ((queue = job_queue_create(id)) == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create job queue instance");
        return -1;
    }

    temp = self->queue_name_str;
    if (name != NULL) {
        if ((self->queue_name_str = PyUnicode_FromStringAndSize(name, namelen)) == NULL) {
            Py_XDECREF(temp);
            goto cleanup_and_fail;
        }
    }
    Py_XDECREF(temp);

    temp = self->queue_id_int;
    if ((self->queue_id_int = PyLong_FromUnsignedLong(id)) == NULL) {
        Py_XDECREF(temp);
        goto cleanup_and_fail;
    }
    Py_XDECREF(temp);

    self->queue = queue;
    return 0;

cleanup_and_fail:
    return -1;
}

/**
 * Obtain a `str` representation of a `JobQueue` instance.
 * @param self The `JobQueue` instance.
 * @return A `str` describing the `JobQueue`.
 */
static PyObject*
JobQueue_str
(
    PyMoxieJobQueue *self
)
{
    return PyUnicode_FromFormat("%S [%S] @ %p", self->queue_name_str, self->queue_id_int, (void*) self->queue);
}

/**
 * Obtain a `str` representation of a `JobQueue` instance.
 * @param self The `JobQueue` instance.
 * @return A `str` describing the job queue.
 */
static PyObject*
JobQueue_repr
(
    PyMoxieJobQueue *self
)
{
    return PyUnicode_FromFormat("JobQueue(name=%S, id=%S)", self->queue_name_str, self->queue_id_int);
}

/**
 * Flush a `JobQueue`, returning it to an empty state and releasing all waiting producers.
 * @param self The `JobQueue` instance to flush.
 * @return This function always returns `None`.
 */
static PyObject*
JobQueue_flush
(
    PyMoxieJobQueue *self
)
{
    job_queue_flush(self->queue);
    Py_RETURN_NONE;
}

/**
 * Check the signal status of a `JobQueue`.
 * @param self The `JobQueue` instance to query.
 * @return An `int` specifying the current signal code. If no signal is raised, this value will be zero.
 */
static PyObject*
JobQueue_check_signal
(
    PyMoxieJobQueue *self
)
{
    uint32_t signal = job_queue_check_signal(self->queue);
    return PyLong_FromUnsignedLong(signal);
}

/**
 * Raise a signal on a `JobQueue`, and wake all waiting producers and consumers.
 * @param self The `JobQueue` instance to signal.
 * @param args A single `int` argument specifying the signal code.
 * @param kwargs A `dict` specifying the keyword arguments. The function accepts a single argument, `code: int`.
 * @return This function always returns `None`.
 */
static PyObject*
JobQueue_raise_signal
(
    PyMoxieJobQueue *self,
    PyObject        *args,
    PyObject      *kwargs
)
{
    uint32_t           signal   = JOB_QUEUE_SIGNAL_CLEAR;
    static char const *kwlist[] = { "code", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "|k:signal", (char**) kwlist, &signal) == 0) {
        return NULL;
    }
    job_queue_signal(self->queue, signal);
    Py_RETURN_NONE;
}

/**
 * Free resources associated with a `JobContext` instance.
 * @param self The object to delete.
 */
static void
JobContext_dealloc
(
    PyMoxieJobContext *self
)
{
    if (self != NULL) {
        if (self->jobctx != NULL) {
            struct job_scheduler_t *scheduler = job_context_scheduler(self->jobctx);
            job_scheduler_release_context(scheduler, self->jobctx);
            self->jobctx = NULL;
        }

        Py_XDECREF(self->context_name_str);
        Py_XDECREF(self->job_queue_obj);
        Py_XDECREF(self->scheduler_obj);
        Py_XDECREF(self->thread_id_int);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

/**
 * Allocate memory for and default-initialize the fields of a `JobContext` instance.
 * @param type The `JobContextType` instance.
 * @param args The list of arguments supplied to the `__new__` call.
 * @param kwargs The table of keyword arguments supplied to the `__new__` call.
 * @return A pointer to the new instance, or `None` if the instance initialization failed.
 */
static PyObject*
JobContext_new
(
    PyTypeObject *type, 
    PyObject     *args, 
    PyObject   *kwargs
)
{
    PyMoxieJobContext *self = NULL;

    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);

    if ((self = (PyMoxieJobContext*) type->tp_alloc(type, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    self->context_name_str = Py_Assign(Py_None);
    self->job_queue_obj    = Py_Assign(Py_None);
    self->scheduler_obj    = Py_Assign(Py_None);
    self->thread_id_int    = Py_Assign(Py_None);
    self->jobctx           = NULL;
    return (PyObject*) self;

cleanup_and_fail:
    JobContext_dealloc(self);
    Py_RETURN_NONE;
}

/**
 * Obtain a `str` representation of a `JobContext` instance.
 * @param self The `JobContext` instance.
 * @return A `str` describing the `JobContext`.
 */
static PyObject*
JobContext_str
(
    PyMoxieJobContext *self
)
{
    return PyUnicode_FromFormat("[%S] %S <=> %S", self->thread_id_int, self->context_name_str, self->job_queue_obj);
}

/**
 * Obtain a `str` representation of a `JobContext` instance.
 * @param self The `JobContext` instance.
 * @return A `str` describing the job queue.
 */
static PyObject*
JobContext_repr
(
    PyMoxieJobContext *self
)
{
    return PyUnicode_FromFormat("JobContext(name=%S, queue=%S, thread=%S)", self->context_name_str, self->job_queue_obj, self->thread_id_int);
}

/**
 * Implement the context manager __enter__ function for a `JobContext`.
 * @param self The `JobContext` being acquired.
 * @return A new reference to `self`.
 */
static PyObject*
JobContext_enter
(
    PyMoxieJobContext *self
)
{
    Py_XINCREF(self);
    return (PyObject*) self;
}

/**
 * Implement the context manager `__exit__` function for a `JobContext`, releasing the `JobContext` back to the `JobScheduler` that created it.
 * @param self The `JobContext` being released.
 * @param args Positional arguments passed to the function.
 * @param kwargs Keyword arguments passed to the function.
 * @return A new reference to `self`.
 */
static PyObject*
JobContext_exit
(
    PyMoxieJobContext *self,
    PyObject          *args,
    PyObject        *kwargs
)
{
    PyObject *result = NULL;

    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);

    if ((result = PyObject_CallMethod(self->scheduler_obj, "release_context", "(O)", self)) == NULL) {
        return NULL;
    }
    Py_INCREF(self);
    Py_DECREF(result);
    return (PyObject*) self;
}

/**
 * Free resources associated with a `JobScheduler` instance.
 * @param self The object to delete.
 */
static void
JobScheduler_dealloc
(
    PyMoxieJobScheduler *self
)
{
    if (self != NULL) {
        if (self->scheduler != NULL) {
            job_scheduler_terminate(self->scheduler);
            job_scheduler_delete(self->scheduler);
        }
        if (self->jobctx_list != NULL) {
            PyList_SetSlice(self->jobctx_list, 0, PyList_Size(self->jobctx_list), NULL);
        }

        Py_XDECREF(self->scheduler_name_str);
        Py_XDECREF(self->jobctx_list);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

/**
 * Allocate memory for and default-initialize the fields of a `JobScheduler` instance.
 * @param type The `JobSchedulerType` instance.
 * @param args The list of arguments supplied to the `__new__` call.
 * @param kwargs The table of keyword arguments supplied to the `__new__` call.
 * @return A pointer to the new instance, or `None` if the instance initialization failed.
 */
static PyObject*
JobScheduler_new
(
    PyTypeObject *type, 
    PyObject     *args, 
    PyObject   *kwargs
)
{
    PyMoxieJobScheduler *self = NULL;

    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);

    if ((self = (PyMoxieJobScheduler*) type->tp_alloc(type, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    self->scheduler_name_str = Py_Assign(Py_None);
    self->jobctx_list        = PyList_New(0);
    self->scheduler          = NULL;
    return (PyObject*) self;

cleanup_and_fail:
    JobScheduler_dealloc(self);
    Py_RETURN_NONE;
}

/**
 * Initialize a `JobScheduler` instance in a pre-allocated block of memory.
 * @param self The `JobScheduler` instance to initialize.
 * @param args The list of arguments supplied to the `__init__` call.
 * @param kwargs The table of keyword arguments supplied to the `__init__` call.
 * @return Zero if the operation was successful or -1 if the operation failed.
 */
static int
JobScheduler_init
(
    PyMoxieJobScheduler *self, 
    PyObject            *args, 
    PyObject          *kwargs
)
{
    struct job_scheduler_t   *sched = NULL;
    PyObject                  *temp = NULL; /* Used for dropping references. */
    char const                *name = NULL; /* A name associated with the scheduler, used for debugging. */
    Py_ssize_t              namelen = 0;    /* The length of the name string, in bytes. */
    Py_ssize_t                 argc = 0;
    Py_ssize_t        context_count = 0;    /* The number of job contexts to pre-allocate. */
    static char const     *kwlist[] = { "name", "context_count", NULL };

    if (kwargs == NULL || (argc = PyDict_Size(kwargs)) == 0) {
        PyErr_SetString(PyExc_SyntaxError, "Too few arguments specified to JobScheduler.__init__");
        return -1;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "|$z#n:__init__", (char**) kwlist, &name, &namelen, &context_count) == 0) {
        return -1;
    }
    if (context_count < 0) {
        context_count = 0;
    }
    if ((sched = job_scheduler_create((size_t) context_count)) == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create job scheduler instance");
        return -1;
    }

    temp = self->scheduler_name_str;
    if (name != NULL) {
        if ((self->scheduler_name_str = PyUnicode_FromStringAndSize(name, namelen)) == NULL) {
            Py_XDECREF(temp);
            goto cleanup_and_fail;
        }
    }
    Py_XDECREF(temp);

    temp = self->jobctx_list;
    if ((self->jobctx_list = PyList_New(0)) == NULL) {
        Py_XDECREF(temp);
        goto cleanup_and_fail;
    }
    Py_XDECREF(temp);

    self->scheduler = sched;
    return 0;

cleanup_and_fail:
    job_scheduler_delete(sched);
    return -1;
}

/**
 * Obtain a `str` representation of a `JobScheduler` instance.
 * @param self The `JobScheduler` instance.
 * @return A `str` describing the `JobScheduler`.
 */
static PyObject*
JobScheduler_str
(
    PyMoxieJobScheduler *self
)
{
    return PyUnicode_FromFormat("%S (%zd)", self->scheduler_name_str, PyList_Size(self->jobctx_list));
}

/**
 * Obtain a `str` representation of a `JobScheduler` instance.
 * @param self The `JobScheduler` instance.
 * @return A `str` describing the job queue.
 */
static PyObject*
JobScheduler_repr
(
    PyMoxieJobScheduler *self
)
{
    return PyUnicode_FromFormat("JobScheduler(name=%S, context_count=%zd)", self->scheduler_name_str, PyList_Size(self->jobctx_list));
}

/**
 * Wake up all waiting worker threads and signal them to terminate.
 * @param self The `JobScheduler` to signal.
 * @return This function always returns `None`.
 */
static PyObject*
JobScheduler_terminate
(
    PyMoxieJobScheduler *self
)
{
    job_scheduler_terminate(self->scheduler);
    Py_RETURN_NONE;
}

/**
 * Acquire a new `JobContext` used to create, submit and wait for jobs.
 * @param self The `JobScheduler` from which the context should be acquired.
 * @param args The positional argument list.
 * @param kwargs The keyword argument `dict`.
 * @return A new `JobContext` instance, or `None`.
 */
static PyObject*
JobScheduler_acquire_context
(
    PyMoxieJobScheduler *self,
    PyObject            *args,
    PyObject          *kwargs
)
{
    PyMoxieJobContext      *context = NULL;
    PyMoxieJobQueue          *queue = NULL; /* PyMoxieJobQueue work queue to wait on. */
    PyObject                 *pytid = NULL; /* PyLong thread identifier. */
    PyObject                  *temp = NULL; /* Used for dropping references. */
    char const                *name = NULL; /* A name associated with the context, used for debugging. */
    Py_ssize_t              namelen = 0;    /* The length of the name string, in bytes. */
    thread_id_t               owner = THREAD_ID_INVALID;
    static char const     *kwlist[] = { "name", "work_queue", "owner_ident", NULL };

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "$z#O!O!:acquire_context", (char**) kwlist, &name, &namelen, &JobQueueType, &queue, &PyLong_Type, &pytid) == 0) {
        return NULL;
    }
    if (Py_IsNotNone(pytid)) {
        owner =(thread_id_t) PyLong_AsUnsignedLongLong(pytid);
        if (PyErr_Occurred()) {
            return NULL;
        }
    } else {
        owner = current_thread_id();
    }
    if (Py_IsNone(queue)) {
        PyErr_SetString(PyExc_ValueError, "The work_queue argument must be set to a valid JobQueue");
        return NULL;
    }
    if (name == NULL || namelen == 0) {
        name  = "(unnamed)";
    }
    if ((context = (PyMoxieJobContext*) JobContext_new(&JobContextType, NULL, NULL)) == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate new JobContext instance");
        return NULL; 
    }
    if ((context->jobctx = job_scheduler_acquire_context(self->scheduler, queue->queue, owner)) == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to acquire a new job_context_t");
        goto cleanup_and_fail;
    }

    temp = context->context_name_str;
    if (name != NULL) {
        if ((context->context_name_str = PyUnicode_FromStringAndSize(name, namelen)) == NULL) {
            Py_XDECREF(temp);
            goto cleanup_and_fail;
        }
    }
    Py_XDECREF(temp);

    temp = context->job_queue_obj;
    context->job_queue_obj = (PyObject*) queue;
    Py_INCREF(queue);
    Py_XDECREF(temp);

    temp = context->scheduler_obj;
    context->scheduler_obj = (PyObject*) self;
    Py_INCREF(self);
    Py_XDECREF(temp);

    temp = context->thread_id_int;
    if ((context->thread_id_int = PyLong_FromUnsignedLongLong((unsigned long long) owner)) == NULL) {
        Py_XDECREF(temp);
        goto cleanup_and_fail;
    }
    Py_XDECREF(temp);

    if (PyList_Append(self->jobctx_list, _PyObject_CAST(context)) < 0) {
        goto cleanup_and_fail;
    }

    Py_INCREF(context);
    return (PyObject*) context;

cleanup_and_fail:
    JobContext_dealloc(context);
    return NULL;
}

/**
 * Release a previously-acquired `JobContext` when the owning thread no longer needs it.
 * @param self The `JobScheduler` from which the `JobContext` was acquired.
 * @param args A `JobContext` instance specifying the context to release.
 * @return This function always returns `None`.
 */
static PyObject*
JobScheduler_release_context
(
    PyMoxieJobScheduler *self,
    PyObject            *args
)
{
    PyMoxieJobContext *jobctx = NULL;
    Py_ssize_t           i, n;

    if (PyArg_ParseTuple(args, "O!", &JobContextType, &jobctx) < 0) {
        return NULL;
    }
    if (jobctx == NULL || _PyObject_CAST(jobctx) == Py_None) {
        Py_RETURN_NONE;
    }
    if ((n = PyList_Size(self->jobctx_list)) <= 0) {
        Py_RETURN_NONE;
    }
    for (i = 0; i < n; ++i) {
        PyMoxieJobContext  *item = (PyMoxieJobContext*) PyList_GetItem(self->jobctx_list, i);
        if (item != NULL && item->jobctx == jobctx->jobctx) {
            PyList_SetSlice(self->jobctx_list, i, i+1, NULL);
            break;
        }
    }
    Py_DECREF(jobctx);
    Py_RETURN_NONE;
}


static PyMemberDef MemoryMarker_members[] = {
    { "tag"      , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryMarker, allocator_tag_int ), READONLY, PyDoc_STR("") },
    { "allocator", T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryMarker, allocator_name_str), READONLY, PyDoc_STR("") },
    { NULL, 0, 0, 0, NULL }
};

static PyMemberDef MemoryAllocator_members[] = {
    { "name"     , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocator, allocator_name_str ), READONLY, PyDoc_STR("") },
    { "tag"      , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocator, allocator_tag_int  ), READONLY, PyDoc_STR("") },
    { "page_size", T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocator, page_size_int      ), READONLY, PyDoc_STR("") },
    { "growable" , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocator, growable_bool      ), READONLY, PyDoc_STR("") },
    { NULL, 0, 0, 0, NULL }
};

static PyMemberDef MemoryAllocation_members[] = {
    { "address"  , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocation, base_address_int  ), READONLY, PyDoc_STR("") },
    { "length"   , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocation, length_int        ), READONLY, PyDoc_STR("") },
    { "readonly" , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocation, readonly_bool     ), READONLY, PyDoc_STR("") },
    { "tag"      , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocation, allocator_tag_int ), READONLY, PyDoc_STR("") },
    { "allocator", T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieMemoryAllocation, allocator_name_str), READONLY, PyDoc_STR("") },
    { NULL, 0, 0, 0, NULL }
};

static PyMemberDef JobQueue_members[] = {
    { "id"       , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobQueue, queue_id_int  ), READONLY, PyDoc_STR("") },
    { "name"     , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobQueue, queue_name_str), READONLY, PyDoc_STR("") },
    { NULL, 0, 0, 0, NULL }
};

static PyMemberDef JobContext_members[] = {
    { "name"     , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobContext, context_name_str), READONLY, PyDoc_STR("") },
    { "owner"    , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobContext, thread_id_int   ), READONLY, PyDoc_STR("") },
    { "queue"    , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobContext, job_queue_obj   ), READONLY, PyDoc_STR("") },
    { "scheduler", T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobContext, scheduler_obj   ), READONLY, PyDoc_STR("") },
    { NULL, 0, 0, 0, NULL }
};

static PyMemberDef JobScheduler_members[] = {
    { "name"     , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobScheduler, scheduler_name_str), READONLY, PyDoc_STR("") },
    { "contexts" , T_OBJECT_EX, PLATFORM_OFFSET_OF(PyMoxieJobScheduler, jobctx_list       ), READONLY, PyDoc_STR("") },
    { NULL, 0, 0, 0, NULL }
};

static PyMethodDef   MemoryMarker_methods[] = {
    { NULL, NULL, 0, NULL }
};

static PyMethodDef   MemoryAllocator_methods[] = {
    { "allocate"       , (PyCFunction) MemoryAllocator_allocate       , METH_VARARGS | METH_KEYWORDS, PyDoc_STR("") },
    { "mark"           , (PyCFunction) MemoryAllocator_mark           , METH_NOARGS                 , PyDoc_STR("") },
    { "reset"          , (PyCFunction) MemoryAllocator_reset          , METH_NOARGS                 , PyDoc_STR("") },
    { "reset_to_marker", (PyCFunction) MemoryAllocator_reset_to_marker, METH_VARARGS                , PyDoc_STR("") },
    { NULL, NULL, 0, NULL }
};

static PyMethodDef   MemoryAllocation_methods[] = {
    { NULL, NULL, 0, NULL }
};

static PyMethodDef   JobQueue_methods[] = {
    { "flush"         , (PyCFunction) JobQueue_flush                  , METH_NOARGS                 , PyDoc_STR("") },
    { "check_signal"  , (PyCFunction) JobQueue_check_signal           , METH_NOARGS                 , PyDoc_STR("") },
    { "raise_signal"  , (PyCFunction) JobQueue_raise_signal           , METH_VARARGS | METH_KEYWORDS, PyDoc_STR("") },
    { NULL, NULL, 0, NULL }
};

static PyMethodDef   JobContext_methods[] = {
    { "__enter__"     , (PyCFunction) JobContext_enter                , METH_NOARGS                 , PyDoc_STR("") },
    { "__exit__"      , (PyCFunction) JobContext_exit                 , METH_VARARGS | METH_KEYWORDS, PyDoc_STR("") },
    { NULL, NULL, 0, NULL }
};

static PyMethodDef   JobScheduler_methods[] = {
    { "acquire_context", (PyCFunction) JobScheduler_acquire_context   , METH_VARARGS | METH_KEYWORDS, PyDoc_STR("") },
    { "release_context", (PyCFunction) JobScheduler_release_context   , METH_VARARGS                , PyDoc_STR("") },
    { "terminate"      , (PyCFunction) JobScheduler_terminate         , METH_NOARGS                 , PyDoc_STR("") },
    { NULL, NULL, 0, NULL }
};

static PyBufferProcs MemoryAllocation_as_buffer = {
    .bf_getbuffer     = (getbufferproc    ) MemoryAllocation_getbuffer,
    .bf_releasebuffer = (releasebufferproc) NULL
};

PyTypeObject MemoryMarkerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = TYPE_NAME(MemoryMarker),
    .tp_doc       = "", 
    .tp_basicsize = sizeof(PyMoxieMemoryMarker),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT, 
    .tp_new       =(newfunc   ) MemoryMarker_new,
    .tp_dealloc   =(destructor) MemoryMarker_dealloc,
    .tp_repr      =(reprfunc  ) MemoryMarker_repr,
    .tp_str       =(reprfunc  ) MemoryMarker_str,
    .tp_members   = MemoryMarker_members,
    .tp_methods   = MemoryMarker_methods
};

PyTypeObject MemoryAllocatorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = TYPE_NAME(MemoryAllocator),
    .tp_doc       = "",
    .tp_basicsize = sizeof(PyMoxieMemoryAllocator),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   ) MemoryAllocator_new,
    .tp_init      =(initproc  ) MemoryAllocator_init,
    .tp_dealloc   =(destructor) MemoryAllocator_dealloc,
    .tp_repr      =(reprfunc  ) MemoryAllocator_repr,
    .tp_str       =(reprfunc  ) MemoryAllocator_str,
    .tp_members   = MemoryAllocator_members,
    .tp_methods   = MemoryAllocator_methods
};

PyTypeObject MemoryAllocationType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = TYPE_NAME(MemoryAllocation),
    .tp_doc       = "",
    .tp_basicsize = sizeof(PyMoxieMemoryAllocation),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   ) MemoryAllocation_new,
    .tp_dealloc   =(destructor) MemoryAllocation_dealloc,
    .tp_repr      =(reprfunc  ) MemoryAllocation_repr,
    .tp_str       =(reprfunc  ) MemoryAllocation_str,
    .tp_members   = MemoryAllocation_members,
    .tp_methods   = MemoryAllocation_methods,
    .tp_as_buffer =&MemoryAllocation_as_buffer
};

PyTypeObject JobQueueType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = TYPE_NAME(JobQueue),
    .tp_doc       = "",
    .tp_basicsize = sizeof(PyMoxieJobQueue),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   ) JobQueue_new,
    .tp_init      =(initproc  ) JobQueue_init,
    .tp_dealloc   =(destructor) JobQueue_dealloc,
    .tp_repr      =(reprfunc  ) JobQueue_repr,
    .tp_str       =(reprfunc  ) JobQueue_str,
    .tp_members   = JobQueue_members,
    .tp_methods   = JobQueue_methods
};

PyTypeObject JobContextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = TYPE_NAME(JobContext),
    .tp_doc       = "",
    .tp_basicsize = sizeof(PyMoxieJobContext),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   ) JobContext_new,
    .tp_dealloc   =(destructor) JobContext_dealloc,
    .tp_repr      =(reprfunc  ) JobContext_repr,
    .tp_str       =(reprfunc  ) JobContext_str,
    .tp_members   = JobContext_members,
    .tp_methods   = JobContext_methods
};

PyTypeObject JobSchedulerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = TYPE_NAME(JobScheduler),
    .tp_doc       = "",
    .tp_basicsize = sizeof(PyMoxieJobScheduler),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   ) JobScheduler_new,
    .tp_init      =(initproc  ) JobScheduler_init,
    .tp_dealloc   =(destructor) JobScheduler_dealloc,
    .tp_repr      =(reprfunc  ) JobScheduler_repr,
    .tp_str       =(reprfunc  ) JobScheduler_str,
    .tp_members   = JobScheduler_members,
    .tp_methods   = JobScheduler_methods
};
