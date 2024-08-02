import platform

from setuptools import Extension, find_packages, setup

target = platform.system().lower()
extra_compile_args = dict(
    windows = ["-fpermissive"],
    linux   = ["-fpermissive"],
    darwin  = ["-fpermissive", "-Wno-deprecated-declarations"]
)

setup(
    name="turbopipe",
    packages=find_packages(),
    ext_modules=[
        Extension(
            "turbopipe._turbopipe",
            sources=["turbopipe/_turbopipe.cpp"],
            include_dirs=["include"],
            extra_compile_args=extra_compile_args.get(target, []),
            language="c++"
        )
    ],
)