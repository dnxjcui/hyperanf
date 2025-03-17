#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "hll.h"

static PyObject* py_hll_example(PyObject* self, PyObject* args) {
    unsigned short p;
    if (!PyArg_ParseTuple(args, "H", &p)) {
        return NULL;
    }

    // Initialize two HyperLogLog counters
    HyperLogLog* hll1 = hll_init(p, 42, false, 0, 0);
    HyperLogLog* hll2 = hll_init(p, 42, false, 0, 0);

    if (!hll1 || !hll2) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize HyperLogLog counters");
        return NULL;
    }

    // Add elements to the counters
    const uint8_t a[] = "a";
    const uint8_t b[] = "b";
    hll_add(hll1, a, sizeof(a) - 1);
    hll_add(hll2, b, sizeof(b) - 1);

    // Merge hll2 into hll1
    if (!hll_merge(hll1, hll2)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to merge HyperLogLog counters");
        return NULL;
    }

    // Get the cardinality of the merged counter
    uint64_t cardinality = hll_cardinality(hll1);

    // Free the HyperLogLog structures
    hll_free(hll1);
    hll_free(hll2);

    return PyLong_FromUnsignedLongLong(cardinality);
}

static PyMethodDef HllMethods[] = {
    {"hll_example", py_hll_example, METH_VARARGS, "Compute HyperLogLog cardinality."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef hllmodule = {
    PyModuleDef_HEAD_INIT,
    "hll_module",
    NULL,
    -1,
    HllMethods
};

PyMODINIT_FUNC PyInit_hll_module(void) {
    return PyModule_Create(&hllmodule);
}
