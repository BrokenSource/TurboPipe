// ------------------------------------------------------------------------------------------------|
// TurboPipe - Faster ModernGL Buffers inter-process data transfers for subprocesses
// (c) MIT License 2024-2025, Tremeschin
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
        this->_pipe(data.buf, (size_t) data.len, file);
    }

    void sync(PyObject* view=nullptr) {
        if (view != nullptr)
            this->_sync((*PyMemoryView_GET_BUFFER(view)).buf);
        else
            this->_sync(nullptr);
    }

    void close() {
        this->_sync();
        this->running = false;
        for (auto& pair: this->signal)
            pair.second.notify_all();
        for (auto& pair: this->threads)
            pair.second.join();
        this->threads.clear();
    }

private:
    unordered_map<int, condition_variable> pending;
    unordered_map<int, condition_variable> signal;
    unordered_map<int, unordered_set<void*>> queue;
    unordered_map<int, deque<Work>> stream;
    unordered_map<int, thread> threads;
    unordered_map<int, mutex> mutexes;
    bool running;

    void _pipe(void* data, size_t size, int file) {
        unique_lock<mutex> lock(this->mutexes[file]);

        /* Notify this memory is queued, wait if pending */ {
            if (!this->queue[file].insert(data).second) {
                this->pending[file].wait(lock, [this, file, data] {
                    return this->queue[file].find(data) == this->queue[file].end();
                });
            }
        }

        /* Add another job to the queue */ {
            this->stream[file].push_back(Work{data, size, file});
            this->queue[file].insert(data);
            this->running = true;
            lock.unlock();
        }

        // Each file descriptor has its own thread
        if (this->threads.find(file) == this->threads.end())
            this->threads[file] = thread(&TurboPipe::worker, this, file);

        // Trigger the worker to write the data
        this->signal[file].notify_all();
    }

    void _sync(void* data=nullptr) {
        for (auto& values: this->queue) {
            while (true) {
                {
                    // Prevent segfault on iteration on changing data
                    lock_guard<mutex> lock(this->mutexes[values.first]);

                    // Continue if specific data is not in queue
                    if (data != nullptr)
                        if (values.second.find(data) == values.second.end())
                            break;

                    // Continue if all queues are empty
                    if (data == nullptr)
                        if (values.second.empty())
                            break;
                }
                this_thread::sleep_for(chrono::microseconds(200));
            }
        }
    }

    void worker(int file) {
        while (this->running) {
            unique_lock<mutex> lock(this->mutexes[file]);

            this->signal[file].wait(lock, [this, file] {
                return (!this->stream[file].empty() || !this->running);
            });

            // Skip on false positives, exit condition
            if ( this->stream[file].empty()) continue;
            if (!this->running) break;

            // Get the next work item
            Work work = this->stream[file].front();
            this->stream[file].pop_front();
            lock.unlock();

            #ifdef _WIN32
                // Fixme: Windows doesn't like chunked writes?
                write(work.file, (char*) work.data, static_cast<unsigned int>(work.size));
            #else
                size_t tell = 0;
                while (tell < work.size) {
                    size_t chunk = min(work.size - tell, static_cast<size_t>(4096));
                    int written = write(work.file, (char*) work.data + tell, chunk);
                    if (written == -1) break;
                    tell += written;
                }
            #endif

            lock.lock();

            /* Signal work is done */ {
                this->pending[file].notify_all();
                this->queue[file].erase(work.data);
                this->signal[file].notify_all();
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

static struct PyModuleDef TurboPipeModule = {
    .m_base     = PyModuleDef_HEAD_INIT,
    .m_name     = "_turbopipe",
    .m_doc      = NULL,
    .m_size     = -1,
    .m_methods  = TurboPipeMethods,
    .m_slots    = NULL,
    .m_traverse = NULL,
    .m_clear    = NULL,
    .m_free     = NULL
};

PyMODINIT_FUNC PyInit__turbopipe(void) {
    PyObject* module = PyModule_Create(&TurboPipeModule);
    if (module == NULL)
        return NULL;
    #ifdef Py_GIL_DISABLED
        PyUnstable_Module_SetGIL(module, Py_MOD_GIL_NOT_USED);
    #endif
    turbopipe = new TurboPipe();
    Py_AtExit(turbopipe_exit);
    return module;
}
