<div align="center">
  <img src="https://raw.githubusercontent.com/BrokenSource/TurboPipe/main/turbopipe/resources/images/turbopipe.png" width="200">
  <h1>TurboPipe</h1>
  Faster <a href="https://github.com/moderngl/moderngl"><b>ModernGL Buffers</b></a> inter-process data transfers for subprocesses
  <br>
  <br>
  <a href="https://pypi.org/project/turbopipe/"><img src="https://img.shields.io/pypi/v/turbopipe?label=PyPI&color=blue"></a>
  <a href="https://pypi.org/project/turbopipe/"><img src="https://img.shields.io/pypi/dw/turbopipe?label=Installs&color=blue"></a>
  <a href="https://github.com/BrokenSource/TurboPipe"><img src="https://img.shields.io/github/v/tag/BrokenSource/TurboPipe?label=GitHub&color=orange"></a>
  <a href="https://github.com/BrokenSource/TurboPipe/stargazers"><img src="https://img.shields.io/github/stars/BrokenSource/TurboPipe?label=Stars&style=flat&color=orange"></a>
  <a href="https://discord.gg/KjqvcYwRHm"><img src="https://img.shields.io/discord/1184696441298485370?label=Discord&style=flat&color=purple"></a>
</div>

<br>

# 🔥 Description

> TurboPipe speeds up sending raw bytes from `moderngl.Buffer` objects primarily to `FFmpeg` subprocess

The **optimizations** involved are:

- **Zero-copy**: Avoid unnecessary memory copies or allocation (intermediate `buffer.read`)
- **C++**: The core of TurboPipe is written in C++ for speed, efficiency and low-level control
- **Threaded**:
    - Doesn't block Python code execution, allows to render next frame
    - Decouples the main thread from the I/O thread for performance
- **Chunks**: Write in chunks of 4096 bytes (RAM page size), so the hardware is happy (Unix)

✅ Don't worry, there's proper **safety** in place. TurboPipe will block Python if a memory address is already queued for writing, and guarantees order of writes per file-descriptor. Just call `.sync()` when done 😉

<sub>Also check out [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow), where **TurboPipe** shines! 😉</sub>

<br>

# 📦 Installation

It couldn't be easier! Just install the [**`turbopipe`**](https://pypi.org/project/turbopipe/) package from PyPI:

```bash
# With pip (https://pip.pypa.io/)
pip install turbopipe

# With Poetry (https://python-poetry.org/)
poetry add turbopipe

# With PDM (https://pdm-project.org/en/latest/)
pdm add turbopipe

# With Rye (https://rye.astral.sh/)
rye add turbopipe
```

<br>

# 🚀 Usage

See also the [**Examples**](https://github.com/BrokenSource/TurboPipe/tree/main/examples) folder for comparisons, and [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow/blob/main/ShaderFlow/Exporting.py)'s usage of it!

```python
import subprocess

import moderngl
import turbopipe

# Create ModernGL objects and proxy buffers
ctx = moderngl.create_standalone_context()
width, height, duration, fps = (1920, 1080, 10, 60)
buffers = [
    ctx.buffer(reserve=(width*height*3))
    for _ in range(nbuffers := 2)
]

# Create your FBO, Textures, Shaders, etc.

# Make sure resolution, pixel format matches!
ffmpeg = subprocess.Popen((
    "ffmpeg",
    "-f", "rawvideo",
    "-pix_fmt", "rgb24",
    "-r", str(fps),
    "-s", f"{width}x{height}",
    "-i", "-",
    "-f", "null",
    "output.mp4"
), stdin=subprocess.PIPE)

# Rendering loop of yours
for frame in range(duration*fps):
    buffer = buffers[frame % nbuffers]

    # Wait queued writes before copying
    turbopipe.sync(buffer)
    fbo.read_into(buffer)

    # Doesn't lock the GIL, writes in parallel
    turbopipe.pipe(buffer, ffmpeg.stdin.fileno())

# Wait for queued writes, clean memory
for buffer in buffers:
    turbopipe.sync(buffer)
    buffer.release()

# Signal stdin stream is done
ffmpeg.stdin.close()

# wait for encoding to finish
ffmpeg.wait()

# Warn: Albeit rare, only call close when no other data
# write is pending, as it might skip a frame or halt
turbopipe.close()
```

<br>

# ⭐️ Benchmarks

> [!NOTE]
> **The tests conditions are as follows**:
> - The tests are the average of 3 runs to ensure consistency, with 5 GB of the same data being piped
> - These aren't tests of render speed; but rather the throughput speed of GPU -> CPU -> RAM -> IPC
> - All resolutions are wide-screen (16:9) and have 3 components (RGB) with 3 bytes per pixel (SDR)
> - The data is a random noise per-buffer between 128-135. So, multi-buffers runs are a noise video
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

📦 TurboPipe v1.0.4:

<details>
    <summary><b>Desktop</b> • (AMD Ryzen 9 5900x) • (NVIDIA RTX 3060 12 GB) • (DDR4 2x32 GB 3200 MT/s) • (Arch Linux)</summary>
    <br>

<b>Note:</b> I have noted inconsistency across tests, specially at lower resolutions. Some 720p runs might peak at 2900 fps and stay there, while others are limited by 1750 fps. I'm not sure if it's the Linux EEVDF scheduler, or CPU Topology that causes this. Nevertheless, results are stable on Windows 11 on the same machine.

| 720p | x264      |   Buffers | Framerate | Bandwidth   | Gain     |
|:----:|:----------|:---------:|----------:|------------:|---------:|
| 🐢   | Null      |         1 |   882 fps | 2.44 GB/s   |          |
| 🚀   | Null      |         1 |   793 fps | 2.19 GB/s   | -10.04%  |
| 🌀   | Null      |         1 |  1911 fps | 5.28 GB/s   | 116.70%  |
| 🐢   | Null      |         4 |   857 fps | 2.37 GB/s   |          |
| 🚀   | Null      |         4 |   891 fps | 2.47 GB/s   | 4.05%    |
| 🌀   | Null      |         4 |  2309 fps | 6.38 GB/s   | 169.45%  |
| 🐢   | ultrafast |         4 |   714 fps | 1.98 GB/s   |          |
| 🚀   | ultrafast |         4 |   670 fps | 1.85 GB/s   | -6.10%   |
| 🌀   | ultrafast |         4 |  1093 fps | 3.02 GB/s   | 53.13%   |
| 🐢   | slow      |         4 |   206 fps | 0.57 GB/s   |          |
| 🚀   | slow      |         4 |   208 fps | 0.58 GB/s   | 1.37%    |
| 🌀   | slow      |         4 |   214 fps | 0.59 GB/s   | 3.93%    |

| 1080p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   410 fps | 2.55 GB/s   |         |
| 🚀    | Null      |         1 |   399 fps | 2.48 GB/s   | -2.60%  |
| 🌀    | Null      |         1 |   794 fps | 4.94 GB/s   | 93.80%  |
| 🐢    | Null      |         4 |   390 fps | 2.43 GB/s   |         |
| 🚀    | Null      |         4 |   391 fps | 2.43 GB/s   | 0.26%   |
| 🌀    | Null      |         4 |   756 fps | 4.71 GB/s   | 94.01%  |
| 🐢    | ultrafast |         4 |   269 fps | 1.68 GB/s   |         |
| 🚀    | ultrafast |         4 |   272 fps | 1.70 GB/s   | 1.48%   |
| 🌀    | ultrafast |         4 |   409 fps | 2.55 GB/s   | 52.29%  |
| 🐢    | slow      |         4 |   115 fps | 0.72 GB/s   |         |
| 🚀    | slow      |         4 |   118 fps | 0.74 GB/s   | 3.40%   |
| 🌀    | slow      |         4 |   119 fps | 0.75 GB/s   | 4.34%   |

| 1440p | x264      |   Buffers | Framerate | Bandwidth   | Gain     |
|:-----:|:----------|:---------:|----------:|------------:|---------:|
| 🐢    | Null      |         1 |   210 fps | 2.33 GB/s   |          |
| 🚀    | Null      |         1 |   239 fps | 2.64 GB/s   | 13.84%   |
| 🌀    | Null      |         1 |   534 fps | 5.91 GB/s   | 154.32%  |
| 🐢    | Null      |         4 |   219 fps | 2.43 GB/s   |          |
| 🚀    | Null      |         4 |   231 fps | 2.56 GB/s   | 5.64%    |
| 🌀    | Null      |         4 |   503 fps | 5.56 GB/s   | 129.75%  |
| 🐢    | ultrafast |         4 |   141 fps | 1.56 GB/s   |          |
| 🚀    | ultrafast |         4 |   150 fps | 1.67 GB/s   | 6.92%    |
| 🌀    | ultrafast |         4 |   226 fps | 2.50 GB/s   | 60.37%   |
| 🐢    | slow      |         4 |    72 fps | 0.80 GB/s   |          |
| 🚀    | slow      |         4 |    71 fps | 0.79 GB/s   | -0.70%   |
| 🌀    | slow      |         4 |    75 fps | 0.83 GB/s   | 4.60%    |

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

| 720p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢   | Null      |         1 |   981 fps | 2.71 GB/s   |         |
| 🚀   | Null      |         1 |  1145 fps | 3.17 GB/s   | 16.74%  |
| 🌀   | Null      |         1 |  1504 fps | 4.16 GB/s   | 53.38%  |
| 🐢   | Null      |         4 |   997 fps | 2.76 GB/s   |         |
| 🚀   | Null      |         4 |  1117 fps | 3.09 GB/s   | 12.08%  |
| 🌀   | Null      |         4 |  1467 fps | 4.06 GB/s   | 47.14%  |
| 🐢   | ultrafast |         4 |   601 fps | 1.66 GB/s   |         |
| 🚀   | ultrafast |         4 |   616 fps | 1.70 GB/s   | 2.57%   |
| 🌀   | ultrafast |         4 |   721 fps | 1.99 GB/s   | 20.04%  |
| 🐢   | slow      |         4 |   206 fps | 0.57 GB/s   |         |
| 🚀   | slow      |         4 |   206 fps | 0.57 GB/s   | 0.39%   |
| 🌀   | slow      |         4 |   206 fps | 0.57 GB/s   | 0.13%   |

| 1080p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   451 fps | 2.81 GB/s   |         |
| 🚀    | Null      |         1 |   542 fps | 3.38 GB/s   | 20.31%  |
| 🌀    | Null      |         1 |   711 fps | 4.43 GB/s   | 57.86%  |
| 🐢    | Null      |         4 |   449 fps | 2.79 GB/s   |         |
| 🚀    | Null      |         4 |   518 fps | 3.23 GB/s   | 15.48%  |
| 🌀    | Null      |         4 |   614 fps | 3.82 GB/s   | 36.83%  |
| 🐢    | ultrafast |         4 |   262 fps | 1.64 GB/s   |         |
| 🚀    | ultrafast |         4 |   266 fps | 1.66 GB/s   | 1.57%   |
| 🌀    | ultrafast |         4 |   319 fps | 1.99 GB/s   | 21.88%  |
| 🐢    | slow      |         4 |   119 fps | 0.74 GB/s   |         |
| 🚀    | slow      |         4 |   121 fps | 0.76 GB/s   | 2.46%   |
| 🌀    | slow      |         4 |   121 fps | 0.75 GB/s   | 1.90%   |

| 1440p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   266 fps | 2.95 GB/s   |         |
| 🚀    | Null      |         1 |   308 fps | 3.41 GB/s   | 15.87%  |
| 🌀    | Null      |         1 |   402 fps | 4.45 GB/s   | 51.22%  |
| 🐢    | Null      |         4 |   276 fps | 3.06 GB/s   |         |
| 🚀    | Null      |         4 |   307 fps | 3.40 GB/s   | 11.32%  |
| 🌀    | Null      |         4 |   427 fps | 4.73 GB/s   | 54.86%  |
| 🐢    | ultrafast |         4 |   152 fps | 1.68 GB/s   |         |
| 🚀    | ultrafast |         4 |   156 fps | 1.73 GB/s   | 3.02%   |
| 🌀    | ultrafast |         4 |   181 fps | 2.01 GB/s   | 19.36%  |
| 🐢    | slow      |         4 |    77 fps | 0.86 GB/s   |         |
| 🚀    | slow      |         4 |    79 fps | 0.88 GB/s   | 3.27%   |
| 🌀    | slow      |         4 |    80 fps | 0.89 GB/s   | 4.86%   |

| 2160p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   134 fps | 3.35 GB/s   |         |
| 🚀    | Null      |         1 |   152 fps | 3.81 GB/s   | 14.15%  |
| 🌀    | Null      |         1 |   221 fps | 5.52 GB/s   | 65.44%  |
| 🐢    | Null      |         4 |   135 fps | 3.36 GB/s   |         |
| 🚀    | Null      |         4 |   151 fps | 3.76 GB/s   | 11.89%  |
| 🌀    | Null      |         4 |   220 fps | 5.49 GB/s   | 63.34%  |
| 🐢    | ultrafast |         4 |    66 fps | 1.65 GB/s   |         |
| 🚀    | ultrafast |         4 |    70 fps | 1.75 GB/s   | 6.44%   |
| 🌀    | ultrafast |         4 |    82 fps | 2.04 GB/s   | 24.31%  |
| 🐢    | slow      |         4 |    40 fps | 1.01 GB/s   |         |
| 🚀    | slow      |         4 |    43 fps | 1.09 GB/s   | 9.54%   |
| 🌀    | slow      |         4 |    44 fps | 1.10 GB/s   | 10.15%  |

</details>

<details>
    <summary><b>Laptop</b> • (Intel Core i7 11800H) • (NVIDIA RTX 3070) • (DDR4 2x16 GB 3200 MT/s) • (Windows 11)</summary>
    <br>

<b>Note:</b> Must select NVIDIA GPU on their Control Panel instead of Intel iGPU

| 720p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢   | Null      |         1 |   786 fps | 2.17 GB/s   |         |
| 🚀   | Null      |         1 |   903 fps | 2.50 GB/s   | 14.91%  |
| 🌀   | Null      |         1 |  1366 fps | 3.78 GB/s   | 73.90%  |
| 🐢   | Null      |         4 |   739 fps | 2.04 GB/s   |         |
| 🚀   | Null      |         4 |   855 fps | 2.37 GB/s   | 15.78%  |
| 🌀   | Null      |         4 |  1240 fps | 3.43 GB/s   | 67.91%  |
| 🐢   | ultrafast |         4 |   484 fps | 1.34 GB/s   |         |
| 🚀   | ultrafast |         4 |   503 fps | 1.39 GB/s   | 4.10%   |
| 🌀   | ultrafast |         4 |   577 fps | 1.60 GB/s   | 19.37%  |
| 🐢   | slow      |         4 |   143 fps | 0.40 GB/s   |         |
| 🚀   | slow      |         4 |   145 fps | 0.40 GB/s   | 1.78%   |
| 🌀   | slow      |         4 |   151 fps | 0.42 GB/s   | 5.76%   |

| 1080p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   358 fps | 2.23 GB/s   |         |
| 🚀    | Null      |         1 |   427 fps | 2.66 GB/s   | 19.45%  |
| 🌀    | Null      |         1 |   566 fps | 3.53 GB/s   | 58.31%  |
| 🐢    | Null      |         4 |   343 fps | 2.14 GB/s   |         |
| 🚀    | Null      |         4 |   404 fps | 2.51 GB/s   | 17.86%  |
| 🌀    | Null      |         4 |   465 fps | 2.89 GB/s   | 35.62%  |
| 🐢    | ultrafast |         4 |   191 fps | 1.19 GB/s   |         |
| 🚀    | ultrafast |         4 |   207 fps | 1.29 GB/s   | 8.89%   |
| 🌀    | ultrafast |         4 |   234 fps | 1.46 GB/s   | 22.77%  |
| 🐢    | slow      |         4 |    62 fps | 0.39 GB/s   |         |
| 🚀    | slow      |         4 |    67 fps | 0.42 GB/s   | 8.40%   |
| 🌀    | slow      |         4 |    74 fps | 0.47 GB/s   | 20.89%  |

| 1440p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   180 fps | 1.99 GB/s   |         |
| 🚀    | Null      |         1 |   216 fps | 2.40 GB/s   | 20.34%  |
| 🌀    | Null      |         1 |   264 fps | 2.92 GB/s   | 46.74%  |
| 🐢    | Null      |         4 |   178 fps | 1.97 GB/s   |         |
| 🚀    | Null      |         4 |   211 fps | 2.34 GB/s   | 19.07%  |
| 🌀    | Null      |         4 |   250 fps | 2.77 GB/s   | 40.48%  |
| 🐢    | ultrafast |         4 |    98 fps | 1.09 GB/s   |         |
| 🚀    | ultrafast |         4 |   110 fps | 1.23 GB/s   | 13.18%  |
| 🌀    | ultrafast |         4 |   121 fps | 1.35 GB/s   | 24.15%  |
| 🐢    | slow      |         4 |    40 fps | 0.45 GB/s   |         |
| 🚀    | slow      |         4 |    41 fps | 0.46 GB/s   | 4.90%   |
| 🌀    | slow      |         4 |    43 fps | 0.48 GB/s   | 7.89%   |

| 2160p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |    79 fps | 1.98 GB/s   |         |
| 🚀    | Null      |         1 |    95 fps | 2.37 GB/s   | 20.52%  |
| 🌀    | Null      |         1 |   104 fps | 2.60 GB/s   | 32.15%  |
| 🐢    | Null      |         4 |    80 fps | 2.00 GB/s   |         |
| 🚀    | Null      |         4 |    94 fps | 2.35 GB/s   | 17.82%  |
| 🌀    | Null      |         4 |   108 fps | 2.70 GB/s   | 35.40%  |
| 🐢    | ultrafast |         4 |    41 fps | 1.04 GB/s   |         |
| 🚀    | ultrafast |         4 |    48 fps | 1.20 GB/s   | 17.67%  |
| 🌀    | ultrafast |         4 |    52 fps | 1.30 GB/s   | 27.49%  |
| 🐢    | slow      |         4 |    17 fps | 0.43 GB/s   |         |
| 🚀    | slow      |         4 |    19 fps | 0.48 GB/s   | 13.16%  |
| 🌀    | slow      |         4 |    19 fps | 0.48 GB/s   | 13.78%  |

</details>
<br>

<div align="justify">

# 🌀 Conclusion

TurboPipe significantly increases the feeding speed of FFmpeg with data, especially at higher resolutions. However, if there's few CPU compute available, or the video is too hard to encode (/slow preset), the gains are insignificant over the other methods (bottleneck). Multi-buffering didn't prove to have an advantage, debugging shows that TurboPipe C++ is often starved of data to write (as the file stream is buffered on the OS most likely), and the context switching, or cache misses, might be the cause of the slowdown.

The theory supports the threaded method being faster, as writing to a file descriptor is a blocking operation for python, but a syscall under the hood, that doesn't necessarily lock the GIL, just the thread. TurboPipe speeds that even further by avoiding an unecessary copy of the buffer data, and writing directly to the file descriptor on a C++ thread. Linux shows a better performance than Windows in the same system after the optimizations, but Windows wins on the standard method.

Interestingly, due either Linux's scheduler on AMD Ryzen CPUs, or their operating philosophy, it was experimentally seen that Ryzen's frenetic thread switching degrades a bit the single thread performance, which can be _"fixed"_ with prepending the command with `taskset --cpu 0,2` (not recommended at all), comparatively speaking to Windows performance on the same system (Linux 🚀 = Windows 🐢). This can also be due the topology of tested CPUs having more than one Core Complex Die (CCD). Intel CPUs seem to stick to the same thread for longer, which makes the Python threaded method often slightly faster.

### Personal experience

On realistically loads, like [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow)'s default lightweight shader export, TurboPipe increases rendering speed from 1080p260 to 1080p360 on my system, with mid 80% CPU usage than low 60%s. For [**DepthFlow**](https://github.com/BrokenSource/ShaderFlow)'s default depth video export, no gains are seen, as the CPU is almost saturated encoding at 1080p130.

</div>

<br>

# 📚 Future work

- Disable/investigate performance degradation on Windows iGPUs
- Maybe use `mmap` instead of chunks writing on Linux
- Split the code into a libturbopipe? Not sure where it would be useful 😅
