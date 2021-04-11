/**
 * exttypes.h: Define all of the types exposed publicly by the extension to the Python code.
 */
#ifndef __PYEXT_EXTTYPES_H__
#define __PYEXT_EXTTYPES_H__

#pragma once

#include <stddef.h>
#include <stdint.h>

#define  PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#include "moxie/platform.h"
#include "moxie/memory.h"
#include "moxie/scheduler.h"

extern PyTypeObject MemoryMarkerType;                                          /* Python type MemoryMarker => py_memory_marker_t.            */
extern PyTypeObject MemoryAllocatorType;                                       /* Python type MemoryAllocator => py_memory_allocator_t.      */
extern PyTypeObject MemoryAllocationType;                                      /* Python type MemoryAllocation => py_memory_allocation_t.    */

typedef struct PyMoxieMemoryMarker {                                           /* Python type wrapper for moxie's mem_marker_t struct.       */
    PyObject_HEAD
    PyObject        *allocator_name_str;
    PyObject         *allocator_tag_int;
    mem_marker_t                 marker;
} PyMoxieMemoryMarker;

typedef struct PyMoxieMemoryAllocator {                                        /* Python type wrapper for moxie's mem_allocator_t struct.    */
    PyObject_HEAD
    PyObject        *allocator_name_str;
    PyObject         *allocator_tag_int;
    PyObject             *page_size_int;
    PyObject             *growable_bool;
    mem_allocator_t           allocator;
} PyMoxieMemoryAllocator;

typedef struct PyMoxieMemoryAllocation {                                       /* Python type wrapper for a memory allocation.               */
    PyObject_HEAD
    PyObject          *base_address_int;
    PyObject                *length_int;
    PyObject        *allocator_name_str;
    PyObject         *allocator_tag_int;
    PyObject             *readonly_bool;
    void                  *base_address;
    size_t                  byte_length;
    mem_tag_t                       tag;
} PyMoxieMemoryAllocation;

#endif /* __PYEXT_EXTTYPES_H__ */
