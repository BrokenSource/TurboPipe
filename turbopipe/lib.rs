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

type PendingPointers = Arc<Mutex<HashMap<FileDescriptor, HashSet<Pointer>>>>;
type StreamsMap = Arc<Mutex<HashMap<FileDescriptor, EternalWriter>>>;

struct EternalWriter {
    sender: Sender<Frame>,
    handle: JoinHandle<()>,
}

struct EternalReader {
    pending: PendingPointers,
    streams: StreamsMap,
    queue: Receiver<Work>,
}

struct TurboPipe {
    queue: Sender<Work>,
    pending: PendingPointers,
    streams: StreamsMap,
}

impl TurboPipe {
    pub fn new() -> Self {
        let pending = Arc::new(Mutex::new(HashMap::new()));
        let streams = Arc::new(Mutex::new(HashMap::new()));
        let (queue, queue_rx) = unbounded();
        for _ in 0..*READ_THREAD_COUNT {
            thread::spawn(move || Self::eternal_reader(EternalReader{
                pending: pending.clone(),
                streams: streams.clone(),
                queue: queue_rx.clone(),
            }));
        }
        Self {queue, pending, streams}
    }

    fn eternal_reader(this: EternalReader) {
        while let Ok(work) = this.queue.recv() {
            let data = unsafe { std::slice::from_raw_parts(work.data as *const u8, work.size).to_vec() };
            let sender = this.streams.lock().unwrap().get(&work.file).map(|w| w.sender.clone());
            if let Some(sender) = sender {
                sender.send(data).unwrap();
            }
            let mut p = this.pending.lock().unwrap();
            if let Some(set) = p.get_mut(&work.file) {
                set.remove(&work.data);
            }
        }
    }

    fn eternal_writer(rx: Receiver<Frame>, file: FileDescriptor) {
        let mut file = unsafe { File::from_raw_fd(file) };
        while let Ok(data) = rx.recv() {
            let _ = file.write_all(&data);
        }
        std::mem::forget(file);
    }

    fn make_stream(&self, file: FileDescriptor) {
        let mut streams = self.streams.lock().unwrap();
        if streams.contains_key(&file) {
            return;
        }
        let (tx, rx) = unbounded();
        let handle = thread::spawn(move || Self::eternal_writer(rx, file));
        streams.insert(file, EternalWriter {sender: tx, handle});
    }

    pub fn pipe(&self, data: Pointer, size: usize, file: FileDescriptor) {
        self.make_stream(file);
        loop {
            let mut p = self.pending.lock().unwrap();
            if p.values().any(|s| s.contains(&data)) {
                drop(p);
                thread::yield_now();
                continue;
            }
            p.entry(file).or_insert_with(HashSet::new).insert(data);
            break;
        }
        self.queue.send(Work { data, size, file }).unwrap();
    }

    pub fn sync(&self) {
        loop {
            let p = self.pending.lock().unwrap();
            if p.values().all(|s| s.is_empty()) {
                break;
            }
            drop(p);
            thread::yield_now();
        }
    }

    pub fn close(&self, file: FileDescriptor) {
        loop {
            let p = self.pending.lock().unwrap();
            if p.get(&file).map_or(true, |s| s.is_empty()) {
                break;
            }
            drop(p);
            thread::yield_now();
        }
        let mut streams = self.streams.lock().unwrap();
        if let Some(w) = streams.remove(&file) {
            drop(w.sender);
            w.handle.join().unwrap();
        }
    }
}

static TURBOPIPE: Lazy<TurboPipe> = Lazy::new(TurboPipe::new);

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
    TURBOPIPE.sync();
    Ok(())
}

#[pyfunction]
fn close(file: FileDescriptor) -> PyResult<()> {
    TURBOPIPE.close(file);
    Ok(())
}

#[pymodule]
fn turbopipe(module: &Bound<'_, PyModule>) -> PyResult<()> {
    module.add_function(wrap_pyfunction!(pipe, module)?)?;
    module.add_function(wrap_pyfunction!(sync, module)?)?;
    module.add_function(wrap_pyfunction!(close, module)?)?;
    Ok(())
}