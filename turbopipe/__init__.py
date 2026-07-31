from typing import Any

from turbopipe import _turbopipe

__about__   = "🌀 Fast memoryview data piping"
__package__ = "turbopipe"
__version__ = "2.0.1"
__license__ = "MIT"

def pipe(data: Any, file: int) -> None:
    """Queue some data to be written into the file descriptor"""
    _turbopipe.pipe(data, file)

def sync(data: Any) -> None:
    """Wait for queued pipes in this buffer to finish"""
    _turbopipe.sync(data)

def close(file: int) -> None:
    """Signals worker threads for this file descriptor to stop"""
    _turbopipe.close(file)
