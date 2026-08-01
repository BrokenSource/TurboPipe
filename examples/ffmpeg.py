import contextlib
import os
import subprocess
from collections.abc import Generator

import moderngl
import turbopipe
from tqdm import tqdm

# User constants
WIDTH, HEIGHT = (1920, 1080)
FRAMERATE = 60
DURATION = 60
NBUFFER = 1

# Calculate constants
BYTES_PER_FRAME = (WIDTH * HEIGHT * 3)
TOTAL_FRAMES = (DURATION * FRAMERATE)
TOTAL_BYTES = (BYTES_PER_FRAME * TOTAL_FRAMES)

# Create ModernGL objects
ctx = moderngl.create_standalone_context()
print("OpenGL Renderer:", ctx.info["GL_RENDERER"])

buffers = []

# Avoid any optimizations on static data
for _ in range(NBUFFER):
    this = ctx.buffer(os.urandom(BYTES_PER_FRAME))
    buffers.append(this)

@contextlib.contextmanager
def FFmpeg() -> Generator[subprocess.Popen, None, None]:

    # Noop passthrough for raw speed
    ffmpeg = subprocess.Popen([
        "ffmpeg",
        "-hide_banner",
        "-loglevel", "error",
        "-f", "rawvideo",
        "-pix_fmt", "rgb24",
        "-s", f"{WIDTH}x{HEIGHT}",
        "-r", str(FRAMERATE),
        "-i", "-",
        "-f", "null",
        "-", "-y"
    ], stdin=subprocess.PIPE)

    yield ffmpeg

with FFmpeg() as ffmpeg:
    assert ffmpeg.stdin is not None
    fileno = ffmpeg.stdin.fileno()

    for frame in tqdm(
        iterable=range(TOTAL_FRAMES),
        mininterval=1/30,
        maxinterval=1/30,
        smoothing=0,
    ):
        buffer = buffers[frame % NBUFFER]
        turbopipe.sync(buffer.mglo)
        turbopipe.pipe(buffer.mglo, fileno)

    for buffer in buffers:
        turbopipe.sync(buffer.mglo)
        turbopipe.done(buffer.mglo)

    ffmpeg.stdin.close()
    ffmpeg.wait()
