"""
Implements the high-level interface for memory management built on top of the low-level _moxie_core memory allocator.
"""
from   typing import Optional

import moxie._moxie_core as _mc
from   moxie._moxie_core import MemoryMarker
from   moxie._moxie_core import MemoryAllocation


DEFAULT_ALIGNMENT: int = 16 # The default alignment, in bytes, for memory returned from a `MemoryAllocator`.


class MemoryAllocator:
    """
    Provides a mechanism for allocating large, contiguous chunks of memory for use in storing data like `numpy` arrays, image pixel data, etc.

    Fields
    ------
        page_size: The size of a single page of virtual address space, in bytes.
        growable : This field is `True` if the memory arena can increase its total capacity.
        readonly : This field is `True` if the memory returned by the arena is read-only.
        virtual  : This field is `True` if the memory allocated from the arena is allocated using the host virtual memory manager.
        name     : A string name associated with the allocator, used for debugging.
        tag      : An integer tag value identifying the allocator, used for debugging.
    """
    def __init__(self, chunk_size: int, alignment: int=DEFAULT_ALIGNMENT, virtual_memory: bool=False, growable: bool=True, name: Optional[str]=None, tag: Optional[str]=None) -> None:
        flags : int = 0
        access: int = _mc.MEM_ACCESS_FLAG_RDWR

        if alignment < 1:
            raise ValueError(f'The alignment argument {alignment} must be >= 1')
        if (alignment & (alignment-1)) != 0:
            raise ValueError(f'The alignment argument {alignment} must be a non-zero power of two')
        if chunk_size <= alignment:
            raise ValueError(f'The chunk_size argument {chunk_size} must be >= the alignment {alignment}')

        if virtual_memory:
            flags = _mc.MEM_ALLOCATION_FLAG_LOCAL | _mc.MEM_ALLOCATION_FLAG_VIRTUAL
        else:
            flags = _mc.MEM_ALLOCATION_FLAG_LOCAL | _mc.MEM_ALLOCATION_FLAG_HEAP

        if growable:
            flags = _mc.MEM_ALLOCATION_FLAG_GROWABLE | flags
        
        self._internal  = _mc.create_memory_allocator(chunk_size, alignment, flags, access, name, tag)
        self.chunk_size = chunk_size
        self.page_size  = self._internal.page_size
        self.growable   = self._internal.growable
        self.readonly   = True if access == _mc.MEM_ACCESS_FLAG_READ else False
        self.virtual    = True if virtual_memory else False
        self.name       = self._internal.name
        self.tag        = self._internal.tag

    def reset(self) -> None:
        """
        Reset the memory arena, invalidating all memory allocations.
        """
        _mc.reset_memory_allocator(self._internal)

    def allocate(self, length: int, alignment: int=DEFAULT_ALIGNMENT) -> MemoryAllocation:
        """
        Attempt to allocate some number of bytes from the arena, with a given alignment.

        Parameters
        ----------
            length   : The minimum size of the memory region to return, in bytes.
            alignment: The desired alignment of the first addressable byte of the returned memory region, in bytes. This must be a non-zero power of two less than or equal to the host page size.

        Returns
        -------
            A `MemoryAllocation` representing the allocation, or `None` if the allocation failed.
            The `MemoryAllocation` type implements the Python buffer protocol.

        Raises
        ------
            A `ValueError` if `length` is less than or equal to zero.
            A `ValueError` if `alignment` is less than or equal to zero.
            A `ValueError` if `alignment` is not a power of two.
            A `ValueError` if `alignment` is greater than the host system page size.
        """
        return _mc.allocate_memory(self._internal, length, alignment)

    def mark(self) -> MemoryMarker:
        """
        Obtain a marker representing the state of the arena at the current point in time.
        The returned `MemoryMarker` can be used to roll back several allocations made after the marker was obtained.

        Returns
        -------
            A `MemoryMarker` instance storing the state of the arena.
        """
        return _mc.create_allocator_marker(self._internal)

    def reset_to(self, marker: MemoryMarker) -> None:
        """
        Reset the memory arena back to a previously obtained marker, invalidating all allocations made after the marker was obtained.

        Parameters
        ----------
            marker: A `MemoryMarker` obtained by a prior call to `MemoryAllocator.mark`.
        """
        _mc.reset_memory_allocator_to_marker(self._internal, marker)
