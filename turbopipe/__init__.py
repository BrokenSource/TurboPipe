from io import IOBase

import turbopipe_cpp
from moderngl import Buffer


def read(buffer: Buffer, target: IOBase, size: int=-1) -> None:
    if hasattr(target, "fileno"):
        target = target.fileno()
    turbopipe_cpp.read(buffer, target, size)

def sync(stop: bool=False) -> None:
    turbopipe_cpp.sync(stop)
