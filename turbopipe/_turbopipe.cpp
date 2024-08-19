// ------------------------------------------------------------------------------------------------|
//
// TurboPipe - Faster ModernGL Buffers inter-process data transfers for subprocesses
//
// (c) 2024, Tremeschin, MIT License
//
// ------------------------------------------------------------------------------------------------|

#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Standard library
#include <functional>
#include <chrono>

// Threading
#include <condition_variable>
#include <thread>
#include <mutex>

// Data structure
#include <unordered_set>
#include <unordered_map>
#include <deque>

using namespace std;

// ------------------------------------------------------------------------------------------------|
// TurboPipe internals

struct Work {
    void* data;
    int file;
    size_t size;
};

class TurboPipe {
public:
    TurboPipe(): running(true) {}
    ~TurboPipe() {close();}

    void pipe(PyObject* memoryview, int file) {
        Py_buffer view = *PyMemoryView_GET_BUFFER(memoryview);
        this->_pipe(view.buf, view.len, file);
    }

    void sync(PyObject* memoryview=nullptr) {
        void* data = nullptr;

        if (memoryview != nullptr) {
            Py_buffer view = *PyMemoryView_GET_BUFFER(memoryview);
            data = view.buf;
        }

        // Wait for some or all queues to be empty, as they are erased when
        // each thread's writing loop is done, guaranteeing finish
        for (auto& values: queue) {
            while (true) {
                {
                    // Prevent segfault on iteration on changing data
                    lock_guard<mutex> lock(mutexes[values.first]);

                    // Either all empty or some memory not queued (None or specific)
                    if (data != nullptr && values.second.find(data) == values.second.end())
                        break;
                    if (data == nullptr && values.second.empty())
                        break;
                }
                this_thread::sleep_for(chrono::microseconds(200));
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
    unordered_map<int, unordered_map<void*, condition_variable>> pending;
    unordered_map<int, unordered_set<void*>> queue;
    unordered_map<int, deque<Work>> stream;
    unordered_map<int, thread> threads;
    unordered_map<int, mutex> mutexes;
    condition_variable signal;
    bool running;

    void _pipe(void* data, size_t size, int file) {
        Work work = {data, file, size};
        unique_lock<mutex> lock(mutexes[file]);

        /* Notify this memory is queued, wait if pending */ {
            if (!queue[file].insert(data).second) {
                pending[file][data].wait(lock, [this, file, data] {
                    return queue[file].find(data) == queue[file].end();
                });
            }
        }

        /* Add another job to the queue */ {
            stream[file].push_back(work);
            queue[file].insert(data);
            this->running = true;
            lock.unlock();
        }

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
                write(work.file, (char*) work.data, work.size);
            #else
                // Optimization: Write in chunks of 4096 (RAM page size)
                size_t tell = 0;
                while (tell < work.size) {
                    size_t chunk = min(work.size - tell, static_cast<size_t>(4096));
                    size_t written = write(work.file, (char*) work.data + tell, chunk);
                    if (written == -1) break;
                    tell += written;
                }
            #endif

            lock.lock();

            /* Signal work is done */ {
                pending[file][work.data].notify_all();
                queue[file].erase(work.data);
                signal.notify_all();
            }
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
    PyObject* memoryview;
    PyObject* file;
    if (!PyArg_ParseTuple(args, "OO", &memoryview, &file))
        return NULL;
    if (!PyMemoryView_Check(memoryview)) {
        PyErr_SetString(PyExc_TypeError, "Expected a memoryview object");
        return NULL;
    }
    turbopipe->pipe(memoryview, PyLong_AsLong(file));
    Py_RETURN_NONE;
}

static PyObject* turbopipe_sync(
    PyObject* Py_UNUSED(self),
    PyObject* args
) {
    PyObject* memoryview;
    if (!PyArg_ParseTuple(args, "|O", &memoryview))
        return NULL;
    if (memoryview != nullptr && !PyMemoryView_Check(memoryview)) {
        PyErr_SetString(PyExc_TypeError, "Expected a memoryview object or None");
        return NULL;
    }
    turbopipe->sync(memoryview);
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
    {"sync",  (PyCFunction) turbopipe_sync,  METH_VARARGS, ""},
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
    if (module == NULL)
        return NULL;
    turbopipe = new TurboPipe();
    Py_AtExit(turbopipe_exit);
    return module;
}
