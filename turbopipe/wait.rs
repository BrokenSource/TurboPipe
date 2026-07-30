//! This module is based on `crossbeam_utils::sync::WaitGroup`
//!
//! - Added an untracked clone method that does not increase the count.
//!
//! License: Same as upstream crate (MIT OR Apache-2.0)

use core::mem::ManuallyDrop;
use std::sync::Arc;
use std::sync::Condvar;
use std::sync::Mutex;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering;

pub struct WaitGroup {
    inner: Arc<Inner>,
}

struct Inner {
    cvar: Condvar,
    lock: Mutex<()>,
    count: AtomicUsize,
}

impl Default for WaitGroup {
    fn default() -> Self {
        Self {
            inner: Arc::new(Inner {
                cvar: Condvar::new(),
                lock: Mutex::new(()),
                count: AtomicUsize::new(1),
            }),
        }
    }
}

impl WaitGroup {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn wait(self) {
        let inner = unsafe {
            let slf = ManuallyDrop::new(self);
            core::ptr::read(&slf.inner)
        };

        if inner.count.fetch_sub(1, Ordering::AcqRel) == 1 {
            drop(inner.lock.lock().unwrap());
            inner.cvar.notify_all();
            return;
        }

        let mut guard = inner.lock.lock().unwrap();
        while inner.count.load(Ordering::Acquire) != 0 {
            guard = inner.cvar.wait(guard).unwrap();
        }
    }

    // TurboPipe addition: Clone that does not increment count
    pub fn clone_untracked(&self) -> Self {
        Self {
            inner: self.inner.clone(),
        }
    }
}

impl Drop for WaitGroup {
    fn drop(&mut self) {
        if self.inner.count.fetch_sub(1, Ordering::Release) == 1 {
            drop(self.inner.lock.lock().unwrap());
            self.inner.cvar.notify_all();
        }
    }
}

impl Clone for WaitGroup {
    fn clone(&self) -> Self {
        self.inner.count.fetch_add(1, Ordering::Relaxed);
        Self {
            inner: self.inner.clone(),
        }
    }
}
