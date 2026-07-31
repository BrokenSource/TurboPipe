from turbopipe import _turbopipe

__about__   = "🌀 Fast memoryview data piping"
__package__ = "turbopipe"
__version__ = "2.0.0"
__license__ = "MIT"

def pipe(buffer: memoryview, file: int) -> None:
    """Queue some data to be written into the file descriptor"""
    _turbopipe.pipe(buffer, file)

def sync(buffer: memoryview) -> None:
    """Wait for queued pipes in this buffer to finish"""
    _turbopipe.sync(buffer)

def close(file: int) -> None:
    """Signals worker threads for this file descriptor to stop"""
    _turbopipe.close(file)
