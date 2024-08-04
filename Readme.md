> [!IMPORTANT]
> <sub>Also check out [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow), where **TurboPipe** shines! 😉</sub>

<div align="center">
  <a href="https://brokensrc.dev/"><img src="https://raw.githubusercontent.com/BrokenSource/TurboPipe/main/turbopipe/resources/images/turbopipe.png" width="200"></a>
  <h1>TurboPipe</h1>
  <br>
  Faster <a href="https://github.com/moderngl/moderngl"><b>ModernGL</b></a> inter-process data transfers
</div>

<br>

# 🔥 Description

> TurboPipe speeds up sending raw bytes from `moderngl.Buffer` objects primarily to `FFmpeg` subprocess

The **optimizations** involved are:

- **Zero-copy**: Avoid unnecessary memory copies or allocation (intermediate `buffer.read()`)
- **C++**: The core of TurboPipe is written in C++ for speed, efficiency and low-level control
- **Chunks**: Write in chunks of 4096 bytes (RAM page size), so the hardware is happy
- **Threaded**:
    - Doesn't block Python code execution, allows to render next frame
    - Decouples the main thread from the I/O thread for performance

✅ Don't worry, there's proper **safety** in place. TurboPipe will block Python if a memory address is already queued for writing, and guarantees order of writes per file-descriptor. Just call `.sync()` when done 😉

<br>

# 📦 Installation

It couldn't be easier! Just install in your package manager:

```bash
pip install turbopipe
poetry add turbopipe
pdm add turbopipe
rye add turbopipe
```

<br>

# 🚀 Usage

See also the [**Examples**](https://github.com/BrokenSource/TurboPipe/tree/main/examples) folder for more controlled usage, and [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow/blob/main/ShaderFlow/Scene.py) usage of it!

```python
import subprocess
import moderngl
import turbopipe

# Create ModernGL objects
ctx = moderngl.create_standalone_context()
buffer = ctx.buffer(reserve=1920*1080*3)

# Make sure resolution, pixel format matches!
ffmpeg = subprocess.Popen(
    'ffmpeg -f rawvideo -pix_fmt rgb24 -s 1920x1080 -i - -f null -'.split(),
    stdin=subprocess.PIPE
)

# Rendering loop of yours
for _ in range(100):
    turbopipe.pipe(buffer, ffmpeg.stdin.fileno())

# Finalize writing
turbo.sync()
ffmpeg.stdin.close()
ffmpeg.wait()
```

<br>

# ⭐️ Benchmarks

> [!NOTE]
> **The tests conditions are as follows**:
> - The tests are the average of 3 runs to ensure consistency, with 3 GB of the same data being piped
> - The data is a random noise per-buffer between 128-135. So, multi-buffers runs are a noise video
> - All resolutions are wide-screen (16:9) and have 3 components (RGB) with 3 bytes per pixel (SDR)
> - Multi-buffer cycles through a list of buffer (eg. 1, 2, 3, 1, 2, 3... for 3-buffers)
> - All FFmpeg outputs are scrapped with `-f null -` to avoid any disk I/O bottlenecks
> - The `gain` column is the percentage increase over the standard method
> - When `x264` is Null, no encoding took place (passthrough)
> - The test cases emoji signifies:
>     - 🐢: Standard `ffmpeg.stdin.write(buffer.read())` on just the main thread, pure Python
>     - 🚀: Threaded `ffmpeg.stdin.write(buffer.read())` with a queue (similar to turbopipe)
>     - 🌀: The magic of `turbopipe.pipe(buffer, ffmpeg.stdin.fileno())`
>
> Also see [`benchmark.py`](https://github.com/BrokenSource/TurboPipe/blob/main/examples/benchmark.py) for the implementation

✅ Check out benchmarks in a couple of systems below:

<details>
    <summary><b>Desktop</b> • (AMD Ryzen 9 5900x) • (NVIDIA RTX 3060 12 GB) • (DDR4 2x32 GB 3200 MT/s) • (Arch Linux)</summary>
    <br>

| 720p | x264      |   Buffers | Framerate | Bandwidth   | Gain     |
|:----:|:----------|:---------:|----------:|------------:|---------:|
| 🐢   | Null      |         1 |   882 fps | 2.44 GB/s   |          |
| 🚀   | Null      |         1 |   793 fps | 2.19 GB/s   | -10.04%  |
| 🌀   | Null      |         1 |  1911 fps | 5.28 GB/s   | 116.70%  |
| 🐢   | Null      |         4 |   818 fps | 2.26 GB/s   |          |
| 🚀   | Null      |         4 |   684 fps | 1.89 GB/s   | -16.35%  |
| 🌀   | Null      |         4 |  1494 fps | 4.13 GB/s   | 82.73%   |
| 🐢   | ultrafast |         4 |   664 fps | 1.84 GB/s   |          |
| 🚀   | ultrafast |         4 |   635 fps | 1.76 GB/s   | -4.33%   |
| 🌀   | ultrafast |         4 |   869 fps | 2.40 GB/s   | 31.00%   |
| 🐢   | slow      |         4 |   204 fps | 0.57 GB/s   |          |
| 🚀   | slow      |         4 |   205 fps | 0.57 GB/s   | 0.58%    |
| 🌀   | slow      |         4 |   208 fps | 0.58 GB/s   | 2.22%    |

| 1080p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   385 fps | 2.40 GB/s   |         |
| 🚀    | Null      |         1 |   369 fps | 2.30 GB/s   | -3.91%  |
| 🌀    | Null      |         1 |   641 fps | 3.99 GB/s   | 66.54%  |
| 🐢    | Null      |         4 |   387 fps | 2.41 GB/s   |         |
| 🚀    | Null      |         4 |   359 fps | 2.23 GB/s   | -7.21%  |
| 🌀    | Null      |         4 |   632 fps | 3.93 GB/s   | 63.40%  |
| 🐢    | ultrafast |         4 |   272 fps | 1.70 GB/s   |         |
| 🚀    | ultrafast |         4 |   266 fps | 1.66 GB/s   | -2.14%  |
| 🌀    | ultrafast |         4 |   405 fps | 2.53 GB/s   | 49.24%  |
| 🐢    | slow      |         4 |   117 fps | 0.73 GB/s   |         |
| 🚀    | slow      |         4 |   122 fps | 0.76 GB/s   | 4.43%   |
| 🌀    | slow      |         4 |   124 fps | 0.77 GB/s   | 6.48%   |

| 1440p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   204 fps | 2.26 GB/s   |         |
| 🚀    | Null      |         1 |   241 fps | 2.67 GB/s   | 18.49%  |
| 🌀    | Null      |         1 |   297 fps | 3.29 GB/s   | 45.67%  |
| 🐢    | Null      |         4 |   230 fps | 2.54 GB/s   |         |
| 🚀    | Null      |         4 |   235 fps | 2.61 GB/s   | 2.52%   |
| 🌀    | Null      |         4 |   411 fps | 4.55 GB/s   | 78.97%  |
| 🐢    | ultrafast |         4 |   146 fps | 1.62 GB/s   |         |
| 🚀    | ultrafast |         4 |   153 fps | 1.70 GB/s   | 5.21%   |
| 🌀    | ultrafast |         4 |   216 fps | 2.39 GB/s   | 47.96%  |
| 🐢    | slow      |         4 |    73 fps | 0.82 GB/s   |         |
| 🚀    | slow      |         4 |    78 fps | 0.86 GB/s   | 7.06%   |
| 🌀    | slow      |         4 |    79 fps | 0.88 GB/s   | 9.27%   |

| 2160p | x264      |   Buffers | Framerate | Bandwidth   | Gain     |
|:-----:|:----------|:---------:|----------:|------------:|---------:|
| 🐢    | Null      |         1 |    81 fps | 2.03 GB/s   |          |
| 🚀    | Null      |         1 |   107 fps | 2.67 GB/s   | 32.26%   |
| 🌀    | Null      |         1 |   213 fps | 5.31 GB/s   | 163.47%  |
| 🐢    | Null      |         4 |    87 fps | 2.18 GB/s   |          |
| 🚀    | Null      |         4 |   109 fps | 2.72 GB/s   | 25.43%   |
| 🌀    | Null      |         4 |   212 fps | 5.28 GB/s   | 143.72%  |
| 🐢    | ultrafast |         4 |    59 fps | 1.48 GB/s   |          |
| 🚀    | ultrafast |         4 |    67 fps | 1.68 GB/s   | 14.46%   |
| 🌀    | ultrafast |         4 |    95 fps | 2.39 GB/s   | 62.66%   |
| 🐢    | slow      |         4 |    37 fps | 0.94 GB/s   |          |
| 🚀    | slow      |         4 |    43 fps | 1.07 GB/s   | 16.22%   |
| 🌀    | slow      |         4 |    44 fps | 1.11 GB/s   | 20.65%   |

</details>

<details>
    <summary><b>Desktop</b> • (AMD Ryzen 9 5900x) • (NVIDIA RTX 3060 12 GB) • (DDR4 2x32 GB 3200 MT/s) • (Windows 11)</summary>
    <br>
</details>

<br>

<div align="justify">

# 🌀 Conclusion

TurboPipe significantly increases the feeding speed of FFmpeg with data, especially at higher resolutions. However, if there's few CPU compute available, or the video is too hard to encode (slow preset), the gains are insignificant over the other methods (bottleneck). Multi-buffering didn't prove to have an advantage, debugging shows that TurboPipe C++ is often starved of data to write (as the file stream is buffered on the OS most likely), and the context switching, or cache misses, might be the cause of the slowdown.

Interestingly, due either Linux's scheduler on AMD Ryzen CPUs, or their operating philosophy, it was experimentally seen that Ryzen's frenetic thread switching degrades a bit the single thread performance, which can be _"fixed"_ with prepending the command with `taskset --cpu 0,2` (not recommended at all), comparatively speaking to Windows performance on the same system (Linux 🚀 = Windows 🐢). This can also be due the topology of tested CPUs having more than one Core Complex Die (CCD). Intel CPUs seem to stick to the same thread for longer, which makes the Python threaded method an unecessary overhead.

### Personal experience

On realistically loads, like [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow)'s default lightweight shader export, TurboPipe increases rendering speed from 1080p260 to 1080p330 on my system, with mid 80% CPU usage than low 60%s. For [**DepthFlow**](https://github.com/BrokenSource/ShaderFlow)'s default depth video export, no gains are seen, as the CPU is almost saturated encoding at 1080p130.

</div>

<br>

# 📚 Future work

- Add support for NumPy arrays, memoryviews, and byte-like objects
- Improve the thread synchronization and/or use a ThreadPool
- Maybe use `mmap` instead of chunks writing
- Test on MacOS 🙈
