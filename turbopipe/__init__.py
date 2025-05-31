from typing import Optional, Union

from moderngl import Buffer

from turbopipe import _turbopipe

__all__ = [
    "pipe",
    "sync",
    "close"
]

def pipe(buffer: Union[Buffer, memoryview], fileno: int) -> None:
    """Pipe a buffer contents to a file descriptor, fast and threaded"""
    if isinstance(buffer, Buffer):
        buffer = memoryview(buffer.mglo)
    _turbopipe.pipe(buffer, fileno)
    del buffer

def sync(buffer: Optional[Union[Buffer, memoryview]]=None) -> None:
    """Wait for pending operations on a buffer to finish"""
    if isinstance(buffer, Buffer):
        buffer = memoryview(buffer.mglo)
    _turbopipe.sync(buffer)
    del buffer

def close() -> None:
    """Syncs and deletes objects"""
    _turbopipe.close()
