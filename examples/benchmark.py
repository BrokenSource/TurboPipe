# python -m pip install pandas tqdm attrs tabulate
import contextlib
import enum
import subprocess
import time
from collections import deque
from queue import Queue
from threading import Thread
from typing import Optional

import attr
import moderngl
import numpy
import pandas
import tqdm

import turbopipe

ctx = moderngl.create_standalone_context()
print(f"OpenGL Renderer: {ctx.info['GL_RENDERER']}")

AVERAGE_N_RUNS = 3
DATA_SIZE_GB = 5

# -------------------------------------------------------------------------------------------------|

class Methods(enum.Enum):
    TRADITIONAL     = "🐢"
    PYTHON_THREADED = "🚀"
    TURBOPIPE       = "🌀"

# -------------------------------------------------------------------------------------------------|

@attr.s(auto_attribs=True)
class StdinWrapper:
    _process: subprocess.Popen
    _queue: Queue = attr.Factory(lambda: Queue(maxsize=10))
    _loop: bool = True
    _stdin: any = None

    def __attrs_post_init__(self):
        Thread(target=self.worker, daemon=True).start()

    def write(self, data):
        self._queue.put(data)

    def worker(self):
        while self._loop:
            self._stdin.write(self._queue.get())
            self._queue.task_done()

    def close(self):
        self._queue.join()
        self._stdin.close()
        self._loop = False
        while self._process.poll() is None:
            time.sleep(0.01)

# -------------------------------------------------------------------------------------------------|

@attr.s(auto_attribs=True)
class Statistics:
    BYTES_PER_FRAME: int

    def __attrs_post_init__(self):
        self.start_time = time.perf_counter()
        self.frametimes = deque()

    def next(self):
        now = time.perf_counter()
        self.frametimes.append(now - self.start_time)
        self.start_time = now

    @property
    def average_fps(self):
        return 1 / numpy.mean(self.frametimes)

    @property
    def std_fps(self):
        return numpy.std([1/t for t in self.frametimes])

    @property
    def average_bps(self):
        return self.BYTES_PER_FRAME * self.average_fps

    @property
    def std_bps(self):
        return self.BYTES_PER_FRAME * self.std_fps

# -------------------------------------------------------------------------------------------------|

HD  = (1280,  720)
FHD = (1920, 1080)
QHD = (2560, 1440)
UHD = (3840, 2160)

@attr.s(auto_attribs=True)
class Benchmark:
    table: pandas.DataFrame = attr.Factory(pandas.DataFrame)

    @contextlib.contextmanager
    def ffmpeg(self, width: int, height: int, x264_preset: Optional[str] = None):
        command = [
            "ffmpeg",
            "-hide_banner",
            "-loglevel", "error",
            "-f", "rawvideo",
            "-pix_fmt", "rgb24",
            "-s", f"{width}x{height}",
            "-r", "60",
            "-i", "-"
        ]

        command.extend([
            "-c:v", "libx264",
            "-preset", x264_preset,
        ] if x264_preset else [])

        command.extend(["-f", "null", "-"])

        try:
            process = subprocess.Popen(command, stdin=subprocess.PIPE)
            yield process
        finally:
            turbopipe.close()
            process.stdin.close()
            process.wait()

    def run(self):
        for (width, height) in (HD, FHD, QHD, UHD):
            for test_case in "ABCD":
                for index, method in enumerate(Methods):

                    # Selective x264 presets
                    if test_case == "C":
                        X264 = "ultrafast"
                    elif test_case == "D":
                        X264 = "slow"
                    else:
                        X264 = None

                    # Test constants for fairness
                    nbuffer = (4 if test_case in ["B", "C", "D"] else 1)
                    buffers = [ctx.buffer(data=numpy.random.randint(128, 135, (height, width, 3), dtype=numpy.uint8)) for _ in range(nbuffer)]
                    bytes_per_frame = (width * height * 3)
                    total_frames = int((DATA_SIZE_GB * 1024**3) / bytes_per_frame)
                    statistics = Statistics(bytes_per_frame)

                    for run in range(AVERAGE_N_RUNS):
                        statistics.start_time = time.perf_counter()

                        with self.ffmpeg(width, height, X264) as process:
                            if method == Methods.PYTHON_THREADED:
                                process.stdin = StdinWrapper(process=process, stdin=process.stdin)

                            for frame in tqdm.tqdm(
                                desc=f"Processing (Run #{run}) ({method.value} {width}x{height})",
                                iterable=range(total_frames), smoothing=0, unit=" Frame", mininterval=1/30,
                            ):
                                buffer = buffers[frame % nbuffer]

                                if method == Methods.TURBOPIPE:
                                    turbopipe.pipe(buffer, process.stdin.fileno())
                                else:
                                    process.stdin.write(buffer.read())

                                statistics.next()

                    for buffer in buffers:
                        buffer.release()

                    # Calculate the gain%
                    if index != 0:
                        baseline = float(self.table.iloc[-index]['Framerate'].split()[0])
                        gain = (float(statistics.average_fps) - baseline) / baseline * 100

                    self.table = self.table._append({
                        'Test': f"{method.value} {height}p",
                        'x264': (X264 or "Null"),
                        'Buffers': nbuffer,
                        'Framerate': f"{int(statistics.average_fps)} fps",
                        'Bandwidth': f"{statistics.average_bps / 1e9:.2f} GB/s",
                        'Gain': (f"{gain:.2f}%" if index != 0 else None)
                    }, ignore_index=True)

        print("\nBenchmark Results:")
        print(self.table.to_markdown(index=False))

# -------------------------------------------------------------------------------------------------|

if __name__ == "__main__":
    benchmark = Benchmark()
    benchmark.run()