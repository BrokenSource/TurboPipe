import contextlib
import subprocess
from typing import Generator

import moderngl
import tqdm
import turbopipe

# User constants
WIDTH, HEIGHT = 1920, 1080
FRAMERATE = 60
DURATION = 60

# Calculate constants
BYTES_PER_FRAME = (WIDTH * HEIGHT * 3)
TOTAL_FRAMES = (DURATION * FRAMERATE)
TOTAL_BYTES = (BYTES_PER_FRAME * TOTAL_FRAMES)

# Create ModernGL objects
ctx = moderngl.create_standalone_context()
buffer = ctx.buffer(reserve=BYTES_PER_FRAME)
print(len(buffer.read()))

# -------------------------------------------------------------------------------------------------|

@contextlib.contextmanager
def FFmpeg() -> Generator[subprocess.Popen, None, None]:
    try:
        ffmpeg = subprocess.Popen([
            "nice", "-20", "ffmpeg",
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
    finally:
        ffmpeg.stdin.close()
        ffmpeg.wait()

# -------------------------------------------------------------------------------------------------|

@contextlib.contextmanager
def Progress():
    with tqdm.tqdm(total=TOTAL_FRAMES, unit="Frame", smoothing=0) as frame_bar, \
         tqdm.tqdm(total=TOTAL_BYTES,  unit="B", smoothing=0, unit_scale=True) as byte_bar:
        def next():
            byte_bar.update(BYTES_PER_FRAME)
            frame_bar.update(1)
        yield next

# -------------------------------------------------------------------------------------------------|

print("\n::Traditional method\n")

with Progress() as progress, FFmpeg() as ffmpeg:
    for frame in range(TOTAL_FRAMES):
        ffmpeg.stdin.write(buffer.read())
        progress()
    turbopipe.sync()


print("\n:: TurboPipe method\n")

with Progress() as progress, FFmpeg() as ffmpeg:
    for frame in range(TOTAL_FRAMES):
        turbopipe.pipe(buffer, ffmpeg.stdin)
        progress()
    turbopipe.sync()

turbopipe.close()
