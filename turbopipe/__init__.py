from io import IOBase

from moderngl import Buffer

from turbopipe import _turbopipe


def pipe(buffer: Buffer, file: IOBase) -> None:
    if hasattr(file, "fileno"):
        file = file.fileno()
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
