from typing import Optional, Union

from moderngl import Buffer

from turbopipe import _turbopipe


def pipe(buffer: Union[Buffer, memoryview], fileno: int) -> None:
    """
    Pipe the content of a moderngl.Buffer or memoryview to a file descriptor, fast, threaded and
    blocking when needed. Call `sync(buffer)` before this, and `sync()` when done for

    Usage:
        ```python
        # Assuming `buffer = ctx.buffer(...)`
        # Note: Use as `fbo.read_into(buffer)`

        # As a open() file
        with open("file.bin", "wb") as file:
            turbopipe.pipe(buffer, file)

        # As a subprocess
        child = subprocess.Popen(..., stdin=subprocess.PIPE)
        turbopipe.pipe(buffer, child.stdin.fileno())
        ```
    """
    if isinstance(buffer, Buffer):
        buffer = memoryview(buffer.mglo)
    _turbopipe.pipe(buffer, fileno)
    del buffer

def sync(buffer: Optional[Union[Buffer, memoryview]]=None) -> None:
    """Waits for any pending write operation on a buffer, or 'all buffers' if None, to finish"""
    if isinstance(buffer, Buffer):
        buffer = memoryview(buffer.mglo)
    _turbopipe.sync(buffer)
    del buffer

def close() -> None:
    """Syncs and deletes objects"""
    _turbopipe.close()

__all__ = [
    "pipe",
    "sync",
    "close"
]
