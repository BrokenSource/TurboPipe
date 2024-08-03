from io import IOBase

from moderngl import Buffer

from turbopipe import _turbopipe


def pipe(buffer: Buffer, file: IOBase) -> None:
    """
    Pipe the content of a moderngl.Buffer to a file descriptor,
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
    _turbopipe.pipe(buffer.mglo, file)

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
