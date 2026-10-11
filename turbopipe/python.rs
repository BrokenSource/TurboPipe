use pyo3::prelude::*;

use crate::Data;
use crate::File;
use crate::Length;
use crate::ObjectID;
use crate::Pointer;
use crate::TURBOPIPE;

impl Data {
    pub fn from_object(object: Py<PyAny>) -> PyResult<Self> {
        unsafe {
            let mut buffer = pyo3::ffi::Py_buffer::new();

            // Stable in >=3.11 for abi3
            let ret = pyo3::ffi::PyObject_GetBuffer(
                object.as_ptr(),
                &mut buffer,
                pyo3::ffi::PyBUF_SIMPLE,
            );

            if ret != 0 {
                return Err(pyo3::exceptions::PyTypeError::new_err(
                    "Couldn't map object into a buffer",
                ));
            }

            // Get mapped metadata
            let this = Self {
                obj: buffer.obj as ObjectID,
                ptr: buffer.buf as Pointer,
                len: buffer.len as Length,
            };

            // Fixme: Works, but is it always valid?
            pyo3::ffi::PyBuffer_Release(&mut buffer);
            return Ok(this);
        }
    }
}

#[pyfunction]
fn _pipe(data: Py<PyAny>, file: File) -> PyResult<()> {
    let data = Data::from_object(data)?;
    TURBOPIPE.pipe(data, file);
    Ok(())
}

#[pyfunction]
fn _sync(data: Py<PyAny>) -> PyResult<()> {
    TURBOPIPE.sync(data.as_ptr() as ObjectID);
    Ok(())
}

#[pyfunction]
fn _done(data: Py<PyAny>) -> PyResult<()> {
    TURBOPIPE.done(data.as_ptr() as ObjectID);
    Ok(())
}

#[pymodule(gil_used = false)]
fn _turbopipe(module: &Bound<'_, PyModule>) -> PyResult<()> {
    module.add_function(wrap_pyfunction!(_pipe, module)?)?;
    module.add_function(wrap_pyfunction!(_sync, module)?)?;
    module.add_function(wrap_pyfunction!(_done, module)?)?;
    Ok(())
}
