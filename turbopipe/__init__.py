from turbopipe import _turbopipe

__about__   = "🌀 Faster ModernGL Buffers inter-process data transfers for subprocesses"
__package__ = "turbopipe"
__version__ = "2.0.0"
__license__ = "MIT"

def pipe(buffer: memoryview, file: int) -> None:
    _turbopipe.pipe(buffer, file)

def sync(buffer: memoryview | None=None) -> None:
    """Wait for pending operations on a buffer to finish"""
    _turbopipe.sync(buffer)

def close(file: int) -> None:
    """Syncs and deletes objects"""
    _turbopipe.close(file)
