use std::collections::{HashMap, HashSet};
use std::fs::File;
use std::io::Write;
use std::os::fd::FromRawFd;
use std::sync::{Arc, Mutex};
use std::thread::{self, JoinHandle};

use crossbeam_channel::{Sender, Receiver, unbounded};
use once_cell::sync::Lazy;
use pyo3::buffer::PyBuffer;
use pyo3::prelude::*;
use pyo3::types::PyMemoryView;

static READ_THREAD_COUNT: Lazy<usize> = Lazy::new(|| {
    std::env::var("TURBOPIPE_READ_THREADS").ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(4)
});

type FileDescriptor = i32;
type Pointer = usize;
type Frame = Vec<u8>;

#[derive(Clone)]
struct Work {
    data: Pointer,
    size: usize,
    file: FileDescriptor,
}

/// Stream to file descriptor
struct Stream {
    sender: Sender<Frame>,
    handle: JoinHandle<()>,
}

struct TurboPipe {
    queue:   Sender<Work>,
    pending: Mutex<HashSet<Pointer>>,
    streams: Mutex<HashMap<FileDescriptor, Stream>>,
}

impl TurboPipe {
    pub fn new() -> Arc<Self> {
        let (queue_tx, queue_rx) = unbounded();

        let pipe = Arc::new(Self {
            queue: queue_tx,
            pending: Mutex::new(HashSet::new()),
            streams: Mutex::new(HashMap::new()),
        });

        // Spawn persistent read threads
        for _ in 0..*READ_THREAD_COUNT {
            let pipe  = Arc::clone(&pipe);
            let queue = queue_rx.clone();
            thread::spawn(move || Self::read(pipe, queue));
        }

        return pipe;
    }

    /// Eternal reader algorithm
    fn read(
        self: Arc<Self>,
        queue: Receiver<Work>
    ) {
        while let Ok(work) = queue.recv() {
            let data = unsafe {
                std::slice::from_raw_parts(
                    work.data as *const u8,
                    work.size
                ).to_vec()
            };

            // Mark the memory as done
            self.pending.lock().unwrap().remove(&work.data);

            // Sort and send results to writers
            self.streams.lock().unwrap()
                .get(&work.file).unwrap()
                .sender.send(data).unwrap();
        }
    }

    /// Eternal writer algorithm
    fn write(
        queue: Receiver<Frame>,
        file: FileDescriptor,
    ) {
        let mut file = unsafe {File::from_raw_fd(file)};

        while let Ok(data) = queue.recv() {
            let _ = file.write_all(&data);
        }

        // Keep the file descriptor open
        std::mem::forget(file);
    }

    fn make_stream(&self, file: FileDescriptor) -> Sender<Frame> {
        let mut streams = self.streams.lock().unwrap();

        if let Some(stream) = streams.get(&file) {
            return stream.sender.clone();
        }

        let (tx, rx) = unbounded();
        let handle = thread::spawn(move || Self::write(rx, file));
        streams.insert(file, Stream {sender: tx.clone(), handle});

        return tx;
    }

    pub fn pipe(&self,
        data: Pointer,
        size: usize,
        file: FileDescriptor
    ) {
        self.make_stream(file);

        // Wait if this pointer is already being processed
        while !self.pending.lock().unwrap().insert(data) {
            thread::yield_now();
        }

        let _ = self.queue.send(Work {data, size, file});
    }

    /// Wait for all pending writes on a file to finish
    pub fn sync(&self, data: Pointer) {
        while self.pending.lock().unwrap().contains(&data) {
            thread::yield_now();
        }
    }

    pub fn close(&self, file: FileDescriptor) {
        // self.sync(file);

        let mut streams = self.streams.lock().unwrap();

        if let Some(stream) = streams.remove(&file) {
            drop(stream.sender);
            let _ = stream.handle.join();
        }
    }
}

static TURBOPIPE: Lazy<Arc<TurboPipe>> = Lazy::new(TurboPipe::new);

#[pyfunction]
fn pipe(view: Bound<'_, PyMemoryView>, file: FileDescriptor) -> PyResult<()> {
    let buffer: PyBuffer<u8> = PyBuffer::get(&view)?;

    TURBOPIPE.pipe(
        buffer.buf_ptr() as Pointer,
        buffer.len_bytes(),
        file
    );

    Ok(())
}

#[pyfunction]
fn sync() -> PyResult<()> {
    TURBOPIPE.sync(0);
    Ok(())
}

#[pyfunction]
fn close(file: FileDescriptor) -> PyResult<()> {
    TURBOPIPE.close(file);
    Ok(())
}

#[pymodule]
fn turbopipe(module: &Bound<'_, PyModule>) -> PyResult<()> {
    module.add_function(wrap_pyfunction!(pipe,  module)?)?;
    module.add_function(wrap_pyfunction!(sync,  module)?)?;
    module.add_function(wrap_pyfunction!(close, module)?)?;
    Ok(())
}
