#include "Python.h"

typedef struct {
    PyObject_HEAD
    char *buf;
    Py_ssize_t pos;
    Py_ssize_t string_size;
    size_t buf_size;
} BytesIOObject;

#define CHECK_CLOSED(self)                                  \
    if ((self)->buf == NULL) {                              \
        PyErr_SetString(PyExc_ValueError,                   \
                        "I/O operation on closed file.");   \
        return NULL;                                        \
    }

/* Internal routine to get a line from the buffer of a BytesIO
   object. Returns the length between the current position to the
   next newline character. */
static Py_ssize_t
get_line(BytesIOObject *self, char **output)
{
    char *n;
    const char *str_end;
    Py_ssize_t len;

    assert(self->buf != NULL);

    /* Move to the end of the line, up to the end of the string, s. */
    str_end = self->buf + self->string_size;
    for (n = self->buf + self->pos;
         n < str_end && *n != '\n';
         n++);

    /* Skip the newline character */
    if (n < str_end)
        n++;

    /* Get the length from the current position to the end of the line. */
    len = n - (self->buf + self->pos);
    *output = self->buf + self->pos;

    assert(len >= 0);
    assert(self->pos < PY_SSIZE_T_MAX - len);
    self->pos += len;

    return len;
}

/* Internal routine for changing the size of the buffer of BytesIO objects.
   The caller should ensure that the 'size' argument is non-negative.  Returns
   0 on success, -1 otherwise. */
static int
resize_buffer(BytesIOObject *self, size_t size)
{
    /* Here, unsigned types are used to avoid dealing with signed integer
       overflow, which is undefined in C. */
    size_t alloc = self->buf_size;
    char *new_buf = NULL;

    assert(self->buf != NULL);

    /* For simplicity, stay in the range of the signed type. Anyway, Python
       doesn't allow strings to be longer than this. */
    if (size > PY_SSIZE_T_MAX)
        goto overflow;

    if (size < alloc / 2) {
        /* Major downsize; resize down to exact size. */
        alloc = size + 1;
    }
    else if (size < alloc) {
        /* Within allocated size; quick exit */
        return 0;
    }
    else if (size <= alloc * 1.125) {
        /* Moderate upsize; overallocate similar to list_resize() */
        alloc = size + (size >> 3) + (size < 9 ? 3 : 6);
    }
    else {
        /* Major upsize; resize up to exact size */
        alloc = size + 1;
    }

    if (alloc > ((size_t)-1) / sizeof(char))
        goto overflow;
    new_buf = (char *)PyMem_Realloc(self->buf, alloc * sizeof(char));
    if (new_buf == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->buf_size = alloc;
    self->buf = new_buf;

    return 0;

  overflow:
    PyErr_SetString(PyExc_OverflowError,
                    "new buffer size too large");
    return -1;
}

/* Internal routine for writing a string of bytes to the buffer of a BytesIO
   object. Returns the number of bytes wrote, or -1 on error. */
static Py_ssize_t
write_bytes(BytesIOObject *self, const char *bytes, Py_ssize_t len)
{
    assert(self->buf != NULL);
    assert(self->pos >= 0);
    assert(len >= 0);

    if ((size_t)self->pos + len > self->buf_size) {
        if (resize_buffer(self, (size_t)self->pos + len) < 0)
            return -1;
    }

    if (self->pos > self->string_size) {
        /* In case of overseek, pad with null bytes the buffer region between
           the end of stream and the current position.

          0   lo      string_size                           hi
          |   |<---used--->|<----------available----------->|
          |   |            <--to pad-->|<---to write--->    |
          0   buf                   position
        */
        memset(self->buf + self->string_size, '\0',
               (self->pos - self->string_size) * sizeof(char));
    }

    /* Copy the data to the internal buffer, overwriting some of the existing
       data if self->pos < self->string_size. */
    memcpy(self->buf + self->pos, bytes, len);
    self->pos += len;

    /* Set the new length of the internal string if it has changed. */
    if (self->string_size < self->pos) {
        self->string_size = self->pos;
    }

    return len;
}

static PyObject *
bytesio_get_closed(BytesIOObject *self)
{
    if (self->buf == NULL)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

/* Generic getter for the writable, readable and seekable properties */
static PyObject *
return_true(BytesIOObject *self)
{
    Py_RETURN_TRUE;
}

PyDoc_STRVAR(flush_doc,
"flush() -> None.  Does nothing.");

static PyObject *
bytesio_flush(BytesIOObject *self)
{
    Py_RETURN_NONE;
}

PyDoc_STRVAR(getval_doc,
"getvalue() -> bytes.\n"
"\n"
"Retrieve the entire contents of the BytesIO object.");

static PyObject *
bytesio_getvalue(BytesIOObject *self)
{
    CHECK_CLOSED(self);
    return PyBytes_FromStringAndSize(self->buf, self->string_size);
}

PyDoc_STRVAR(isatty_doc,
"isatty() -> False.\n"
"\n"
"Always returns False since BytesIO objects are not connected\n"
"to a tty-like device.");

static PyObject *
bytesio_isatty(BytesIOObject *self)
{
    CHECK_CLOSED(self);
    Py_RETURN_FALSE;
}

PyDoc_STRVAR(tell_doc,
"tell() -> current file position, an integer\n");

static PyObject *
bytesio_tell(BytesIOObject *self)
{
    CHECK_CLOSED(self);
    return PyLong_FromSsize_t(self->pos);
}

PyDoc_STRVAR(read_doc,
"read([size]) -> read at most size bytes, returned as a string.\n"
"\n"
"If the size argument is negative, read until EOF is reached.\n"
"Return an empty string at EOF.");

static PyObject *
bytesio_read(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t size, n;
    char *output;
    PyObject *arg = Py_None;

    CHECK_CLOSED(self);

    if (!PyArg_ParseTuple(args, "|O:read", &arg))
        return NULL;

    if (PyLong_Check(arg)) {
        size = PyLong_AsSsize_t(arg);
        if (size == -1 && PyErr_Occurred())
            return NULL;
    }
    else if (arg == Py_None) {
        /* Read until EOF is reached, by default. */
        size = -1;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got '%s'",
                     Py_TYPE(arg)->tp_name);
        return NULL;
    }

    /* adjust invalid sizes */
    n = self->string_size - self->pos;
    if (size < 0 || size > n) {
        size = n;
        if (size < 0)
            size = 0;
    }

    assert(self->buf != NULL);
    output = self->buf + self->pos;
    self->pos += size;

    return PyBytes_FromStringAndSize(output, size);
}


PyDoc_STRVAR(read1_doc,
"read1(size) -> read at most size bytes, returned as a string.\n"
"\n"
"If the size argument is negative or omitted, read until EOF is reached.\n"
"Return an empty string at EOF.");

static PyObject *
bytesio_read1(BytesIOObject *self, PyObject *n)
{
    PyObject *arg, *res;

    arg = PyTuple_Pack(1, n);
    if (arg == NULL)
        return NULL;
    res  = bytesio_read(self, arg);
    Py_DECREF(arg);
    return res;
}

PyDoc_STRVAR(readline_doc,
"readline([size]) -> next line from the file, as a string.\n"
"\n"
"Retain newline.  A non-negative size argument limits the maximum\n"
"number of bytes to return (an incomplete line may be returned then).\n"
"Return an empty string at EOF.\n");

static PyObject *
bytesio_readline(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t size, n;
    char *output;
    PyObject *arg = Py_None;

    CHECK_CLOSED(self);

    if (!PyArg_ParseTuple(args, "|O:readline", &arg))
        return NULL;

    if (PyLong_Check(arg)) {
        size = PyLong_AsSsize_t(arg);
        if (size == -1 && PyErr_Occurred())
            return NULL;
    }
    else if (arg == Py_None) {
        /* No size limit, by default. */
        size = -1;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got '%s'",
                     Py_TYPE(arg)->tp_name);
        return NULL;
    }

    n = get_line(self, &output);

    if (size >= 0 && size < n) {
        size = n - size;
        n -= size;
        self->pos -= size;
    }

    return PyBytes_FromStringAndSize(output, n);
}

PyDoc_STRVAR(readlines_doc,
"readlines([size]) -> list of strings, each a line from the file.\n"
"\n"
"Call readline() repeatedly and return a list of the lines so read.\n"
"The optional size argument, if given, is an approximate bound on the\n"
"total number of bytes in the lines returned.\n");

static PyObject *
bytesio_readlines(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t maxsize, size, n;
    PyObject *result, *line;
    char *output;
    PyObject *arg = Py_None;

    CHECK_CLOSED(self);

    if (!PyArg_ParseTuple(args, "|O:readlines", &arg))
        return NULL;

    if (PyLong_Check(arg)) {
        maxsize = PyLong_AsSsize_t(arg);
        if (maxsize == -1 && PyErr_Occurred())
            return NULL;
    }
    else if (arg == Py_None) {
        /* No size limit, by default. */
        maxsize = -1;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got '%s'",
                     Py_TYPE(arg)->tp_name);
        return NULL;
    }

    size = 0;
    result = PyList_New(0);
    if (!result)
        return NULL;

    while ((n = get_line(self, &output)) != 0) {
        line = PyBytes_FromStringAndSize(output, n);
        if (!line)
            goto on_error;
        if (PyList_Append(result, line) == -1) {
            Py_DECREF(line);
            goto on_error;
        }
        Py_DECREF(line);
        size += n;
        if (maxsize > 0 && size >= maxsize)
            break;
    }
    return result;

  on_error:
    Py_DECREF(result);
    return NULL;
}

PyDoc_STRVAR(readinto_doc,
"readinto(bytearray) -> int.  Read up to len(b) bytes into b.\n"
"\n"
"Returns number of bytes read (0 for EOF), or None if the object\n"
"is set not to block as has no data to read.");

static PyObject *
bytesio_readinto(BytesIOObject *self, PyObject *buffer)
{
    void *raw_buffer;
    Py_ssize_t len;

    CHECK_CLOSED(self);

    if (PyObject_AsWriteBuffer(buffer, &raw_buffer, &len) == -1)
        return NULL;

    if (self->pos + len > self->string_size)
        len = self->string_size - self->pos;

    memcpy(raw_buffer, self->buf + self->pos, len);
    assert(self->pos + len < PY_SSIZE_T_MAX);
    assert(len >= 0);
    self->pos += len;

    return PyLong_FromSsize_t(len);
}

PyDoc_STRVAR(truncate_doc,
"truncate([size]) -> int.  Truncate the file to at most size bytes.\n"
"\n"
"Size defaults to the current file position, as returned by tell().\n"
"Returns the new size.  Imply an absolute seek to the position size.");

static PyObject *
bytesio_truncate(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t size;
    PyObject *arg = Py_None;

    CHECK_CLOSED(self);

    if (!PyArg_ParseTuple(args, "|O:truncate", &arg))
        return NULL;

    if (PyLong_Check(arg)) {
        size = PyLong_AsSsize_t(arg);
        if (size == -1 && PyErr_Occurred())
            return NULL;
    }
    else if (arg == Py_None) {
        /* Truncate to current position if no argument is passed. */
        size = self->pos;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got '%s'",
                     Py_TYPE(arg)->tp_name);
        return NULL;
    }

    if (size < 0) {
        PyErr_Format(PyExc_ValueError,
                     "negative size value %zd", size);
        return NULL;
    }

    if (size < self->string_size) {
        self->string_size = size;
        if (resize_buffer(self, size) < 0)
            return NULL;
    }
    self->pos = size;

    return PyLong_FromSsize_t(size);
}

static PyObject *
bytesio_iternext(BytesIOObject *self)
{
    char *next;
    Py_ssize_t n;

    CHECK_CLOSED(self);

    n = get_line(self, &next);

    if (!next || n == 0)
        return NULL;

    return PyBytes_FromStringAndSize(next, n);
}

PyDoc_STRVAR(seek_doc,
"seek(pos, whence=0) -> int.  Change stream position.\n"
"\n"
"Seek to byte offset pos relative to position indicated by whence:\n"
"     0  Start of stream (the default).  pos should be >= 0;\n"
"     1  Current position - pos may be negative;\n"
"     2  End of stream - pos usually negative.\n"
"Returns the new absolute position.");

static PyObject *
bytesio_seek(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t pos;
    int mode = 0;

    CHECK_CLOSED(self);

    if (!PyArg_ParseTuple(args, "n|i:seek", &pos, &mode))
        return NULL;

    if (pos < 0 && mode == 0) {
        PyErr_Format(PyExc_ValueError,
                     "negative seek value %zd", pos);
        return NULL;
    }

    /* mode 0: offset relative to beginning of the string.
       mode 1: offset relative to current position.
       mode 2: offset relative the end of the string. */
    if (mode == 1) {
        if (pos > PY_SSIZE_T_MAX - self->pos) {
            PyErr_SetString(PyExc_OverflowError,
                            "new position too large");
            return NULL;
        }
        pos += self->pos;
    }
    else if (mode == 2) {
        if (pos > PY_SSIZE_T_MAX - self->string_size) {
            PyErr_SetString(PyExc_OverflowError,
                            "new position too large");
            return NULL;
        }
        pos += self->string_size;
    }
    else if (mode != 0) {
        PyErr_Format(PyExc_ValueError,
                     "invalid whence (%i, should be 0, 1 or 2)", mode);
        return NULL;
    }

    if (pos < 0)
        pos = 0;
    self->pos = pos;

    return PyLong_FromSsize_t(self->pos);
}

PyDoc_STRVAR(write_doc,
"write(bytes) -> int.  Write bytes to file.\n"
"\n"
"Return the number of bytes written.");

static PyObject *
bytesio_write(BytesIOObject *self, PyObject *obj)
{
    const char *bytes;
    Py_ssize_t size;
    Py_ssize_t n = 0;

    CHECK_CLOSED(self);

    if (PyObject_AsReadBuffer(obj, (void *)&bytes, &size) < 0)
        return NULL;

    if (size != 0) {
        n = write_bytes(self, bytes, size);
        if (n < 0)
            return NULL;
    }

    return PyLong_FromSsize_t(n);
}

PyDoc_STRVAR(writelines_doc,
"writelines(sequence_of_strings) -> None.  Write strings to the file.\n"
"\n"
"Note that newlines are not added.  The sequence can be any iterable\n"
"object producing strings. This is equivalent to calling write() for\n"
"each string.");

static PyObject *
bytesio_writelines(BytesIOObject *self, PyObject *v)
{
    PyObject *it, *item;
    PyObject *ret;

    CHECK_CLOSED(self);

    it = PyObject_GetIter(v);
    if (it == NULL)
        return NULL;

    while ((item = PyIter_Next(it)) != NULL) {
        ret = bytesio_write(self, item);
        Py_DECREF(item);
        if (ret == NULL) {
            Py_DECREF(it);
            return NULL;
        }
        Py_DECREF(ret);
    }
    Py_DECREF(it);

    /* See if PyIter_Next failed */
    if (PyErr_Occurred())
        return NULL;

    Py_RETURN_NONE;
}

PyDoc_STRVAR(close_doc,
"close() -> None.  Disable all I/O operations.");

static PyObject *
bytesio_close(BytesIOObject *self)
{
    if (self->buf != NULL) {
        PyMem_Free(self->buf);
        self->buf = NULL;
    }
    Py_RETURN_NONE;
}

static void
bytesio_dealloc(BytesIOObject *self)
{
    if (self->buf != NULL) {
        PyMem_Free(self->buf);
        self->buf = NULL;
    }
    Py_TYPE(self)->tp_free(self);
}

static PyObject *
bytesio_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    BytesIOObject *self;

    assert(type != NULL && type->tp_alloc != NULL);
    self = (BytesIOObject *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    self->string_size = 0;
    self->pos = 0;
    self->buf_size = 0;
    self->buf = (char *)PyMem_Malloc(0);
    if (self->buf == NULL) {
        Py_DECREF(self);
        return PyErr_NoMemory();
    }

    return (PyObject *)self;
}

static int
bytesio_init(BytesIOObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *initvalue = NULL;

    if (!PyArg_ParseTuple(args, "|O:BytesIO", &initvalue))
        return -1;

    /* In case, __init__ is called multiple times. */
    self->string_size = 0;
    self->pos = 0;

    if (initvalue && initvalue != Py_None) {
        PyObject *res;
        res = bytesio_write(self, initvalue);
        if (res == NULL)
            return -1;
        Py_DECREF(res);
        self->pos = 0;
    }

    return 0;
}

static PyGetSetDef bytesio_getsetlist[] = {
    {"closed",  (getter)bytesio_get_closed, NULL,
     "True if the file is closed."},
    {0},            /* sentinel */
};

static struct PyMethodDef bytesio_methods[] = {
    {"readable",   (PyCFunction)return_true,        METH_NOARGS, NULL},
    {"seekable",   (PyCFunction)return_true,        METH_NOARGS, NULL},
    {"writable",   (PyCFunction)return_true,        METH_NOARGS, NULL},
    {"close",      (PyCFunction)bytesio_close,      METH_NOARGS, close_doc},
    {"flush",      (PyCFunction)bytesio_flush,      METH_NOARGS, flush_doc},
    {"isatty",     (PyCFunction)bytesio_isatty,     METH_NOARGS, isatty_doc},
    {"tell",       (PyCFunction)bytesio_tell,       METH_NOARGS, tell_doc},
    {"write",      (PyCFunction)bytesio_write,      METH_O, write_doc},
    {"writelines", (PyCFunction)bytesio_writelines, METH_O, writelines_doc},
    {"read1",      (PyCFunction)bytesio_read1,      METH_O, read1_doc},
    {"readinto",   (PyCFunction)bytesio_readinto,   METH_O, readinto_doc},
    {"readline",   (PyCFunction)bytesio_readline,   METH_VARARGS, readline_doc},
    {"readlines",  (PyCFunction)bytesio_readlines,  METH_VARARGS, readlines_doc},
    {"read",       (PyCFunction)bytesio_read,       METH_VARARGS, read_doc},
    {"getvalue",   (PyCFunction)bytesio_getvalue,   METH_VARARGS, getval_doc},
    {"seek",       (PyCFunction)bytesio_seek,       METH_VARARGS, seek_doc},
    {"truncate",   (PyCFunction)bytesio_truncate,   METH_VARARGS, truncate_doc},
    {NULL, NULL}        /* sentinel */
};

PyDoc_STRVAR(bytesio_doc,
"BytesIO([buffer]) -> object\n"
"\n"
"Create a buffered I/O implementation using an in-memory bytes\n"
"buffer, ready for reading and writing.");

static PyTypeObject BytesIO_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_bytesio._BytesIO",                       /*tp_name*/
    sizeof(BytesIOObject),                     /*tp_basicsize*/
    0,                                         /*tp_itemsize*/
    (destructor)bytesio_dealloc,               /*tp_dealloc*/
    0,                                         /*tp_print*/
    0,                                         /*tp_getattr*/
    0,                                         /*tp_setattr*/
    0,                                         /*tp_compare*/
    0,                                         /*tp_repr*/
    0,                                         /*tp_as_number*/
    0,                                         /*tp_as_sequence*/
    0,                                         /*tp_as_mapping*/
    0,                                         /*tp_hash*/
    0,                                         /*tp_call*/
    0,                                         /*tp_str*/
    0,                                         /*tp_getattro*/
    0,                                         /*tp_setattro*/
    0,                                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /*tp_flags*/
    bytesio_doc,                               /*tp_doc*/
    0,                                         /*tp_traverse*/
    0,                                         /*tp_clear*/
    0,                                         /*tp_richcompare*/
    0,                                         /*tp_weaklistoffset*/
    PyObject_SelfIter,                         /*tp_iter*/
    (iternextfunc)bytesio_iternext,            /*tp_iternext*/
    bytesio_methods,                           /*tp_methods*/
    0,                                         /*tp_members*/
    bytesio_getsetlist,                        /*tp_getset*/
    0,                                         /*tp_base*/
    0,                                         /*tp_dict*/
    0,                                         /*tp_descr_get*/
    0,                                         /*tp_descr_set*/
    0,                                         /*tp_dictoffset*/
    (initproc)bytesio_init,                    /*tp_init*/
    0,                                         /*tp_alloc*/
    bytesio_new,                               /*tp_new*/
};


static struct PyModuleDef _bytesiomodule = {
	PyModuleDef_HEAD_INIT,
	"_bytesio",
	NULL,
	-1,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

PyMODINIT_FUNC
PyInit__bytesio(void)
{
    PyObject *m;

    if (PyType_Ready(&BytesIO_Type) < 0)
        return NULL;
    m = PyModule_Create(&_bytesiomodule);
    if (m == NULL)
        return NULL;
    Py_INCREF(&BytesIO_Type);
    PyModule_AddObject(m, "_BytesIO", (PyObject *)&BytesIO_Type);
    return m;
}
