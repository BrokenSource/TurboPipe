# ----------------------------------------------|

project('turbopipe', 'cpp',
    version: run_command(
        ['turbopipe/version.py'],
        check: true
    ).stdout().strip(),

    default_options: [
        'warning_level=3',
        'cpp_std=c++20',
        'buildtype=release',
        'optimization=3'
    ]
)

message('C++ compiler: ' + meson.get_compiler('cpp').get_id())

# ----------------------------------------------|
# Platform flags

cpp_args = ['-fpermissive']

if host_machine.system() == 'darwin'
    cpp_args += ['-Wno-deprecated-declarations']
endif

# ----------------------------------------------|
# Source files

source = files('turbopipe/_turbopipe.cpp')

# ----------------------------------------------|

python = import('python').find_installation()

python.extension_module(
    '_turbopipe', source,
    cpp_args: cpp_args,
    install: true,
    subdir: 'turbopipe'
)

python.install_sources(
    ['turbopipe/__init__.py'],
    subdir: 'turbopipe',
    pure: false
)

# ----------------------------------------------|
