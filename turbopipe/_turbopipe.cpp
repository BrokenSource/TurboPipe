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
    void*  data;
    size_t size;
    int    file;
};

class TurboPipe {
public:
    TurboPipe(): running(true) {}
    ~TurboPipe() {close();}

    void pipe(PyObject* view, int file) {
        Py_buffer data = *PyMemoryView_GET_BUFFER(view);
        this->_pipe({data.buf, (size_t) data.len, file});
    }

    void sync(PyObject* view=nullptr) {
        void* data = nullptr;

        if (view != nullptr) {
            Py_buffer temp = *PyMemoryView_GET_BUFFER(view);
            data = temp.buf;
        }

        this->_sync(data);
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

    void _pipe(Work work) {
        unique_lock<mutex> lock(mutexes[work.file]);

        /* Notify this memory is queued, wait if pending */ {
            if (!queue[work.file].insert(work.data).second) {
                pending[work.file][work.data].wait(lock, [this, work] {
                    return queue[work.file].find(work.data) == queue[work.file].end();
                });
            }
        }

        /* Add another job to the queue */ {
            stream[work.file].push_back(work);
            queue[work.file].insert(work.data);
            this->running = true;
            lock.unlock();
        }

        // Each file descriptor has its own thread
        if (threads.find(work.file) == threads.end())
            threads[work.file] = thread(&TurboPipe::worker, this, work.file);

        signal.notify_all();
    }

    void _sync(void* data=nullptr) {
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
    PyObject* view;
    PyObject* file;
    if (!PyArg_ParseTuple(args, "OO", &view, &file))
        return NULL;
    if (!PyMemoryView_Check(view)) {
        PyErr_SetString(PyExc_TypeError, "Expected a memoryview object");
        return NULL;
    }
    turbopipe->pipe(view, PyLong_AsLong(file));
    Py_RETURN_NONE;
}

static PyObject* turbopipe_sync(
    PyObject* Py_UNUSED(self),
    PyObject* args
) {
    PyObject* view;
    if (!PyArg_ParseTuple(args, "|O", &view))
        return NULL;
    if (view != nullptr && !PyMemoryView_Check(view)) {
        PyErr_SetString(PyExc_TypeError, "Expected a memoryview object or None");
        return NULL;
    }
    turbopipe->sync(view);
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
