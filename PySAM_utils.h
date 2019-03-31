#ifndef PYSAM_SAM_UTILS_H
#define PYSAM_SAM_UTILS_H

#include <Python.h>
#include <stdio.h>
//
// Runtime linking to SAM shared library
//

static char* SAM_lib_path = NULL;
#if defined(__WINDOWS__) || defined(__CYGWIN__)
static const char SAM_sep = '\\';
static char* SAM_lib = "SAM_apid.lib";
#else
static const char SAM_sep = '/';
static char* SAM_lib = "libSAM_apid.so";
#endif

static void* SAM_lib_handle = NULL;

//
// Error Handling
//

static PyObject *PySAM_ErrorObject;

static int PySAM_has_error(SAM_error error){
    const char* cc = error_message(error);
    if ((cc != NULL) && (cc[0] != '\0')) {
        PyErr_SetString(PySAM_ErrorObject, cc);
        error_destruct(error);
        return 1;
    }
    error_destruct(error);
    return 0;
}

static int PySAM_has_error_msg(SAM_error error, const char *msg){
    const char* cc = error_message(error);
    if ((cc != NULL) && (cc[0] != '\0')) {
        char err_msg[256];
        strncat(err_msg, cc, strlen(err_msg) - 1);
        strncat(err_msg, ". ", 2);
        strncat(err_msg, msg, strlen(msg));
        PyErr_SetString(PySAM_ErrorObject, err_msg);
        error_destruct(error);
        return 1;
    }
    error_destruct(error);
    return 0;
}

//
// Macros for defining attribute dictionaries and methods
//

#define PySAM_TECH_ATTR(tech, ctor) \
if (self == NULL) return NULL; SAM_error error = new_error(); self->data_ptr = (*ctor)(0, &error); \
if (PySAM_has_error(error)) return NULL; \
PyObject* attr_dict = PyDict_New(); self->x_attr = attr_dict; \
PyDict_SetItemString(attr_dict, "technology", PyUnicode_FromString(tech));

#define PySAM_GET_ATTR() \
if (self->x_attr != NULL) { PyObject *v = PyDict_GetItemWithError(self->x_attr, name); \
    if (v != NULL) { Py_INCREF(v); return v; } \
    else if (PyErr_Occurred()) { return NULL; }  \
    return PyObject_GenericGetAttr((PyObject *)self, name); }

#define PySAM_SET_ATTR() \
if (self->x_attr == NULL) { self->x_attr = PyDict_New(); \
    if (self->x_attr == NULL) return -1; } \
if (v == NULL) { int rv = PyDict_DelItemString(self->x_attr, name); \
    if (rv < 0 && PyErr_ExceptionMatches(PyExc_KeyError)) \
    PyErr_SetString(PyExc_AttributeError, "delete non-existing attribute"); return rv; } \
else return PyDict_SetItemString(self->x_attr, name, v);

//
// Macros for defining SAM attributes getters and setters
//

#define PySAM_FLOAT_GETTER(func) \
double val; SAM_error error = new_error(); \
val = (*func)(self->data_ptr, &error); \
if (PySAM_has_error(error)) return NULL;  \
PyObject* result = PyFloat_FromDouble(val); Py_XINCREF(result);\
return result;

#define PySAM_FLOAT_SETTER(func) \
if (value == NULL) { PyErr_SetString(PyExc_TypeError, "No value provided"); return -1;} \
if (!PyNumber_Float(value)) { PyErr_SetString(PyExc_TypeError, "Value must be numeric"); return -1; } \
float val = (float)PyFloat_AsDouble(value); SAM_error error = new_error(); \
(*func)(self->data_ptr, val, &error); \
if (PySAM_has_error(error)) return -1;  return 0; \

static PyObject* PySAM_array_getter(SAM_get_array_t func,void *data_ptr){
    float* arr;
    int seqlen;
    int i = 0;

    SAM_error error = new_error();
    arr = (*func)(data_ptr, &seqlen, &error);
    if (PySAM_has_error(error)) return NULL;

    PyObject* seq = PyTuple_New(seqlen);
    for(i=0; i < seqlen; i++) {
        PyTuple_SetItem(seq, i, PyFloat_FromDouble(arr[i]));
    }
    return seq;
}

static int PySAM_array_setter(PyObject *value, SAM_set_array_t func, void *data_ptr) {
    PyObject* seq;
    float *arr;
    int seqlen;
    int i;

    seq = PySequence_Fast(value, "argument must be iterable");
    if(!seq)
        return -1;

    seqlen = PySequence_Fast_GET_SIZE(seq);
    arr = malloc(seqlen*sizeof(float));
    if(!arr) {
        Py_DECREF(seq);
        PyErr_NoMemory(  );
        return -1;
    }
    for(i=0; i < seqlen; i++) {
        PyObject *fitem;
        PyObject *item = PySequence_Fast_GET_ITEM(seq, i);
        if(!item) {
            Py_DECREF(seq);
            free(arr);
            return -1;
        }
        if(!PyNumber_Check(item)) {
            Py_DECREF(seq);
            free(arr);
            PyErr_SetString(PyExc_TypeError, "all items must be numbers");
            return -1;
        }
        fitem = PyNumber_Float(item);
        arr[i] = (float)PyFloat_AS_DOUBLE(fitem);
        Py_DECREF(fitem);
    }

    Py_DECREF(seq);

    SAM_error error = new_error();
    (*func)(data_ptr, arr, seqlen, &error);
    if (PySAM_has_error(error)){
        free(arr);
        return -1;
    }
    free(arr);
    return 0;
}

//
// Methods for assigning class attributes from a dictionary
//

static void PySAM_concat_msg(char *dest, const char *first, char *second) {
    memcpy(dest, first, strlen(first));
    strncat(dest, second, strlen(second));
}

static int PySAM_assign_from_dict(void *data_ptr, PyObject *dict, const char *tech, const char *group){
    if (SAM_lib_handle == NULL){
        SAM_error error = new_error();
        SAM_lib_handle = SAM_load_library(SAM_lib_path, &error);
        if (PySAM_has_error(error)) return 0;
    }
    Py_INCREF(dict);

    PyObject* key;
    PyObject* value;
    Py_ssize_t pos = 0;

    PyObject* ascii_mystring = NULL;
    float* mat = NULL;
    float* arr = NULL;

    while (PyDict_Next(dict, &pos, &key, &value)){
        ascii_mystring = PyUnicode_AsASCIIString(key);
        char* name = PyBytes_AsString(ascii_mystring);

        // numeric
        if (PyNumber_Check(value)){
            SAM_error error = new_error();
            SAM_set_float_t func = SAM_set_float_func(SAM_lib_handle, "GenericSystem", "PowerPlant", name, &error);
            if (PySAM_has_error_msg(error, "Either parameter does not exist or is not numeric type.")) goto fail;

            float val = (float)PyFloat_AsDouble(value);

            error = new_error();
            func(data_ptr, val, &error);
            if (PySAM_has_error(error)) goto fail;
        }
        // string
        else if (PyUnicode_Check(value)){
            SAM_error error = new_error();
            SAM_set_string_t func = SAM_set_string_func(SAM_lib_handle, "GenericSystem", "PowerPlant", name, &error);
            if (PySAM_has_error_msg(error, "Either parameter does not exist or is not string.")) goto fail;

            PyObject* ascii_val = PyUnicode_AsASCIIString(value);
            char* val = PyBytes_AsString(ascii_val); \

            error = new_error();
            func(data_ptr, val, &error);
            if (PySAM_has_error(error)) goto fail;

            Py_DECREF(ascii_val);
        }
        // sequences
        else if (PySequence_Check(value)){
            PyObject* first = PySequence_GetItem(value, 0);
            if (!first){
                char str[256];
                PySAM_concat_msg(str, "Error assigning empty tuple to ", name);
                PyErr_SetString(PySAM_ErrorObject, str);
                Py_XDECREF(first);
                goto fail;
            }

            Py_ssize_t n = PySequence_Size(value);

            // matrix
            if (PySequence_Check(first)){
                SAM_error error = new_error();
                SAM_set_matrix_t func = SAM_set_matrix_func(SAM_lib_handle, "GenericSystem", "PowerPlant", name, &error);
                if (PySAM_has_error_msg(error, "Either parameter does not exist or is not matrix type.")) goto fail;

                Py_ssize_t cols = PySequence_Size(first);
                mat = malloc(n*cols*sizeof(float));

                for (Py_ssize_t i = 0; i < n; i++){
                    PyObject* row = PySequence_GetItem(value, i);

                    if (PySequence_Size(row) != cols){
                        char str[256];
                        PySAM_concat_msg(str, name, " matrix must be rectangular");
                        PyErr_SetString(PySAM_ErrorObject, str);
                        goto fail;
                    }
                    for (Py_ssize_t j = 0; j < cols; j++){
                        PyObject* val_o = PySequence_GetItem(row, j);

                        if (!PyNumber_Check(val_o)){
                            char str[256];

                            PySAM_concat_msg(str, name, " must have numeric matrix entries.");
                            PyErr_SetString(PySAM_ErrorObject, str);
                            Py_XDECREF(val_o);
                            goto fail;
                        }
                        float val = (float)PyFloat_AsDouble(val_o);
                        Py_XDECREF(val_o);
                        mat[i * cols + j] = val;

                    }
                    Py_XDECREF(row);
                }

                error = new_error();
                func(data_ptr, mat, (int)n, (int)cols, &error);
                if (PySAM_has_error(error)) goto fail;

                free(mat);
            }
            // array
            else{
                SAM_error error = new_error();
                SAM_set_array_t func = SAM_set_array_func(SAM_lib_handle, "GenericSystem", "PowerPlant", name, &error);
                if (PySAM_has_error_msg(error, "Either parameter does not exist or is not array type.")) goto fail;

                arr = malloc(n*sizeof(float));

                for (Py_ssize_t i = 0; i < n; i++){
                    PyObject* val_o = PySequence_GetItem(value, i);
                    Py_XINCREF(val_o);

                    float val = (float)PyFloat_AsDouble(val_o);

                    if (PyErr_Occurred()){
                        char str[256];
                        PySAM_concat_msg(str, name, " array entries must be convertable to numbers");
                        PyErr_SetString(PySAM_ErrorObject, str);
                        Py_XDECREF(val_o);
                        goto fail;
                    }
                    Py_XDECREF(val_o);
                    arr[i] = val;
                }

                error = new_error();
                func(data_ptr, arr, (int)n, &error);
                if (PySAM_has_error(error)) goto fail;
                free(arr);
            }
        }
        else {
            char str[256];
            PySAM_concat_msg(str, name, " assignment value must be numeric, string or tuple.");
            PyErr_SetString(PySAM_ErrorObject, str);
            goto fail;
        }
        Py_DECREF(ascii_mystring);
    }
    Py_XDECREF(dict);
    return 1;
    fail:
    Py_XDECREF(ascii_mystring);
    Py_XDECREF(dict);
    if (mat) free(mat);
    if (arr) free(arr);
    return 0;
}

//
// Methods for reading class attributes into a dictionary
//

static PyObject *
PySAM_export_to_dict(void *data_ptr, PyObject *self, PyTypeObject *tp, const char *tech, const char *group) {

    if (SAM_lib_handle == NULL){
        SAM_error error = new_error();
        SAM_lib_handle = SAM_load_library(SAM_lib_path, &error);
        if (PySAM_has_error(error)) return 0;
    }

    PyObject* export = PyDict_New();
    if (!export){
        PyErr_NoMemory();
        return NULL;
    }

    PyGetSetDef* getset = tp->tp_getset;
    while(getset->name){
        PyObject* val = (*getset->get)(self, NULL);
        PyDict_SetItemString(export, getset->name, val);
        getset++;
    }

    return export;
}


#endif //PYSAM_SAM_UTILS_H
