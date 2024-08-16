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

#define dict std::unordered_map
using namespace std;

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

    void pipe(PyObject* memoryview, int file) {
        Py_buffer view = *PyMemoryView_GET_BUFFER(memoryview);
        this->_pipe(view.buf, view.len, file);
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
    if (module == NULL)
        return NULL;
    turbopipe = new TurboPipe();
    Py_AtExit(turbopipe_exit);
    return module;
}
