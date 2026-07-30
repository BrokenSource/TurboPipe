use std::sync::LazyLock;

use crossbeam::sync::WaitGroup;
use crossbeam_channel::Receiver;
use crossbeam_channel::Sender;
use crossbeam_channel::unbounded;
use dashmap::DashMap;
use pyo3::ffi::Py_buffer;
use pyo3::ffi::PyBUF_SIMPLE;
use pyo3::prelude::*;
use pyo3::types::PyMemoryView;

type Pointer = usize;
type File = i32;

#[derive(Clone, Debug)]
struct Work {
    data: Pointer,
    size: isize,
    file: File,
    wait: WaitGroup,
}

impl Work {
    fn from_buffer(memoryview: Py<PyMemoryView>) -> Self {
        unsafe {
            let mut buffer: Py_buffer = std::mem::zeroed();

            pyo3::ffi::PyObject_GetBuffer(memoryview.as_ptr(), &mut buffer, PyBUF_SIMPLE);
            pyo3::ffi::PyBuffer_Release(&mut buffer);

            Self {
                data: buffer.buf as Pointer,
                size: buffer.len as isize,
                wait: WaitGroup::new(),
                file: 0,
            }
        }
    }
}

struct TurboPipe {
    send: DashMap<File, Sender<Option<Work>>>,
    wait: DashMap<Pointer, WaitGroup>,
}

impl TurboPipe {
    pub fn new() -> Self {
        Self {
            send: DashMap::new(),
            wait: DashMap::new(),
        }
    }

    pub fn pipe(&self, data: Pointer, size: isize, file: File) {
        self.sync(data);
        let wait = self.wait.entry(data).or_insert_with(WaitGroup::new).clone();

        // Create worker thread
        if !self.send.contains_key(&file) {
            let (send, receive) = unbounded();
            self.send.insert(file, send);
            std::thread::spawn(move || Self::worker(file, receive));
        }

        // Send work to channel
        if let Some(sender) = self.send.get(&file) {
            let work = Work {
                data,
                size,
                file,
                wait,
            };
            sender.send(Some(work)).expect("Send failed");
        }
    }

    fn worker(file: File, receiver: Receiver<Option<Work>>) {
        while let Ok(Some(work)) = receiver.recv() {
            let mut written = 0;

            while written < work.size {
                written += unsafe {
                    libc::write(
                        file,
                        (work.data as *const u8).add(written as usize) as *const libc::c_void,
                        (work.size - written).min(4096) as usize,
                    )
                };
            }

            drop(work.wait);
        }
    }

    /// Ensures this memory is not pending
    pub fn sync(&self, data: Pointer) {
        if let Some((_, wait)) = self.wait.remove(&data) {
            wait.wait();
        }
    }

    pub fn close(&self, file: File) {
        if let Some(sender) = self.send.get(&file) {
            let _ = sender.send(None);
            self.send.remove(&file);
        }
    }
}

/// Global and the only turbopipe instance that should exist
static TURBOPIPE: LazyLock<TurboPipe> = LazyLock::new(TurboPipe::new);

#[pyfunction]
fn pipe(buffer: Py<PyMemoryView>, file: File) -> PyResult<()> {
    let mut work = Work::from_buffer(buffer);
    work.file = file;
    TURBOPIPE.pipe(work.data, work.size, file);
    Ok(())
}

#[pyfunction]
fn sync(buffer: Py<PyMemoryView>) -> PyResult<()> {
    let work = Work::from_buffer(buffer);
    TURBOPIPE.sync(work.data);
    Ok(())
}

#[pyfunction]
fn close(file: File) -> PyResult<()> {
    TURBOPIPE.close(file);
    Ok(())
}

#[pymodule(gil_used = false)]
fn _turbopipe(module: &Bound<'_, PyModule>) -> PyResult<()> {
    module.add_function(wrap_pyfunction!(pipe, module)?)?;
    module.add_function(wrap_pyfunction!(sync, module)?)?;
    module.add_function(wrap_pyfunction!(close, module)?)?;
    Ok(())
}
