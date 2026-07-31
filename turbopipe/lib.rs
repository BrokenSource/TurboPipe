use std::sync::LazyLock;

use crossbeam_channel::Receiver;
use crossbeam_channel::Sender;
use dashmap::DashMap;
use pyo3::prelude::*;
use pyo3::types::PyMemoryView;

mod wait;
use wait::WaitGroup;

pub type Pointer = usize;
pub type Length = usize;
pub type File = usize;

/* -------------------------------------------------------------------------- */

/// Some memory region
pub struct Data {
    pub ptr: Pointer,
    pub len: Length,
}

/// Queued write
pub struct Work {
    pub data: Data,
    pub sync: WaitGroup,
}

impl Data {
    pub fn from_memoryview(memoryview: Py<PyMemoryView>) -> Self {
        unsafe {
            let mut buffer = pyo3::ffi::Py_buffer::new();

            // Stable in >=3.11 for abi3
            pyo3::ffi::PyObject_GetBuffer(
                memoryview.as_ptr(),
                &mut buffer,
                pyo3::ffi::PyBUF_SIMPLE,
            );

            // Get only pointer and size
            let this = Self {
                ptr: buffer.buf as Pointer,
                len: buffer.len as Length,
            };

            // Fixme: Works, but is it always valid?
            pyo3::ffi::PyBuffer_Release(&mut buffer);
            return this;
        }
    }
}

/* -------------------------------------------------------------------------- */

pub struct TurboPipe {
    /// Channels for queueing pipes (mpsc + fifo)
    pub send: DashMap<File, Sender<Option<Work>>>,

    /// Barrier for pending pipes in pointers
    pub sync: DashMap<Pointer, WaitGroup>,
}

impl TurboPipe {
    pub fn new() -> Self {
        Self {
            send: DashMap::new(),
            sync: DashMap::new(),
        }
    }

    /// Queues some data to be written into the file descriptor by a worker
    ///
    /// - Callers must use [`TurboPipe::sync`] to wait on prior pipes
    /// - File descriptor (or handler) must be open and valid in the OS
    ///
    pub fn pipe(&self, data: Data, file: File) {
        // Get or create the work sync barrier
        let sync = self.sync.entry(data.ptr).or_insert_with(WaitGroup::new);

        // Get or create the channel and its worker
        let sender = self.send.entry(file).or_insert_with(|| {
            let (send, receive) = crossbeam_channel::bounded(32);
            std::thread::spawn(move || Self::worker(receive, file));
            return send;
        });

        // Attach barrier
        let work = Work {
            data,
            sync: sync.clone(),
        };

        sender.send(Some(work)).expect("Send failed");
    }

    /// Controls the chunk size in bytes a worker writes
    pub fn chunk() -> usize {
        static VALUE: LazyLock<usize> = LazyLock::new(|| {
            std::env::var("TURBOPIPE_CHUNK_SIZE")
                .ok()
                .and_then(|v| v.parse().ok())
                .unwrap_or(if cfg!(windows) { usize::MAX } else { 8192 })
        });
        *VALUE
    }

    /// Eternally reads and writes a work's data to the bound file descriptor.
    ///
    /// - Only one worker must exist per file descriptor (synchronization)
    /// - Poison-pill implementation with Option<Work>
    ///
    fn worker(receiver: Receiver<Option<Work>>, file: File) {
        while let Ok(Some(work)) = receiver.recv() {
            let mut written = 0;

            // Note: Chunked writes experimentally gave up to +100% speed
            //   improvements on certain systems, really not sure why.
            while written < work.data.len {
                written += unsafe {
                    libc::write(
                        file as libc::c_int,
                        (work.data.ptr as *const u8).add(written).cast(),
                        (work.data.len - written)
                            .min(Self::chunk())
                            .try_into()
                            .unwrap(),
                    ) as Length
                };
            }

            // Signal work done
            drop(work.sync);
        }
    }

    /// Ensures this memory is not pending
    pub fn sync(&self, data: Pointer) {
        if let Some(sync) = self.sync.get(&data) {
            sync.wait_untracked();
        }
    }

    /// Waits for all pending writes to this file to finish
    pub fn close(&self, file: File) {
        if let Some(sender) = self.send.get(&file) {
            sender.send(None).expect("Send failed");
            self.send.remove(&file);
        }
    }
}

/* -------------------------------------------------------------------------- */

/// Global and the only turbopipe instance that should exist
pub static TURBOPIPE: LazyLock<TurboPipe> = LazyLock::new(TurboPipe::new);

#[pyfunction]
fn pipe(view: Py<PyMemoryView>, file: File) -> PyResult<()> {
    let data = Data::from_memoryview(view);
    TURBOPIPE.pipe(data, file);
    Ok(())
}

#[pyfunction]
fn sync(view: Py<PyMemoryView>) -> PyResult<()> {
    let data = Data::from_memoryview(view);
    TURBOPIPE.sync(data.ptr);
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
