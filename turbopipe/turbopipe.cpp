#include <Python.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <iostream>

#include "gl_methods.hpp"

static std::thread worker_thread;
static int frame_fd = -1;
static void *frame_data = nullptr;
static size_t frame_size = 0;
static std::mutex frame_mutex;
static std::condition_variable frame_cv;
static bool worker_thread_running = false;

void worker_thread_function() {
    std::unique_lock<std::mutex> lock(frame_mutex);
    while (worker_thread_running) {
        frame_cv.wait(lock, [] { return frame_data != nullptr || !worker_thread_running; });

        if (frame_data != nullptr) {
            void *data = frame_data;
            size_t size = frame_size;
            int fd = frame_fd;

            // Reset to indicate no work
            frame_data = nullptr;
            lock.unlock();

            size_t total_written = 0;
            while (total_written < size) {
                size_t chunk_size = std::min(size - total_written, (size_t) 4096);
                ssize_t written = write(fd, (char *)data + total_written, chunk_size);
                if (written == -1) {
                    // Handle error if needed
                    break;
                } else {
                    total_written += written;
                }
            }
            free(data);

            // Notify work is done
            lock.lock();
            frame_cv.notify_all();
        }
    }
}

struct MGLContext;
struct MGLFramebuffer;

struct MGLBuffer {
    PyObject_HEAD
    MGLContext* context;
    int buffer_obj;
    Py_ssize_t size;
    bool dynamic;
    bool released;
};

struct MGLContext {
    PyObject_HEAD
    PyObject * ctx;
    PyObject * extensions;
    MGLFramebuffer * default_framebuffer;
    MGLFramebuffer * bound_framebuffer;
    PyObject * includes;
    int version_code;
    int max_samples;
    int max_integer_samples;
    int max_color_attachments;
    int max_texture_units;
    int default_texture_unit;
    float max_anisotropy;
    int enable_flags;
    int front_face;
    int cull_face;
    int depth_func;
    bool depth_clamp;
    double depth_range[2];
    int blend_func_src;
    int blend_func_dst;
    bool wireframe;
    bool multisample;
    int provoking_vertex;
    float polygon_offset_factor;
    float polygon_offset_units;
    GLMethods gl;
    bool released;
};

PyObject* turbopipe_read(MGLBuffer* buffer, PyObject* args) {
    PyObject* target;
    Py_ssize_t size;

    int args_ok = PyArg_ParseTuple(
        args,
        "OO|n",
        &buffer,
        &target,
        &size
    );

    if (!args_ok) {
        return 0;
    }

    if (!PyLong_Check(target)) {
        PyErr_SetString(PyExc_TypeError, "target must be a file descriptor (integer)");
        return NULL;
    }

    if (size == -1) {
        size = buffer->size;
    }

    int fd = PyLong_AsLong(target);
    if (fd == -1 && PyErr_Occurred()) {return NULL;}

    std::cout << "Buffer Object: " << buffer->buffer_obj << std::endl;

    const GLMethods & gl = buffer->context->gl;
    gl.BindBuffer(GL_ARRAY_BUFFER, buffer->buffer_obj);

    void *map = gl.MapBufferRange(GL_ARRAY_BUFFER, 0, size, GL_MAP_READ_BIT);

    /* Send a work to write */ {
        std::unique_lock<std::mutex> lock(frame_mutex);
        frame_cv.wait(lock, [] { return frame_data == nullptr; });
        frame_fd = fd;
        frame_data = map;
        frame_size = size;
        frame_cv.notify_one();
    }

    gl.UnmapBuffer(GL_ARRAY_BUFFER);

    if (!worker_thread_running) {
        worker_thread_running = true;
        worker_thread = std::thread(worker_thread_function);
        worker_thread.detach();
    }

    Py_RETURN_NONE;
}

PyObject* turbopipe_sync(PyObject* self, PyObject* args) {
    PyObject* stop;

    if (!PyArg_ParseTuple(args, "O", &stop)) {
        return NULL;
    }

    /* Wait until all work is done */ {
        std::unique_lock<std::mutex> lock(frame_mutex);
        frame_cv.wait(lock, [] { return frame_data == nullptr; });
    }

    // Stop the thread
    if (stop == Py_True) {
        std::unique_lock<std::mutex> lock(frame_mutex);
        worker_thread_running = false;
        frame_cv.notify_all();
    }

    Py_RETURN_NONE;
}

static PyMethodDef TurboPipeMethods[] = {
    {"read", (PyCFunction) turbopipe_read, METH_VARARGS, "Read data into buffer or file descriptor"},
    {"sync", (PyCFunction) turbopipe_sync, METH_VARARGS, "Wait until all work is done, optionally stop the thread"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef turbopipe_module = {
    PyModuleDef_HEAD_INIT,
    "turbopipe_cpp",
    NULL,
    -1,
    TurboPipeMethods
};

extern "C" PyObject * PyInit_turbopipe_cpp() {
    return PyModule_Create(&turbopipe_module);
}
