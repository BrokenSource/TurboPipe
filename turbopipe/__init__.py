from typing import Union

from moderngl import Buffer

from turbopipe import _turbopipe


def pipe(buffer: Union[Buffer, memoryview], fileno: int) -> None:
    """
    Pipe the content of a moderngl.Buffer or memoryview to a file descriptor,
    Fast, threaded and non-blocking. Call `sync()` when done!

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

def sync() -> None:
    """Waits for all jobs to finish"""
    _turbopipe.sync()

def close() -> None:
    """Syncs and deletes objects"""
    _turbopipe.close()

__all__ = [
    "pipe",
    "sync",
    "close"
]
