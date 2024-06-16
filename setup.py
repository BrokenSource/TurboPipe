import platform

from setuptools import Extension, find_packages, setup

target = platform.system().lower()

extra_compile_args = {
    "windows": ["-fpermissive"],
    "linux": ["-fpermissive"],
    "cygwin": ["-fpermissive"],
    "darwin": ["-Wno-deprecated-declarations"],
    "android": ["-fpermissive"],
}

setup_args = dict(
    name="turbopipe",
    packages=find_packages(),
    ext_modules=[
        Extension(
            name="turbopipe_cpp",
            extra_compile_args=extra_compile_args[target],
            sources=["turbopipe/turbopipe.cpp"],
            include_dirs=["include"],
        )
    ]
)

setup(**setup_args)
