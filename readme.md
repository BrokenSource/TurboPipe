<div align="center">
  <img src="https://raw.githubusercontent.com/BrokenSource/TurboPipe/main/website/assets/logo.png" width="200">
  <h1>TurboPipe</h1>
  Fast memoryview data piping
  <br><br>
  <a href="https://pypi.org/project/turbopipe/"><img src="https://img.shields.io/pypi/v/turbopipe?label=PyPI&color=blue"></a>
  <a href="https://pypi.org/project/turbopipe/"><img src="https://img.shields.io/pypi/dw/turbopipe?label=Installs&color=blue"></a>
  <a href="https://github.com/BrokenSource/TurboPipe/stargazers"><img src="https://img.shields.io/github/stars/BrokenSource/TurboPipe?label=Stars&style=flat&color=orange"></a>
  <a href="https://discord.gg/KjqvcYwRHm"><img src="https://img.shields.io/discord/1184696441298485370?label=Discord&style=flat&color=purple"></a>
</div>

## 🔥 Description

> TurboPipe speeds up sending raw bytes from `moderngl.Buffer` objects primarily to `FFmpeg` subprocess

The **optimizations** involved are:

- **Zero-copy**: Avoid unnecessary memory copies or allocation (intermediate `buffer.read()`)
- **Chunks**: Write in blocks of 8192 bytes (RAM page size), so the hardware is happy (Unix)
- **Threaded**:
  - Doesn't block the Python GIL, allows to render next frame
  - Decouples the main thread from the I/O thread for performance
- **Rust**: The core of TurboPipe is written in Rust for speed, efficiency and low-level control
- **Safe**: Guarantees order, blocks if the memory is queued on any pipe

<sub><b>Note</b>: Also check out [**ShaderFlow**](https://github.com/BrokenSource/ShaderFlow), where TurboPipe shines! 😉</sub>

<br>

## 📦 Installation

Simply add the [`turbopipe`](https://pypi.org/project/turbopipe/) PyPI package to your `pyproject.toml`:

```toml
[project]
dependencies = ["turbopipe"]
```

## 🚀 Usage

### Foundations

On its simplest form, the two are equivalent:

```python
# Whatever data you can get a memoryview
data = memoryview(os.urandom(1000))

with open("/dev/null", "wb") as stream:

    # Native python method (sync)
    stream.write(data)

    # Fast turbopipe method (async)
    turbopipe.pipe(data, stream.fileno())

# Wait for queued writes
turbopipe.sync(data)
```

Alternatively, for subprocesses:

```python
from subprocess import PIPE, Popen

# Must have stdin or named pipes
process = subprocess.Popen(
    ("sh", "-c", "cat > /dev/null"]),
    stdin=PIPE,
)

# Faster than stdin.write(data)
turbopipe.pipe(data, process.stdin.fileno())
```

### ModernGL

Framebuffers expose their data with the internal `.mglo` object:

```python
import moderngl

ctx = moderngl.create_standalone_context()
buf = ctx.buffer(reserve=1000)

# Send to FFmpeg, named pipes, raw data files
turbopipe.pipe(memoryview(buf.mglo), fileno)
```

However, TurboPipe shines in large data transfers for video encoding:

```python
# Pseudocode for a video editor-like
buffer = ctx.buffer(reserve=width*height*3)
scene = Scene()

ffmpeg = subprocess.Popen(...)
fileno = ffmpeg.stdin.fileno()

while not scene.finished:
    scene.render_frame()

    # Waits on all pending pipes in this buffer
    turbopipe.sync(memoryview(buffer.mglo))

    # Copy data so the next frame can be pre-rendered
    scene.fbo.read_into(buffer)

    # Queue the write into a worker thread
    turbopipe.pipe(memoryview(buffer.mglo, fileno))

# Sync all buffers, cleanup, etc.
turbopipe.sync(memoryview(buffer.mglo))
ffmpeg.stdin.close()
```

See the [<kbd>examples</kbd>](https://github.com/BrokenSource/TurboPipe/tree/main/examples) directory for more, and [ShaderFlow's](https://github.com/BrokenSource/ShaderFlow/blob/main/shaderflow/exporting.py) usage of it!

## Future work

_Design is compromise:_

- Split crate into a pure-rust and pyo3 bindings.
- Are eternal workers heavy on scheduling resources? [^eternal]
- Support untracked writes without waitgroup overhead. [^untracked]
- Store `Py_buffer` in `Work` and release in the worker (correctness).
- Support synchronizing all queued writes in a file descriptor. [^sync-all]

[^eternal]: Stopping workers midway has non-trivial concurrency problems, like needing a new WaitGroup per file descriptor to block `.pipe()` creating a new thread while due exiting (unecessary overhead).

[^untracked]: Simple to implement, but most realistic usage needs to sync at some point, and reutilize buffers. Strong argument is to not create an ephemeral waitgroup overhead.

[^sync-all]: Nice for minimal code or rotating/untrackable/dangling data sources, however the decision was _know your data-first_, controlling the truth for pipes and syncs.
