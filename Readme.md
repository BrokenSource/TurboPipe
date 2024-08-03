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

See also the [**Examples**](examples) folder for more controlled usage, and [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow/blob/main/ShaderFlow/Scene.py) usage of it!

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
> Also see [`benchmark.py`](examples/benchmark.py) for the implementation

✅ Check out benchmarks in a couple of systems below:

<details>
    <summary><b>Desktop</b> • (AMD Ryzen 9 5900x) • (NVIDIA RTX 3060 12 GB) • (DDR4 2x32 GB 3200 MT/s) • (Arch Linux)</summary>
    <br>

| 720p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢   | Null      |         1 |   883 fps | 2.44 GB/s   |         |
| 🚀   | Null      |         1 |   843 fps | 2.33 GB/s   | -4.42%  |
| 🌀   | Null      |         1 |  1125 fps | 3.11 GB/s   | 27.49%  |
| 🐢   | Null      |         4 |   845 fps | 2.34 GB/s   |         |
| 🚀   | Null      |         4 |   773 fps | 2.14 GB/s   | -8.49%  |
| 🌀   | Null      |         4 |  1351 fps | 3.74 GB/s   | 59.82%  |
| 🐢   | ultrafast |         4 |   657 fps | 1.82 GB/s   |         |
| 🚀   | ultrafast |         4 |   642 fps | 1.78 GB/s   | -2.30%  |
| 🌀   | ultrafast |         4 |  1011 fps | 2.80 GB/s   | 53.68%  |
| 🐢   | slow      |         4 |   206 fps | 0.57 GB/s   |         |
| 🚀   | slow      |         4 |   207 fps | 0.57 GB/s   | 0.81%   |
| 🌀   | slow      |         4 |   211 fps | 0.58 GB/s   | 2.43%   |

| 1080p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   348 fps | 2.17 GB/s   |         |
| 🚀    | Null      |         1 |   390 fps | 2.43 GB/s   | 11.97%  |
| 🌀    | Null      |         1 |   732 fps | 4.56 GB/s   | 110.17% |
| 🐢    | Null      |         4 |   385 fps | 2.40 GB/s   |         |
| 🚀    | Null      |         4 |   383 fps | 2.39 GB/s   | -0.30%  |
| 🌀    | Null      |         4 |   696 fps | 4.33 GB/s   | 80.90%  |
| 🐢    | ultrafast |         4 |   282 fps | 1.76 GB/s   |         |
| 🚀    | ultrafast |         4 |   270 fps | 1.68 GB/s   | -4.18%  |
| 🌀    | ultrafast |         4 |   403 fps | 2.51 GB/s   | 42.81%  |
| 🐢    | slow      |         4 |   119 fps | 0.74 GB/s   |         |
| 🚀    | slow      |         4 |   123 fps | 0.77 GB/s   | 3.62%   |
| 🌀    | slow      |         4 |   123 fps | 0.77 GB/s   | 3.48%   |

| 1440p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |   166 fps | 1.84 GB/s   |         |
| 🚀    | Null      |         1 |   240 fps | 2.66 GB/s   | 44.24%  |
| 🌀    | Null      |         1 |   388 fps | 4.30 GB/s   | 133.05% |
| 🐢    | Null      |         4 |   226 fps | 2.51 GB/s   |         |
| 🚀    | Null      |         4 |   207 fps | 2.29 GB/s   | -8.61%  |
| 🌀    | Null      |         4 |   494 fps | 5.47 GB/s   | 117.82% |
| 🐢    | ultrafast |         4 |   145 fps | 1.61 GB/s   |         |
| 🚀    | ultrafast |         4 |   154 fps | 1.71 GB/s   | 5.93%   |
| 🌀    | ultrafast |         4 |   227 fps | 2.51 GB/s   | 55.76%  |
| 🐢    | slow      |         4 |    75 fps | 0.83 GB/s   |         |
| 🚀    | slow      |         4 |    78 fps | 0.87 GB/s   | 4.94%   |
| 🌀    | slow      |         4 |    80 fps | 0.90 GB/s   | 7.74%   |

| 2160p | x264      |   Buffers | Framerate | Bandwidth   | Gain    |
|:-----:|:----------|:---------:|----------:|------------:|--------:|
| 🐢    | Null      |         1 |    86 fps | 2.24 GB/s   |         |
| 🚀    | Null      |         1 |   111 fps | 2.88 GB/s   | 28.60%  |
| 🌀    | Null      |         1 |   210 fps | 5.44 GB/s   | 142.88% |
| 🐢    | Null      |         4 |    77 fps | 1.99 GB/s   |         |
| 🚀    | Null      |         4 |   108 fps | 2.80 GB/s   | 40.55%  |
| 🌀    | Null      |         4 |   211 fps | 5.44 GB/s   | 173.40% |
| 🐢    | ultrafast |         4 |    59 fps | 1.53 GB/s   |         |
| 🚀    | ultrafast |         4 |    69 fps | 1.80 GB/s   | 17.86%  |
| 🌀    | ultrafast |         4 |    97 fps | 2.51 GB/s   | 63.71%  |
| 🐢    | slow      |         4 |    38 fps | 0.98 GB/s   |         |
| 🚀    | slow      |         4 |    45 fps | 1.18 GB/s   | 20.31%  |
| 🌀    | slow      |         4 |    45 fps | 1.18 GB/s   | 19.63%  |

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
