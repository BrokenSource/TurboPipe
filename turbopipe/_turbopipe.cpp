// ------------------------------------------------------------------------------------------------|
//
// TurboPipe - Faster ModernGL Buffer inter process data transfers
//
// (c) 2024, Tremeschin, MIT License
//
// ------------------------------------------------------------------------------------------------|

#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Standard library
#include <functional>
#include <iostream>
#include <chrono>

// Threading
#include <condition_variable>
#include <thread>
#include <mutex>

// Data structure
#include <unordered_set>
#include <unordered_map>
#include <deque>

// Third party
#include "gl_methods.hpp"

#define dict std::unordered_map
using namespace std;

// ------------------------------------------------------------------------------------------------|
// ModernGL Types - Courtesy of the moderngl package developers (MIT)

static PyTypeObject* MGLBuffer_type = nullptr;

struct MGLContext;
struct MGLFramebuffer;

struct MGLBuffer {
    PyObject_HEAD
    MGLContext* context;
    int buffer;
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

// ------------------------------------------------------------------------------------------------|
// TurboPipe internals

struct Work {
    void* map;
    int file;
    size_t size;

    int hash() {
        return std::hash<int>()(file) ^ std::hash<void*>()(map);
    }
};

class TurboPipe {
public:
    TurboPipe(): running(true) {}
    ~TurboPipe() {close();}

    void pipe(MGLBuffer* buffer, int file) {
        const GLMethods& gl = buffer->context->gl;

        gl.BindBuffer(GL_ARRAY_BUFFER, buffer->buffer);
        void* data = gl.MapBufferRange(GL_ARRAY_BUFFER, 0, buffer->size, GL_MAP_READ_BIT);
        gl.UnmapBuffer(GL_ARRAY_BUFFER);

        this->_pipe(data, buffer->size, file);
    }

    void sync() {
        // Wait for all queues to be empty, as they are erased when
        // each thread's writing loop is done, guaranteeing finish
        for (auto& values: queue) {
            while (!values.second.empty()) {
                this_thread::sleep_for(chrono::milliseconds(1));
            }
        }
    }

    void close() {
        sync();
        running = false;
        signal.notify_all();
        for (auto& pair: threads)
            pair.second.join();
        threads.clear();
    }

private:
    dict<int, dict<int, condition_variable>> pending;
    dict<int, unordered_set<int>> queue;
    dict<int, deque<Work>> stream;
    dict<int, thread> threads;
    dict<int, mutex> mutexes;
    condition_variable signal;
    bool running;

    void _pipe(void* data, size_t size, int file) {
        Work work = {data, file, size};
        int hash = work.hash();

        unique_lock<mutex> lock(mutexes[file]);

        // Notify this hash is queued, wait if pending
        if (!queue[file].insert(hash).second) {
            pending[file][hash].wait(lock, [this, file, hash] {
                return queue[file].find(hash) == queue[file].end();
            });
        }

        // Add another job to the queue
        stream[file].push_back(work);
        queue[file].insert(hash);
        this->running = true;
        lock.unlock();

        // Each file descriptor has its own thread
        if (threads.find(file) == threads.end())
            threads[file] = thread(&TurboPipe::worker, this, file);

        signal.notify_all();
    }

    void worker(int file) {
        while (this->running) {
            unique_lock<mutex> lock(mutexes[file]);

            signal.wait(lock, [this, file] {
                return (!stream[file].empty() || !this->running);
            });

            // Skip on false positives, exit condition
            if (stream[file].empty()) continue;
            if (!this->running) break;

            // Get the next work item
            Work work = stream[file].front();
            stream[file].pop_front();
            lock.unlock();

            #ifdef _WIN32
                // Windows doesn't like chunked writes ??
                write(work.file, (char*) work.map, work.size);
            #else
                // Optimization: Write in chunks of 4096 (RAM page size)
                size_t tell = 0;
                while (tell < work.size) {
                    size_t chunk = min(work.size - tell, static_cast<size_t>(4096));
                    size_t written = write(work.file, (char*) work.map + tell, chunk);
                    if (written == -1) break;
                    tell += written;
                }
            #endif

            // Signal work is done
            lock.lock();
            int hash = work.hash();
            pending[file][hash].notify_all();
            queue[file].erase(hash);
            signal.notify_all();
        }
    }
};

// The main and only instance of TurboPipe
static TurboPipe* turbopipe = nullptr;

// ------------------------------------------------------------------------------------------------|
// End user methods

static PyObject* turbopipe_pipe(
    PyObject* Py_UNUSED(self),
    PyObject* args
) {
    PyObject* buffer;
    PyObject* file;
    if (!PyArg_ParseTuple(args, "OO", &buffer, &file))
        return NULL;
    turbopipe->pipe((MGLBuffer*) buffer, PyLong_AsLong(file));
    Py_RETURN_NONE;
}

static PyObject* turbopipe_sync(
    PyObject* Py_UNUSED(self),
    PyObject* Py_UNUSED(args)
) {
    turbopipe->sync();
    Py_RETURN_NONE;
}

static PyObject* turbopipe_close(
    PyObject* Py_UNUSED(self),
    PyObject* Py_UNUSED(args)
) {
    turbopipe->close();
    Py_RETURN_NONE;
}

// ------------------------------------------------------------------------------------------------|
// Python module definition

static void turbopipe_exit() {
    turbopipe->close();
}

static PyMethodDef TurboPipeMethods[] = {
    {"pipe",  (PyCFunction) turbopipe_pipe,  METH_VARARGS, ""},
    {"sync",  (PyCFunction) turbopipe_sync,  METH_NOARGS,  ""},
    {"close", (PyCFunction) turbopipe_close, METH_NOARGS,  ""},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef turbopipe_module = {
    PyModuleDef_HEAD_INIT,
    "_turbopipe",
    NULL, -1,
    TurboPipeMethods,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC PyInit__turbopipe(void) {
    PyObject* module = PyModule_Create(&turbopipe_module);
    PyObject* moderngl = PyImport_ImportModule("moderngl");
    PyObject* buffer = PyObject_GetAttrString(moderngl, "Buffer");
    MGLBuffer_type = (PyTypeObject*) buffer;
    turbopipe = new TurboPipe();
    Py_AtExit(turbopipe_exit);
    return module;
}
