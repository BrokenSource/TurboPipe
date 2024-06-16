import time

import moderngl
import turbopipe

ctx = moderngl.create_standalone_context()

buffer = ctx.buffer(reserve=1920*1080*3)

print(len(buffer.read()))

with open("test.turbo", "wb") as file:
    file.write(turbopipe.read(buffer, file.fileno()))
    # turbopipe.sync()
    time.sleep(1)

