from typing import Any

from turbopipe._turbopipe import _done, _pipe, _stop, _sync

__about__   = "🌀 Fast data piping for python"
__package__ = "turbopipe"
__version__ = "2.1.0"
__license__ = "MIT"

# Actions

def pipe(data: Any, file: int) -> None:
    """Queue some data to be written into the file descriptor"""
    _pipe(data, file)

def sync(data: Any) -> None:
    """Wait for queued pipes in this buffer to finish"""
    _sync(data)

# Cleanup

def done(data: Any) -> None:
    """Signal this data won't be used again"""
    _done(data)
