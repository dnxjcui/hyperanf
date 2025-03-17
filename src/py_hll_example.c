#define PY_SSIZE_T_CLEAN
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <Python.h>
#include <numpy/arrayobject.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "hll.h"
#include <string.h>

static PyObject* py_hyperanf(PyObject* self, PyObject* args) {
    unsigned short p;
    PyArrayObject* adjacency_matrix;
    if (!PyArg_ParseTuple(args, "HO", &p, &adjacency_matrix)) {
        return NULL;
    }

    if (!PyArray_Check(adjacency_matrix) || PyArray_NDIM(adjacency_matrix) != 2) {
        PyErr_SetString(PyExc_ValueError, "Expected a 2D numpy array");
        return NULL;
    }

    npy_intp* dims = PyArray_DIMS(adjacency_matrix);
    npy_intp N = dims[0];
    if (N != dims[1]) {
        PyErr_SetString(PyExc_ValueError, "Expected a square adjacency matrix");
        return NULL;
    }

    // Initialize an array of N HyperLogLog counters
    HyperLogLog** hll_array = (HyperLogLog**)malloc(N * sizeof(HyperLogLog*));
    if (!hll_array) {
        PyErr_SetString(PyExc_RuntimeError, "Memory allocation failed");
        return NULL;
    }
    for (npy_intp i = 0; i < N; i++) {
        hll_array[i] = hll_init(p, 12345, false, 0, 0);
        if (!hll_array[i]) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to initialize HyperLogLog counter");
            return NULL;
        }
        // Each node adds itself
        hll_add(hll_array[i], (const uint8_t*)&i, sizeof(i));
    }

    PyObject* neighborhood_sizes = PyList_New(0);
    bool changed;
    do {
        changed = false;
        uint64_t total_cardinality = 0;
        HyperLogLog** new_hll_array = (HyperLogLog**)malloc(N * sizeof(HyperLogLog*));
        for (npy_intp i = 0; i < N; i++) {
            new_hll_array[i] = hll_init(p, 12345, false, 0, 0);
            if (!new_hll_array[i]) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to initialize HyperLogLog counter");
                return NULL;
            }
            hll_merge(new_hll_array[i], hll_array[i]);
            for (npy_intp j = 0; j < N; j++) {
                uint8_t value = *(uint8_t*)PyArray_GETPTR2(adjacency_matrix, i, j);
                if (value) {
                    hll_merge(new_hll_array[i], hll_array[j]);
                }
            }
            uint64_t prev_cardinality = hll_cardinality(hll_array[i]);
            uint64_t new_cardinality = hll_cardinality(new_hll_array[i]);
            if (prev_cardinality != new_cardinality) {
                changed = true;
            }
            total_cardinality += new_cardinality;
        }

        PyList_Append(neighborhood_sizes, PyLong_FromUnsignedLongLong(total_cardinality));

        for (npy_intp i = 0; i < N; i++) {
            hll_free(hll_array[i]);
        }
        free(hll_array);
        hll_array = new_hll_array;
    } while (changed);

    for (npy_intp i = 0; i < N; i++) {
        hll_free(hll_array[i]);
    }
    free(hll_array);

    return neighborhood_sizes;
}

static PyObject* py_hyperanf_distance(PyObject* self, PyObject* args) {
    unsigned short p;
    PyArrayObject* adjacency_matrix;
    if (!PyArg_ParseTuple(args, "HO", &p, &adjacency_matrix)) {
        return NULL;
    }

    if (!PyArray_Check(adjacency_matrix) || PyArray_NDIM(adjacency_matrix) != 2) {
        PyErr_SetString(PyExc_ValueError, "Expected a 2D numpy array");
        return NULL;
    }

    npy_intp* dims = PyArray_DIMS(adjacency_matrix);
    npy_intp N = dims[0];
    if (N != dims[1]) {
        PyErr_SetString(PyExc_ValueError, "Expected a square adjacency matrix");
        return NULL;
    }

    // Initialize HyperLogLog counters
    HyperLogLog** hll_array = (HyperLogLog**)malloc(N * sizeof(HyperLogLog*));
    if (!hll_array) {
        PyErr_SetString(PyExc_RuntimeError, "Memory allocation failed");
        return NULL;
    }

    // Initialize counters and add each node to its own counter
    for (npy_intp i = 0; i < N; i++) {
        hll_array[i] = hll_init(p, 42, false, 0, 0);
        if (!hll_array[i]) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to initialize HyperLogLog counter");
            return NULL;
        }
        hll_add(hll_array[i], (const uint8_t*)&i, sizeof(i));
    }

    // Create lists for NFs and node_pairs
    PyObject* NFs = PyList_New(0);
    PyObject* node_pairs = PyList_New(0);
    bool changed;
    int t = 0;

    do {
        changed = false;
        uint64_t total_cardinality = 0;
        HyperLogLog** new_hll_array = (HyperLogLog**)malloc(N * sizeof(HyperLogLog*));

        // Calculate total cardinality and store in NFs
        for (npy_intp i = 0; i < N; i++) {
            total_cardinality += hll_cardinality(hll_array[i]);
        }
        PyList_Append(NFs, PyLong_FromUnsignedLongLong(total_cardinality));

        // Calculate node_pairs
        uint64_t prev_total = 0;
        if (t > 0) {
            PyObject* prev = PyList_GetItem(NFs, t-1);
            prev_total = PyLong_AsUnsignedLongLong(prev);
        }
        PyList_Append(node_pairs, PyLong_FromUnsignedLongLong(total_cardinality - prev_total));

        // Update counters
        for (npy_intp i = 0; i < N; i++) {
            new_hll_array[i] = hll_init(p, 42, false, 0, 0);
            if (!new_hll_array[i]) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to initialize new HyperLogLog counter");
                return NULL;
            }
            
            // Copy current counter
            hll_merge(new_hll_array[i], hll_array[i]);

            // Merge with neighbors
            for (npy_intp j = 0; j < N; j++) {
                uint8_t value = *(uint8_t*)PyArray_GETPTR2(adjacency_matrix, i, j);
                if (value) {
                    hll_merge(new_hll_array[i], hll_array[j]);
                }
            }

            // Check if counter changed
            if (hll_cardinality(new_hll_array[i]) != hll_cardinality(hll_array[i])) {
                changed = true;
            }
        }

        // Free old counters and update
        for (npy_intp i = 0; i < N; i++) {
            hll_free(hll_array[i]);
        }
        free(hll_array);
        hll_array = new_hll_array;
        t++;

    } while (changed);

    // Calculate average graph distance
    double avg_distance = 0.0;
    double total_pairs = 0.0;
    Py_ssize_t num_steps = PyList_Size(node_pairs);
    
    for (Py_ssize_t i = 0; i < num_steps; i++) {
        PyObject* pairs = PyList_GetItem(node_pairs, i);
        double pair_count = (double)PyLong_AsUnsignedLongLong(pairs);
        total_pairs += pair_count;
        avg_distance += (i + 1) * pair_count;
    }
    
    avg_distance = avg_distance / total_pairs;

    // Clean up
    for (npy_intp i = 0; i < N; i++) {
        hll_free(hll_array[i]);
    }
    free(hll_array);
    Py_DECREF(NFs);
    Py_DECREF(node_pairs);

    return PyFloat_FromDouble(avg_distance);
}

static PyMethodDef HllMethods[] = {
    {"hyperanf", py_hyperanf, METH_VARARGS, "Compute approximate neighborhood function using HyperANF."},
    {"hyperanf_distance", py_hyperanf_distance, METH_VARARGS, "Compute average graph distance using HyperANF."},
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
    import_array(); // Required for numpy integration
    return PyModule_Create(&hllmodule);
}
