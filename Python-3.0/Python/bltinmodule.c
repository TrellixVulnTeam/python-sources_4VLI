/* Built-in functions */

#include "Python.h"
#include "Python-ast.h"

#include "node.h"
#include "code.h"
#include "eval.h"

#include <ctype.h>

/* The default encoding used by the platform file system APIs
   Can remain NULL for all platforms that don't have such a concept

   Don't forget to modify PyUnicode_DecodeFSDefault() if you touch any of the
   values for Py_FileSystemDefaultEncoding!
*/
#if defined(MS_WINDOWS) && defined(HAVE_USABLE_WCHAR_T)
const char *Py_FileSystemDefaultEncoding = "mbcs";
int Py_HasFileSystemDefaultEncoding = 1;
#elif defined(__APPLE__)
const char *Py_FileSystemDefaultEncoding = "utf-8";
int Py_HasFileSystemDefaultEncoding = 1;
#else
const char *Py_FileSystemDefaultEncoding = NULL; /* use default */
int Py_HasFileSystemDefaultEncoding = 0;
#endif

int
_Py_SetFileSystemEncoding(PyObject *s)
{
	PyObject *defenc;
	if (!PyUnicode_Check(s)) {
		PyErr_BadInternalCall();
		return -1;
	}
	defenc = _PyUnicode_AsDefaultEncodedString(s, NULL);
	if (!defenc)
		return -1;
	if (!Py_HasFileSystemDefaultEncoding && Py_FileSystemDefaultEncoding)
		/* A file system encoding was set at run-time */
		free((char*)Py_FileSystemDefaultEncoding);
	Py_FileSystemDefaultEncoding = strdup(PyBytes_AsString(defenc));
	Py_HasFileSystemDefaultEncoding = 0;
	return 0;
}

static PyObject *
builtin___build_class__(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *func, *name, *bases, *mkw, *meta, *prep, *ns, *cell;
	PyObject *cls = NULL;
	Py_ssize_t nargs, nbases;

	assert(args != NULL);
	if (!PyTuple_Check(args)) {
		PyErr_SetString(PyExc_TypeError,
				"__build_class__: args is not a tuple");
		return NULL;
	}
	nargs = PyTuple_GET_SIZE(args);
	if (nargs < 2) {
		PyErr_SetString(PyExc_TypeError,
				"__build_class__: not enough arguments");
		return NULL;
	}
	func = PyTuple_GET_ITEM(args, 0); /* Better be callable */
	name = PyTuple_GET_ITEM(args, 1);
	if (!PyUnicode_Check(name)) {
		PyErr_SetString(PyExc_TypeError,
				"__build_class__: name is not a string");
		return NULL;
	}
	bases = PyTuple_GetSlice(args, 2, nargs);
	if (bases == NULL)
		return NULL;
	nbases = nargs - 2;

	if (kwds == NULL) {
		meta = NULL;
                mkw = NULL;
        }
	else {
		mkw = PyDict_Copy(kwds); /* Don't modify kwds passed in! */
		if (mkw == NULL) {
			Py_DECREF(bases);
			return NULL;
		}
		meta = PyDict_GetItemString(mkw, "metaclass");
		if (meta != NULL) {
			Py_INCREF(meta);
			if (PyDict_DelItemString(mkw, "metaclass") < 0) {
				Py_DECREF(meta);
				Py_DECREF(mkw);
				Py_DECREF(bases);
				return NULL;
			}
		}
	}
	if (meta == NULL) {
		if (PyTuple_GET_SIZE(bases) == 0)
			meta = (PyObject *) (&PyType_Type);
		else {
			PyObject *base0 = PyTuple_GET_ITEM(bases, 0);
			meta = (PyObject *) (base0->ob_type);
		}
		Py_INCREF(meta);
	}
	prep = PyObject_GetAttrString(meta, "__prepare__");
	if (prep == NULL) {
		PyErr_Clear();
		ns = PyDict_New();
	}
	else {
		PyObject *pargs = Py_BuildValue("OO", name, bases);
		if (pargs == NULL) {
			Py_DECREF(prep);
			Py_DECREF(meta);
			Py_XDECREF(mkw);
			Py_DECREF(bases);
			return NULL;
		}
		ns = PyEval_CallObjectWithKeywords(prep, pargs, mkw);
		Py_DECREF(pargs);
		Py_DECREF(prep);
		if (ns == NULL) {
			Py_DECREF(meta);
			Py_XDECREF(mkw);
			Py_DECREF(bases);
			return NULL;
		}
	}
	cell = PyObject_CallFunctionObjArgs(func, ns, NULL);
	if (cell != NULL) {
		PyObject *margs;
		margs = Py_BuildValue("OOO", name, bases, ns);
		if (margs != NULL) {
			cls = PyEval_CallObjectWithKeywords(meta, margs, mkw);
			Py_DECREF(margs);
		}
		if (cls != NULL && PyCell_Check(cell)) {
			Py_INCREF(cls);
			PyCell_SET(cell, cls);
		}
		Py_DECREF(cell);
	}
	Py_DECREF(ns);
	Py_DECREF(meta);
	Py_XDECREF(mkw);
	Py_DECREF(bases);
	return cls;
}

PyDoc_STRVAR(build_class_doc,
"__build_class__(func, name, *bases, metaclass=None, **kwds) -> class\n\
\n\
Internal helper function used by the class statement.");

static PyObject *
builtin___import__(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"name", "globals", "locals", "fromlist",
				 "level", 0};
	char *name;
	PyObject *globals = NULL;
	PyObject *locals = NULL;
	PyObject *fromlist = NULL;
	int level = -1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|OOOi:__import__",
			kwlist, &name, &globals, &locals, &fromlist, &level))
		return NULL;
	return PyImport_ImportModuleLevel(name, globals, locals,
					  fromlist, level);
}

PyDoc_STRVAR(import_doc,
"__import__(name, globals={}, locals={}, fromlist=[], level=-1) -> module\n\
\n\
Import a module.  The globals are only used to determine the context;\n\
they are not modified.  The locals are currently unused.  The fromlist\n\
should be a list of names to emulate ``from name import ...'', or an\n\
empty list to emulate ``import name''.\n\
When importing a module from a package, note that __import__('A.B', ...)\n\
returns package A when fromlist is empty, but its submodule B when\n\
fromlist is not empty.  Level is used to determine whether to perform \n\
absolute or relative imports.  -1 is the original strategy of attempting\n\
both absolute and relative imports, 0 is absolute, a positive number\n\
is the number of parent directories to search relative to the current module.");


static PyObject *
builtin_abs(PyObject *self, PyObject *v)
{
	return PyNumber_Absolute(v);
}

PyDoc_STRVAR(abs_doc,
"abs(number) -> number\n\
\n\
Return the absolute value of the argument.");

static PyObject *
builtin_all(PyObject *self, PyObject *v)
{
	PyObject *it, *item;
	PyObject *(*iternext)(PyObject *);
	int cmp;

	it = PyObject_GetIter(v);
	if (it == NULL)
		return NULL;
	iternext = *Py_TYPE(it)->tp_iternext;

	for (;;) {
		item = iternext(it);
		if (item == NULL)
			break;
		cmp = PyObject_IsTrue(item);
		Py_DECREF(item);
		if (cmp < 0) {
			Py_DECREF(it);
			return NULL;
		}
		if (cmp == 0) {
			Py_DECREF(it);
			Py_RETURN_FALSE;
		}
	}
	Py_DECREF(it);
	if (PyErr_Occurred()) {
		if (PyErr_ExceptionMatches(PyExc_StopIteration))
			PyErr_Clear();
		else
			return NULL;
	}
	Py_RETURN_TRUE;
}

PyDoc_STRVAR(all_doc,
"all(iterable) -> bool\n\
\n\
Return True if bool(x) is True for all values x in the iterable.");

static PyObject *
builtin_any(PyObject *self, PyObject *v)
{
	PyObject *it, *item;
	PyObject *(*iternext)(PyObject *);
	int cmp;

	it = PyObject_GetIter(v);
	if (it == NULL)
		return NULL;
	iternext = *Py_TYPE(it)->tp_iternext;

	for (;;) {
		item = iternext(it);
		if (item == NULL)
			break;
		cmp = PyObject_IsTrue(item);
		Py_DECREF(item);
		if (cmp < 0) {
			Py_DECREF(it);
			return NULL;
		}
		if (cmp == 1) {
			Py_DECREF(it);
			Py_RETURN_TRUE;
		}
	}
	Py_DECREF(it);
	if (PyErr_Occurred()) {
		if (PyErr_ExceptionMatches(PyExc_StopIteration))
			PyErr_Clear();
		else
			return NULL;
	}
	Py_RETURN_FALSE;
}

PyDoc_STRVAR(any_doc,
"any(iterable) -> bool\n\
\n\
Return True if bool(x) is True for any x in the iterable.");

static PyObject *
builtin_ascii(PyObject *self, PyObject *v)
{
	return PyObject_ASCII(v);
}

PyDoc_STRVAR(ascii_doc,
"ascii(object) -> string\n\
\n\
As repr(), return a string containing a printable representation of an\n\
object, but escape the non-ASCII characters in the string returned by\n\
repr() using \\x, \\u or \\U escapes.  This generates a string similar\n\
to that returned by repr() in Python 2.");


static PyObject *
builtin_bin(PyObject *self, PyObject *v)
{
	return PyNumber_ToBase(v, 2);
}

PyDoc_STRVAR(bin_doc,
"bin(number) -> string\n\
\n\
Return the binary representation of an integer or long integer.");


typedef struct {
	PyObject_HEAD
	PyObject *func;
	PyObject *it;
} filterobject;

PyTypeObject PyFilter_Type;

static PyObject *
filter_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *func, *seq;
	PyObject *it;
	filterobject *lz;

	if (type == &PyFilter_Type && !_PyArg_NoKeywords("filter()", kwds))
		return NULL;

	if (!PyArg_UnpackTuple(args, "filter", 2, 2, &func, &seq))
		return NULL;

	/* Get iterator. */
	it = PyObject_GetIter(seq);
	if (it == NULL)
		return NULL;

	/* create filterobject structure */
	lz = (filterobject *)type->tp_alloc(type, 0);
	if (lz == NULL) {
		Py_DECREF(it);
		return NULL;
	}
	Py_INCREF(func);
	lz->func = func;
	lz->it = it;

	return (PyObject *)lz;
}

static void
filter_dealloc(filterobject *lz)
{
	PyObject_GC_UnTrack(lz);
	Py_XDECREF(lz->func);
	Py_XDECREF(lz->it);
	Py_TYPE(lz)->tp_free(lz);
}

static int
filter_traverse(filterobject *lz, visitproc visit, void *arg)
{
	Py_VISIT(lz->it);
	Py_VISIT(lz->func);
	return 0;
}

static PyObject *
filter_next(filterobject *lz)
{
	PyObject *item;
	PyObject *it = lz->it;
	long ok;
	PyObject *(*iternext)(PyObject *);

	assert(PyIter_Check(it));
	iternext = *Py_TYPE(it)->tp_iternext;
	for (;;) {
		item = iternext(it);
		if (item == NULL)
			return NULL;

		if (lz->func == Py_None || lz->func == (PyObject *)&PyBool_Type) {
			ok = PyObject_IsTrue(item);
		} else {
			PyObject *good;
			good = PyObject_CallFunctionObjArgs(lz->func,
							    item, NULL);
			if (good == NULL) {
				Py_DECREF(item);
				return NULL;
			}
			ok = PyObject_IsTrue(good);
			Py_DECREF(good);
		}
		if (ok)
			return item;
		Py_DECREF(item);
	}
}

PyDoc_STRVAR(filter_doc,
"filter(function or None, iterable) --> filter object\n\
\n\
Return an iterator yielding those items of iterable for which function(item)\n\
is true. If function is None, return the items that are true.");

PyTypeObject PyFilter_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"filter",			/* tp_name */
	sizeof(filterobject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)filter_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	0,				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
		Py_TPFLAGS_BASETYPE,	/* tp_flags */
	filter_doc,			/* tp_doc */
	(traverseproc)filter_traverse,	/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc)filter_next,	/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	PyType_GenericAlloc,		/* tp_alloc */
	filter_new,			/* tp_new */
	PyObject_GC_Del,		/* tp_free */
};


static PyObject *
builtin_format(PyObject *self, PyObject *args)
{
    PyObject *value;
    PyObject *format_spec = NULL;

    if (!PyArg_ParseTuple(args, "O|U:format", &value, &format_spec))
        return NULL;

    return PyObject_Format(value, format_spec);
}

PyDoc_STRVAR(format_doc,
"format(value[, format_spec]) -> string\n\
\n\
Returns value.__format__(format_spec)\n\
format_spec defaults to \"\"");

static PyObject *
builtin_chr(PyObject *self, PyObject *args)
{
	int x;

	if (!PyArg_ParseTuple(args, "i:chr", &x))
		return NULL;

	return PyUnicode_FromOrdinal(x);
}

PyDoc_VAR(chr_doc) = PyDoc_STR(
"chr(i) -> Unicode character\n\
\n\
Return a Unicode string of one character with ordinal i; 0 <= i <= 0x10ffff."
)
#ifndef Py_UNICODE_WIDE
PyDoc_STR(
"\nIf 0x10000 <= i, a surrogate pair is returned."
)
#endif
;


static PyObject *
builtin_cmp(PyObject *self, PyObject *args)
{
	PyObject *a, *b;
	int c;

	if (!PyArg_UnpackTuple(args, "cmp", 2, 2, &a, &b))
		return NULL;
	if (PyObject_Cmp(a, b, &c) < 0)
		return NULL;
	return PyLong_FromLong((long)c);
}

PyDoc_STRVAR(cmp_doc,
"cmp(x, y) -> integer\n\
\n\
Return negative if x<y, zero if x==y, positive if x>y.");


static char *
source_as_string(PyObject *cmd, char *funcname, char *what)
{
	char *str;
	Py_ssize_t size;

	if (PyUnicode_Check(cmd)) {
		cmd = _PyUnicode_AsDefaultEncodedString(cmd, NULL);
		if (cmd == NULL)
			return NULL;
	}
	else if (!PyObject_CheckReadBuffer(cmd)) {
		PyErr_Format(PyExc_TypeError,
		  "%s() arg 1 must be a %s object",
		  funcname, what);
		return NULL;
	}
	if (PyObject_AsReadBuffer(cmd, (const void **)&str, &size) < 0) {
		return NULL;
	}
	if (strlen(str) != size) {
		PyErr_SetString(PyExc_TypeError,
				"source code string cannot contain null bytes");
		return NULL;
	}
	return str;
}

static PyObject *
builtin_compile(PyObject *self, PyObject *args, PyObject *kwds)
{
	char *str;
	char *filename;
	char *startstr;
	int mode = -1;
	int dont_inherit = 0;
	int supplied_flags = 0;
	PyCompilerFlags cf;
	PyObject *cmd;
	static char *kwlist[] = {"source", "filename", "mode", "flags",
				 "dont_inherit", NULL};
	int start[] = {Py_file_input, Py_eval_input, Py_single_input};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oss|ii:compile",
					 kwlist, &cmd, &filename, &startstr,
					 &supplied_flags, &dont_inherit))
		return NULL;

	cf.cf_flags = supplied_flags | PyCF_SOURCE_IS_UTF8;

	if (supplied_flags &
	    ~(PyCF_MASK | PyCF_MASK_OBSOLETE | PyCF_DONT_IMPLY_DEDENT | PyCF_ONLY_AST))
	{
		PyErr_SetString(PyExc_ValueError,
				"compile(): unrecognised flags");
		return NULL;
	}
	/* XXX Warn if (supplied_flags & PyCF_MASK_OBSOLETE) != 0? */

	if (!dont_inherit) {
		PyEval_MergeCompilerFlags(&cf);
	}

	if (strcmp(startstr, "exec") == 0)
		mode = 0;
	else if (strcmp(startstr, "eval") == 0)
		mode = 1;
	else if (strcmp(startstr, "single") == 0)
		mode = 2;
	else {
		PyErr_SetString(PyExc_ValueError,
				"compile() arg 3 must be 'exec', 'eval' or 'single'");
		return NULL;
	}

	if (PyAST_Check(cmd)) {
		PyObject *result;
		if (supplied_flags & PyCF_ONLY_AST) {
			Py_INCREF(cmd);
			result = cmd;
		}
		else {
			PyArena *arena;
			mod_ty mod;

			arena = PyArena_New();
			mod = PyAST_obj2mod(cmd, arena, mode);
			if (mod == NULL) {
				PyArena_Free(arena);
				return NULL;
			}
			result = (PyObject*)PyAST_Compile(mod, filename,
							  &cf, arena);
			PyArena_Free(arena);
		}
		return result;
	}

	str = source_as_string(cmd, "compile", "string, bytes, AST or code");
	if (str == NULL)
		return NULL;

	return Py_CompileStringFlags(str, filename, start[mode], &cf);
}

PyDoc_STRVAR(compile_doc,
"compile(source, filename, mode[, flags[, dont_inherit]]) -> code object\n\
\n\
Compile the source string (a Python module, statement or expression)\n\
into a code object that can be executed by exec() or eval().\n\
The filename will be used for run-time error messages.\n\
The mode must be 'exec' to compile a module, 'single' to compile a\n\
single (interactive) statement, or 'eval' to compile an expression.\n\
The flags argument, if present, controls which future statements influence\n\
the compilation of the code.\n\
The dont_inherit argument, if non-zero, stops the compilation inheriting\n\
the effects of any future statements in effect in the code calling\n\
compile; if absent or zero these statements do influence the compilation,\n\
in addition to any features explicitly specified.");

static PyObject *
builtin_dir(PyObject *self, PyObject *args)
{
	PyObject *arg = NULL;

	if (!PyArg_UnpackTuple(args, "dir", 0, 1, &arg))
		return NULL;
	return PyObject_Dir(arg);
}

PyDoc_STRVAR(dir_doc,
"dir([object]) -> list of strings\n"
"\n"
"If called without an argument, return the names in the current scope.\n"
"Else, return an alphabetized list of names comprising (some of) the attributes\n"
"of the given object, and of attributes reachable from it.\n"
"If the object supplies a method named __dir__, it will be used; otherwise\n"
"the default dir() logic is used and returns:\n"
"  for a module object: the module's attributes.\n"
"  for a class object:  its attributes, and recursively the attributes\n"
"    of its bases.\n"
"  for any other object: its attributes, its class's attributes, and\n"
"    recursively the attributes of its class's base classes.");

static PyObject *
builtin_divmod(PyObject *self, PyObject *args)
{
	PyObject *v, *w;

	if (!PyArg_UnpackTuple(args, "divmod", 2, 2, &v, &w))
		return NULL;
	return PyNumber_Divmod(v, w);
}

PyDoc_STRVAR(divmod_doc,
"divmod(x, y) -> (div, mod)\n\
\n\
Return the tuple ((x-x%y)/y, x%y).  Invariant: div*y + mod == x.");


static PyObject *
builtin_eval(PyObject *self, PyObject *args)
{
	PyObject *cmd, *result, *tmp = NULL;
	PyObject *globals = Py_None, *locals = Py_None;
	char *str;
	PyCompilerFlags cf;

	if (!PyArg_UnpackTuple(args, "eval", 1, 3, &cmd, &globals, &locals))
		return NULL;
	if (locals != Py_None && !PyMapping_Check(locals)) {
		PyErr_SetString(PyExc_TypeError, "locals must be a mapping");
		return NULL;
	}
	if (globals != Py_None && !PyDict_Check(globals)) {
		PyErr_SetString(PyExc_TypeError, PyMapping_Check(globals) ?
			"globals must be a real dict; try eval(expr, {}, mapping)"
			: "globals must be a dict");
		return NULL;
	}
	if (globals == Py_None) {
		globals = PyEval_GetGlobals();
		if (locals == Py_None)
			locals = PyEval_GetLocals();
	}
	else if (locals == Py_None)
		locals = globals;

	if (globals == NULL || locals == NULL) {
		PyErr_SetString(PyExc_TypeError, 
			"eval must be given globals and locals "
			"when called without a frame");
		return NULL;
	}

	if (PyDict_GetItemString(globals, "__builtins__") == NULL) {
		if (PyDict_SetItemString(globals, "__builtins__",
					 PyEval_GetBuiltins()) != 0)
			return NULL;
	}

	if (PyCode_Check(cmd)) {
		if (PyCode_GetNumFree((PyCodeObject *)cmd) > 0) {
			PyErr_SetString(PyExc_TypeError,
		"code object passed to eval() may not contain free variables");
			return NULL;
		}
		return PyEval_EvalCode((PyCodeObject *) cmd, globals, locals);
	}

	str = source_as_string(cmd, "eval", "string, bytes or code");
	if (str == NULL)
		return NULL;

	while (*str == ' ' || *str == '\t')
		str++;

	cf.cf_flags = PyCF_SOURCE_IS_UTF8;
	(void)PyEval_MergeCompilerFlags(&cf);
	result = PyRun_StringFlags(str, Py_eval_input, globals, locals, &cf);
	Py_XDECREF(tmp);
	return result;
}

PyDoc_STRVAR(eval_doc,
"eval(source[, globals[, locals]]) -> value\n\
\n\
Evaluate the source in the context of globals and locals.\n\
The source may be a string representing a Python expression\n\
or a code object as returned by compile().\n\
The globals must be a dictionary and locals can be any mapping,\n\
defaulting to the current globals and locals.\n\
If only globals is given, locals defaults to it.\n");

static PyObject *
builtin_exec(PyObject *self, PyObject *args)
{
	PyObject *v;
	PyObject *prog, *globals = Py_None, *locals = Py_None;
	int plain = 0;

	if (!PyArg_ParseTuple(args, "O|OO:exec", &prog, &globals, &locals))
		return NULL;
	
	if (globals == Py_None) {
		globals = PyEval_GetGlobals();
		if (locals == Py_None) {
			locals = PyEval_GetLocals();
			plain = 1;
		}
		if (!globals || !locals) {
			PyErr_SetString(PyExc_SystemError,
					"globals and locals cannot be NULL");
			return NULL;
		}
	}
	else if (locals == Py_None)
		locals = globals;

	if (!PyDict_Check(globals)) {
		PyErr_Format(PyExc_TypeError, "exec() arg 2 must be a dict, not %.100s",
			     globals->ob_type->tp_name);
		return NULL;
	}
	if (!PyMapping_Check(locals)) {
		PyErr_Format(PyExc_TypeError,
		    "arg 3 must be a mapping or None, not %.100s",
		    locals->ob_type->tp_name);
		return NULL;
	}
	if (PyDict_GetItemString(globals, "__builtins__") == NULL) {
		if (PyDict_SetItemString(globals, "__builtins__",
					 PyEval_GetBuiltins()) != 0)
			return NULL;
	}

	if (PyCode_Check(prog)) {
		if (PyCode_GetNumFree((PyCodeObject *)prog) > 0) {
			PyErr_SetString(PyExc_TypeError,
				"code object passed to exec() may not "
				"contain free variables");
			return NULL;
		}
		v = PyEval_EvalCode((PyCodeObject *) prog, globals, locals);
	}
	else {
		char *str = source_as_string(prog, "exec",
					     "string, bytes or code");
		PyCompilerFlags cf;
		if (str == NULL)
			return NULL;
		cf.cf_flags = PyCF_SOURCE_IS_UTF8;
		if (PyEval_MergeCompilerFlags(&cf))
			v = PyRun_StringFlags(str, Py_file_input, globals,
					      locals, &cf);
		else
			v = PyRun_String(str, Py_file_input, globals, locals);
	}
	if (v == NULL)
		return NULL;
	Py_DECREF(v);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(exec_doc,
"exec(object[, globals[, locals]])\n\
\n\
Read and execute code from a object, which can be a string, a code\n\
object or a file object.\n\
The globals and locals are dictionaries, defaulting to the current\n\
globals and locals.  If only globals is given, locals defaults to it.");


static PyObject *
builtin_getattr(PyObject *self, PyObject *args)
{
	PyObject *v, *result, *dflt = NULL;
	PyObject *name;

	if (!PyArg_UnpackTuple(args, "getattr", 2, 3, &v, &name, &dflt))
		return NULL;

	if (!PyUnicode_Check(name)) {
		PyErr_SetString(PyExc_TypeError,
				"getattr(): attribute name must be string");
		return NULL;
	}
	result = PyObject_GetAttr(v, name);
	if (result == NULL && dflt != NULL &&
	    PyErr_ExceptionMatches(PyExc_AttributeError))
	{
		PyErr_Clear();
		Py_INCREF(dflt);
		result = dflt;
	}
	return result;
}

PyDoc_STRVAR(getattr_doc,
"getattr(object, name[, default]) -> value\n\
\n\
Get a named attribute from an object; getattr(x, 'y') is equivalent to x.y.\n\
When a default argument is given, it is returned when the attribute doesn't\n\
exist; without it, an exception is raised in that case.");


static PyObject *
builtin_globals(PyObject *self)
{
	PyObject *d;

	d = PyEval_GetGlobals();
	Py_XINCREF(d);
	return d;
}

PyDoc_STRVAR(globals_doc,
"globals() -> dictionary\n\
\n\
Return the dictionary containing the current scope's global variables.");


static PyObject *
builtin_hasattr(PyObject *self, PyObject *args)
{
	PyObject *v;
	PyObject *name;

	if (!PyArg_UnpackTuple(args, "hasattr", 2, 2, &v, &name))
		return NULL;
	if (!PyUnicode_Check(name)) {
		PyErr_SetString(PyExc_TypeError,
				"hasattr(): attribute name must be string");
		return NULL;
	}
	v = PyObject_GetAttr(v, name);
	if (v == NULL) {
		if (!PyErr_ExceptionMatches(PyExc_Exception))
			return NULL;
		else {
			PyErr_Clear();
			Py_INCREF(Py_False);
			return Py_False;
		}
	}
	Py_DECREF(v);
	Py_INCREF(Py_True);
	return Py_True;
}

PyDoc_STRVAR(hasattr_doc,
"hasattr(object, name) -> bool\n\
\n\
Return whether the object has an attribute with the given name.\n\
(This is done by calling getattr(object, name) and catching exceptions.)");


static PyObject *
builtin_id(PyObject *self, PyObject *v)
{
	return PyLong_FromVoidPtr(v);
}

PyDoc_STRVAR(id_doc,
"id(object) -> integer\n\
\n\
Return the identity of an object.  This is guaranteed to be unique among\n\
simultaneously existing objects.  (Hint: it's the object's memory address.)");


/* map object ************************************************************/

typedef struct {
	PyObject_HEAD
	PyObject *iters;
	PyObject *func;
} mapobject;

PyTypeObject PyMap_Type;

static PyObject *
map_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *it, *iters, *func;
	mapobject *lz;
	Py_ssize_t numargs, i;

	if (type == &PyMap_Type && !_PyArg_NoKeywords("map()", kwds))
		return NULL;

	numargs = PyTuple_Size(args);
	if (numargs < 2) {
		PyErr_SetString(PyExc_TypeError,
		   "map() must have at least two arguments.");
		return NULL;
	}

	iters = PyTuple_New(numargs-1);
	if (iters == NULL)
		return NULL;

	for (i=1 ; i<numargs ; i++) {
		/* Get iterator. */
		it = PyObject_GetIter(PyTuple_GET_ITEM(args, i));
		if (it == NULL) {
			Py_DECREF(iters);
			return NULL;
		}
		PyTuple_SET_ITEM(iters, i-1, it);
	}

	/* create mapobject structure */
	lz = (mapobject *)type->tp_alloc(type, 0);
	if (lz == NULL) {
		Py_DECREF(iters);
		return NULL;
	}
	lz->iters = iters;
	func = PyTuple_GET_ITEM(args, 0);
	Py_INCREF(func);
	lz->func = func;

	return (PyObject *)lz;
}

static void
map_dealloc(mapobject *lz)
{
	PyObject_GC_UnTrack(lz);
	Py_XDECREF(lz->iters);
	Py_XDECREF(lz->func);
	Py_TYPE(lz)->tp_free(lz);
}

static int
map_traverse(mapobject *lz, visitproc visit, void *arg)
{
	Py_VISIT(lz->iters);
	Py_VISIT(lz->func);
	return 0;
}

static PyObject *
map_next(mapobject *lz)
{
	PyObject *val;
	PyObject *argtuple;
	PyObject *result;
	Py_ssize_t numargs, i;

	numargs = PyTuple_Size(lz->iters);
	argtuple = PyTuple_New(numargs);
	if (argtuple == NULL)
		return NULL;

	for (i=0 ; i<numargs ; i++) {
		val = PyIter_Next(PyTuple_GET_ITEM(lz->iters, i));
		if (val == NULL) {
			Py_DECREF(argtuple);
			return NULL;
		}
		PyTuple_SET_ITEM(argtuple, i, val);
	}
	result = PyObject_Call(lz->func, argtuple, NULL);
	Py_DECREF(argtuple);
	return result;
}

PyDoc_STRVAR(map_doc,
"map(func, *iterables) --> map object\n\
\n\
Make an iterator that computes the function using arguments from\n\
each of the iterables.	Stops when the shortest iterable is exhausted.");

PyTypeObject PyMap_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"map",				/* tp_name */
	sizeof(mapobject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)map_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	0,				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
		Py_TPFLAGS_BASETYPE,	/* tp_flags */
	map_doc,			/* tp_doc */
	(traverseproc)map_traverse,	/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc)map_next,	/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	PyType_GenericAlloc,		/* tp_alloc */
	map_new,			/* tp_new */
	PyObject_GC_Del,		/* tp_free */
};

static PyObject *
builtin_next(PyObject *self, PyObject *args)
{
	PyObject *it, *res;
	PyObject *def = NULL;

	if (!PyArg_UnpackTuple(args, "next", 1, 2, &it, &def))
		return NULL;
	if (!PyIter_Check(it)) {
		PyErr_Format(PyExc_TypeError,
			"%.200s object is not an iterator",
			it->ob_type->tp_name);
		return NULL;
	}
	
	res = (*it->ob_type->tp_iternext)(it);
	if (res != NULL) {
		return res;
	} else if (def != NULL) {
		if (PyErr_Occurred()) {
			if(!PyErr_ExceptionMatches(PyExc_StopIteration))
				return NULL;
			PyErr_Clear();
		}
		Py_INCREF(def);
		return def;
	} else if (PyErr_Occurred()) {
		return NULL;
	} else {
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
}

PyDoc_STRVAR(next_doc,
"next(iterator[, default])\n\
\n\
Return the next item from the iterator. If default is given and the iterator\n\
is exhausted, it is returned instead of raising StopIteration.");


static PyObject *
builtin_setattr(PyObject *self, PyObject *args)
{
	PyObject *v;
	PyObject *name;
	PyObject *value;

	if (!PyArg_UnpackTuple(args, "setattr", 3, 3, &v, &name, &value))
		return NULL;
	if (PyObject_SetAttr(v, name, value) != 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(setattr_doc,
"setattr(object, name, value)\n\
\n\
Set a named attribute on an object; setattr(x, 'y', v) is equivalent to\n\
``x.y = v''.");


static PyObject *
builtin_delattr(PyObject *self, PyObject *args)
{
	PyObject *v;
	PyObject *name;

	if (!PyArg_UnpackTuple(args, "delattr", 2, 2, &v, &name))
		return NULL;
	if (PyObject_SetAttr(v, name, (PyObject *)NULL) != 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(delattr_doc,
"delattr(object, name)\n\
\n\
Delete a named attribute on an object; delattr(x, 'y') is equivalent to\n\
``del x.y''.");


static PyObject *
builtin_hash(PyObject *self, PyObject *v)
{
	long x;

	x = PyObject_Hash(v);
	if (x == -1)
		return NULL;
	return PyLong_FromLong(x);
}

PyDoc_STRVAR(hash_doc,
"hash(object) -> integer\n\
\n\
Return a hash value for the object.  Two objects with the same value have\n\
the same hash value.  The reverse is not necessarily true, but likely.");


static PyObject *
builtin_hex(PyObject *self, PyObject *v)
{
	return PyNumber_ToBase(v, 16);
}

PyDoc_STRVAR(hex_doc,
"hex(number) -> string\n\
\n\
Return the hexadecimal representation of an integer or long integer.");


static PyObject *
builtin_iter(PyObject *self, PyObject *args)
{
	PyObject *v, *w = NULL;

	if (!PyArg_UnpackTuple(args, "iter", 1, 2, &v, &w))
		return NULL;
	if (w == NULL)
		return PyObject_GetIter(v);
	if (!PyCallable_Check(v)) {
		PyErr_SetString(PyExc_TypeError,
				"iter(v, w): v must be callable");
		return NULL;
	}
	return PyCallIter_New(v, w);
}

PyDoc_STRVAR(iter_doc,
"iter(iterable) -> iterator\n\
iter(callable, sentinel) -> iterator\n\
\n\
Get an iterator from an object.  In the first form, the argument must\n\
supply its own iterator, or be a sequence.\n\
In the second form, the callable is called until it returns the sentinel.");


static PyObject *
builtin_len(PyObject *self, PyObject *v)
{
	Py_ssize_t res;

	res = PyObject_Size(v);
	if (res < 0 && PyErr_Occurred())
		return NULL;
	return PyLong_FromSsize_t(res);
}

PyDoc_STRVAR(len_doc,
"len(object) -> integer\n\
\n\
Return the number of items of a sequence or mapping.");


static PyObject *
builtin_locals(PyObject *self)
{
	PyObject *d;

	d = PyEval_GetLocals();
	Py_XINCREF(d);
	return d;
}

PyDoc_STRVAR(locals_doc,
"locals() -> dictionary\n\
\n\
Update and return a dictionary containing the current scope's local variables.");


static PyObject *
min_max(PyObject *args, PyObject *kwds, int op)
{
	PyObject *v, *it, *item, *val, *maxitem, *maxval, *keyfunc=NULL;
	const char *name = op == Py_LT ? "min" : "max";

	if (PyTuple_Size(args) > 1)
		v = args;
	else if (!PyArg_UnpackTuple(args, (char *)name, 1, 1, &v))
		return NULL;

	if (kwds != NULL && PyDict_Check(kwds) && PyDict_Size(kwds)) {
		keyfunc = PyDict_GetItemString(kwds, "key");
		if (PyDict_Size(kwds)!=1  ||  keyfunc == NULL) {
			PyErr_Format(PyExc_TypeError,
				"%s() got an unexpected keyword argument", name);
			return NULL;
		}
		Py_INCREF(keyfunc);
	}

	it = PyObject_GetIter(v);
	if (it == NULL) {
		Py_XDECREF(keyfunc);
		return NULL;
	}

	maxitem = NULL; /* the result */
	maxval = NULL;  /* the value associated with the result */
	while (( item = PyIter_Next(it) )) {
		/* get the value from the key function */
		if (keyfunc != NULL) {
			val = PyObject_CallFunctionObjArgs(keyfunc, item, NULL);
			if (val == NULL)
				goto Fail_it_item;
		}
		/* no key function; the value is the item */
		else {
			val = item;
			Py_INCREF(val);
		}

		/* maximum value and item are unset; set them */
		if (maxval == NULL) {
			maxitem = item;
			maxval = val;
		}
		/* maximum value and item are set; update them as necessary */
		else {
			int cmp = PyObject_RichCompareBool(val, maxval, op);
			if (cmp < 0)
				goto Fail_it_item_and_val;
			else if (cmp > 0) {
				Py_DECREF(maxval);
				Py_DECREF(maxitem);
				maxval = val;
				maxitem = item;
			}
			else {
				Py_DECREF(item);
				Py_DECREF(val);
			}
		}
	}
	if (PyErr_Occurred())
		goto Fail_it;
	if (maxval == NULL) {
		PyErr_Format(PyExc_ValueError,
			     "%s() arg is an empty sequence", name);
		assert(maxitem == NULL);
	}
	else
		Py_DECREF(maxval);
	Py_DECREF(it);
	Py_XDECREF(keyfunc);
	return maxitem;

Fail_it_item_and_val:
	Py_DECREF(val);
Fail_it_item:
	Py_DECREF(item);
Fail_it:
	Py_XDECREF(maxval);
	Py_XDECREF(maxitem);
	Py_DECREF(it);
	Py_XDECREF(keyfunc);
	return NULL;
}

static PyObject *
builtin_min(PyObject *self, PyObject *args, PyObject *kwds)
{
	return min_max(args, kwds, Py_LT);
}

PyDoc_STRVAR(min_doc,
"min(iterable[, key=func]) -> value\n\
min(a, b, c, ...[, key=func]) -> value\n\
\n\
With a single iterable argument, return its smallest item.\n\
With two or more arguments, return the smallest argument.");


static PyObject *
builtin_max(PyObject *self, PyObject *args, PyObject *kwds)
{
	return min_max(args, kwds, Py_GT);
}

PyDoc_STRVAR(max_doc,
"max(iterable[, key=func]) -> value\n\
max(a, b, c, ...[, key=func]) -> value\n\
\n\
With a single iterable argument, return its largest item.\n\
With two or more arguments, return the largest argument.");


static PyObject *
builtin_oct(PyObject *self, PyObject *v)
{
	return PyNumber_ToBase(v, 8);
}

PyDoc_STRVAR(oct_doc,
"oct(number) -> string\n\
\n\
Return the octal representation of an integer or long integer.");


static PyObject *
builtin_ord(PyObject *self, PyObject* obj)
{
	long ord;
	Py_ssize_t size;

	if (PyBytes_Check(obj)) {
		size = PyBytes_GET_SIZE(obj);
		if (size == 1) {
			ord = (long)((unsigned char)*PyBytes_AS_STRING(obj));
			return PyLong_FromLong(ord);
		}
	}
	else if (PyUnicode_Check(obj)) {
		size = PyUnicode_GET_SIZE(obj);
		if (size == 1) {
			ord = (long)*PyUnicode_AS_UNICODE(obj);
			return PyLong_FromLong(ord);
		}
#ifndef Py_UNICODE_WIDE
		if (size == 2) {
			/* Decode a valid surrogate pair */
			int c0 = PyUnicode_AS_UNICODE(obj)[0];
			int c1 = PyUnicode_AS_UNICODE(obj)[1];
			if (0xD800 <= c0 && c0 <= 0xDBFF &&
			    0xDC00 <= c1 && c1 <= 0xDFFF) {
				ord = ((((c0 & 0x03FF) << 10) | (c1 & 0x03FF)) +
				       0x00010000);
				return PyLong_FromLong(ord);
			}
		}
#endif
	}
	else if (PyByteArray_Check(obj)) {
		/* XXX Hopefully this is temporary */
		size = PyByteArray_GET_SIZE(obj);
		if (size == 1) {
			ord = (long)((unsigned char)*PyByteArray_AS_STRING(obj));
			return PyLong_FromLong(ord);
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
			     "ord() expected string of length 1, but " \
			     "%.200s found", obj->ob_type->tp_name);
		return NULL;
	}

	PyErr_Format(PyExc_TypeError,
		     "ord() expected a character, "
		     "but string of length %zd found",
		     size);
	return NULL;
}

PyDoc_VAR(ord_doc) = PyDoc_STR(
"ord(c) -> integer\n\
\n\
Return the integer ordinal of a one-character string."
)
#ifndef Py_UNICODE_WIDE
PyDoc_STR(
"\nA valid surrogate pair is also accepted."
)
#endif
;


static PyObject *
builtin_pow(PyObject *self, PyObject *args)
{
	PyObject *v, *w, *z = Py_None;

	if (!PyArg_UnpackTuple(args, "pow", 2, 3, &v, &w, &z))
		return NULL;
	return PyNumber_Power(v, w, z);
}

PyDoc_STRVAR(pow_doc,
"pow(x, y[, z]) -> number\n\
\n\
With two arguments, equivalent to x**y.  With three arguments,\n\
equivalent to (x**y) % z, but may be more efficient (e.g. for longs).");



static PyObject *
builtin_print(PyObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"sep", "end", "file", 0};
	static PyObject *dummy_args;
	PyObject *sep = NULL, *end = NULL, *file = NULL;
	int i, err;

	if (dummy_args == NULL) {
		if (!(dummy_args = PyTuple_New(0)))
			return NULL;
	}
	if (!PyArg_ParseTupleAndKeywords(dummy_args, kwds, "|OOO:print",
					 kwlist, &sep, &end, &file))
		return NULL;
	if (file == NULL || file == Py_None) {
		file = PySys_GetObject("stdout");
		/* sys.stdout may be None when FILE* stdout isn't connected */
		if (file == Py_None)
			Py_RETURN_NONE;
	}

	if (sep && sep != Py_None && !PyUnicode_Check(sep)) {
		PyErr_Format(PyExc_TypeError,
			     "sep must be None or a string, not %.200s",
			     sep->ob_type->tp_name);
		return NULL;
	}
	if (end && end != Py_None && !PyUnicode_Check(end)) {
		PyErr_Format(PyExc_TypeError,
			     "end must be None or a string, not %.200s",
			     end->ob_type->tp_name);
		return NULL;
	}

	for (i = 0; i < PyTuple_Size(args); i++) {
		if (i > 0) {
			if (sep == NULL || sep == Py_None)
				err = PyFile_WriteString(" ", file);
			else
				err = PyFile_WriteObject(sep, file,
							 Py_PRINT_RAW);
			if (err)
				return NULL;
		}
		err = PyFile_WriteObject(PyTuple_GetItem(args, i), file,
					 Py_PRINT_RAW);
		if (err)
			return NULL;
	}

	if (end == NULL || end == Py_None)
		err = PyFile_WriteString("\n", file);
	else
		err = PyFile_WriteObject(end, file, Py_PRINT_RAW);
	if (err)
		return NULL;

	Py_RETURN_NONE;
}

PyDoc_STRVAR(print_doc,
"print(value, ..., sep=' ', end='\\n', file=sys.stdout)\n\
\n\
Prints the values to a stream, or to sys.stdout by default.\n\
Optional keyword arguments:\n\
file: a file-like object (stream); defaults to the current sys.stdout.\n\
sep:  string inserted between values, default a space.\n\
end:  string appended after the last value, default a newline.");


static PyObject *
builtin_input(PyObject *self, PyObject *args)
{
	PyObject *promptarg = NULL;
	PyObject *fin = PySys_GetObject("stdin");
	PyObject *fout = PySys_GetObject("stdout");
	PyObject *ferr = PySys_GetObject("stderr");
	PyObject *tmp;
	long fd;
	int tty;

	/* Parse arguments */
	if (!PyArg_UnpackTuple(args, "input", 0, 1, &promptarg))
		return NULL;

	/* Check that stdin/out/err are intact */
	if (fin == NULL || fin == Py_None) {
		PyErr_SetString(PyExc_RuntimeError,
				"input(): lost sys.stdin");
		return NULL;
	}
	if (fout == NULL || fout == Py_None) {
		PyErr_SetString(PyExc_RuntimeError,
				"input(): lost sys.stdout");
		return NULL;
	}
	if (ferr == NULL || ferr == Py_None) {
		PyErr_SetString(PyExc_RuntimeError,
				"input(): lost sys.stderr");
		return NULL;
	}

	/* First of all, flush stderr */
	tmp = PyObject_CallMethod(ferr, "flush", "");
	if (tmp == NULL)
		PyErr_Clear();
	else
		Py_DECREF(tmp);

	/* We should only use (GNU) readline if Python's sys.stdin and
	   sys.stdout are the same as C's stdin and stdout, because we
	   need to pass it those. */
	tmp = PyObject_CallMethod(fin, "fileno", "");
	if (tmp == NULL) {
		PyErr_Clear();
		tty = 0;
	}
	else {
		fd = PyLong_AsLong(tmp);
		Py_DECREF(tmp);
		if (fd < 0 && PyErr_Occurred())
			return NULL;
		tty = fd == fileno(stdin) && isatty(fd);
	}
	if (tty) {
		tmp = PyObject_CallMethod(fout, "fileno", "");
		if (tmp == NULL)
			PyErr_Clear();
		else {
			fd = PyLong_AsLong(tmp);
			Py_DECREF(tmp);
			if (fd < 0 && PyErr_Occurred())
				return NULL;
			tty = fd == fileno(stdout) && isatty(fd);
		}
	}

	/* If we're interactive, use (GNU) readline */
	if (tty) {
		PyObject *po;
		char *prompt;
		char *s;
		PyObject *stdin_encoding;
		PyObject *result;

		stdin_encoding = PyObject_GetAttrString(fin, "encoding");
		if (!stdin_encoding)
			/* stdin is a text stream, so it must have an
			   encoding. */
			return NULL;
		tmp = PyObject_CallMethod(fout, "flush", "");
		if (tmp == NULL)
			PyErr_Clear();
		else
			Py_DECREF(tmp);
		if (promptarg != NULL) {
			PyObject *stringpo;
			PyObject *stdout_encoding;
			stdout_encoding = PyObject_GetAttrString(fout,
								 "encoding");
			if (stdout_encoding == NULL) {
				Py_DECREF(stdin_encoding);
				return NULL;
			}
			stringpo = PyObject_Str(promptarg);
			if (stringpo == NULL) {
				Py_DECREF(stdin_encoding);
				Py_DECREF(stdout_encoding);
				return NULL;
			}
			po = PyUnicode_AsEncodedString(stringpo,
				_PyUnicode_AsString(stdout_encoding), NULL);
			Py_DECREF(stdout_encoding);
			Py_DECREF(stringpo);
			if (po == NULL) {
				Py_DECREF(stdin_encoding);
				return NULL;
			}
			prompt = PyBytes_AsString(po);
			if (prompt == NULL) {
				Py_DECREF(stdin_encoding);
				Py_DECREF(po);
				return NULL;
			}
		}
		else {
			po = NULL;
			prompt = "";
		}
		s = PyOS_Readline(stdin, stdout, prompt);
		Py_XDECREF(po);
		if (s == NULL) {
			if (!PyErr_Occurred())
				PyErr_SetNone(PyExc_KeyboardInterrupt);
			Py_DECREF(stdin_encoding);
			return NULL;
		}
		if (*s == '\0') {
			PyErr_SetNone(PyExc_EOFError);
			result = NULL;
		}
		else { /* strip trailing '\n' */
			size_t len = strlen(s);
			if (len > PY_SSIZE_T_MAX) {
				PyErr_SetString(PyExc_OverflowError,
						"input: input too long");
				result = NULL;
			}
			else {
				result = PyUnicode_Decode
					(s, len-1,
					 _PyUnicode_AsString(stdin_encoding),
					 NULL);
			}
		}
		Py_DECREF(stdin_encoding);
		PyMem_FREE(s);
		return result;
	}

	/* Fallback if we're not interactive */
	if (promptarg != NULL) {
		if (PyFile_WriteObject(promptarg, fout, Py_PRINT_RAW) != 0)
			return NULL;
	}
	tmp = PyObject_CallMethod(fout, "flush", "");
	if (tmp == NULL)
		PyErr_Clear();
	else
		Py_DECREF(tmp);
	return PyFile_GetLine(fin, -1);
}

PyDoc_STRVAR(input_doc,
"input([prompt]) -> string\n\
\n\
Read a string from standard input.  The trailing newline is stripped.\n\
If the user hits EOF (Unix: Ctl-D, Windows: Ctl-Z+Return), raise EOFError.\n\
On Unix, GNU readline is used if enabled.  The prompt string, if given,\n\
is printed without a trailing newline before reading.");


static PyObject *
builtin_repr(PyObject *self, PyObject *v)
{
	return PyObject_Repr(v);
}

PyDoc_STRVAR(repr_doc,
"repr(object) -> string\n\
\n\
Return the canonical string representation of the object.\n\
For most object types, eval(repr(object)) == object.");


static PyObject *
builtin_round(PyObject *self, PyObject *args, PyObject *kwds)
{
#define UNDEF_NDIGITS (-0x7fffffff) /* Unlikely ndigits value */
	static PyObject *round_str = NULL;
	int ndigits = UNDEF_NDIGITS;
	static char *kwlist[] = {"number", "ndigits", 0};
	PyObject *number, *round;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|i:round",
                kwlist, &number, &ndigits))
                return NULL;

	if (Py_TYPE(number)->tp_dict == NULL) {
		if (PyType_Ready(Py_TYPE(number)) < 0)
			return NULL;
	}

	if (round_str == NULL) {
		round_str = PyUnicode_InternFromString("__round__");
		if (round_str == NULL)
			return NULL;
	}

	round = _PyType_Lookup(Py_TYPE(number), round_str);
	if (round == NULL) {
		PyErr_Format(PyExc_TypeError,
			     "type %.100s doesn't define __round__ method",
			     Py_TYPE(number)->tp_name);
		return NULL;
	}

	if (ndigits == UNDEF_NDIGITS)
                return PyObject_CallFunction(round, "O", number);
	else
                return PyObject_CallFunction(round, "Oi", number, ndigits);
#undef UNDEF_NDIGITS
}

PyDoc_STRVAR(round_doc,
"round(number[, ndigits]) -> floating point number\n\
\n\
Round a number to a given precision in decimal digits (default 0 digits).\n\
This returns an int when called with one argument, otherwise the\n\
same type as the number. ndigits may be negative.");


static PyObject *
builtin_sorted(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *newlist, *v, *seq, *keyfunc=NULL, *newargs;
	PyObject *callable;
	static char *kwlist[] = {"iterable", "key", "reverse", 0};
	int reverse;

	/* args 1-3 should match listsort in Objects/listobject.c */
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|Oi:sorted",
		kwlist, &seq, &keyfunc, &reverse))
		return NULL;

	newlist = PySequence_List(seq);
	if (newlist == NULL)
		return NULL;

	callable = PyObject_GetAttrString(newlist, "sort");
	if (callable == NULL) {
		Py_DECREF(newlist);
		return NULL;
	}

	newargs = PyTuple_GetSlice(args, 1, 4);
	if (newargs == NULL) {
		Py_DECREF(newlist);
		Py_DECREF(callable);
		return NULL;
	}

	v = PyObject_Call(callable, newargs, kwds);
	Py_DECREF(newargs);
	Py_DECREF(callable);
	if (v == NULL) {
		Py_DECREF(newlist);
		return NULL;
	}
	Py_DECREF(v);
	return newlist;
}

PyDoc_STRVAR(sorted_doc,
"sorted(iterable, key=None, reverse=False) --> new sorted list");

static PyObject *
builtin_vars(PyObject *self, PyObject *args)
{
	PyObject *v = NULL;
	PyObject *d;

	if (!PyArg_UnpackTuple(args, "vars", 0, 1, &v))
		return NULL;
	if (v == NULL) {
		d = PyEval_GetLocals();
		if (d == NULL) {
			if (!PyErr_Occurred())
				PyErr_SetString(PyExc_SystemError,
						"vars(): no locals!?");
		}
		else
			Py_INCREF(d);
	}
	else {
		d = PyObject_GetAttrString(v, "__dict__");
		if (d == NULL) {
			PyErr_SetString(PyExc_TypeError,
			    "vars() argument must have __dict__ attribute");
			return NULL;
		}
	}
	return d;
}

PyDoc_STRVAR(vars_doc,
"vars([object]) -> dictionary\n\
\n\
Without arguments, equivalent to locals().\n\
With an argument, equivalent to object.__dict__.");

static PyObject*
builtin_sum(PyObject *self, PyObject *args)
{
	PyObject *seq;
	PyObject *result = NULL;
	PyObject *temp, *item, *iter;

	if (!PyArg_UnpackTuple(args, "sum", 1, 2, &seq, &result))
		return NULL;

	iter = PyObject_GetIter(seq);
	if (iter == NULL)
		return NULL;

	if (result == NULL) {
		result = PyLong_FromLong(0);
		if (result == NULL) {
			Py_DECREF(iter);
			return NULL;
		}
	} else {
		/* reject string values for 'start' parameter */
		if (PyUnicode_Check(result)) {
			PyErr_SetString(PyExc_TypeError,
				"sum() can't sum strings [use ''.join(seq) instead]");
			Py_DECREF(iter);
			return NULL;
		}
		if (PyByteArray_Check(result)) {
			PyErr_SetString(PyExc_TypeError,
				"sum() can't sum bytes [use b''.join(seq) instead]");
			Py_DECREF(iter);
			return NULL;
		}

		Py_INCREF(result);
	}

#ifndef SLOW_SUM
	/* Fast addition by keeping temporary sums in C instead of new Python objects.
           Assumes all inputs are the same type.  If the assumption fails, default
           to the more general routine.
	*/
	if (PyLong_CheckExact(result)) {
		int overflow;
		long i_result = PyLong_AsLongAndOverflow(result, &overflow);
		/* If this already overflowed, don't even enter the loop. */
		if (overflow == 0) {
			Py_DECREF(result);
			result = NULL;
		}
		while(result == NULL) {
			item = PyIter_Next(iter);
			if (item == NULL) {
				Py_DECREF(iter);
				if (PyErr_Occurred())
					return NULL;
    				return PyLong_FromLong(i_result);
			}
        		if (PyLong_CheckExact(item)) {
            			long b = PyLong_AsLongAndOverflow(item, &overflow);
				long x = i_result + b;
				if (overflow == 0 && ((x^i_result) >= 0 || (x^b) >= 0)) {
					i_result = x;
					Py_DECREF(item);
					continue;
				}
			}
			/* Either overflowed or is not an int. Restore real objects and process normally */
			result = PyLong_FromLong(i_result);
			temp = PyNumber_Add(result, item);
			Py_DECREF(result);
			Py_DECREF(item);
			result = temp;
			if (result == NULL) {
				Py_DECREF(iter);
				return NULL;
			}
		}
	}

	if (PyFloat_CheckExact(result)) {
		double f_result = PyFloat_AS_DOUBLE(result);
		Py_DECREF(result);
		result = NULL;
		while(result == NULL) {
			item = PyIter_Next(iter);
			if (item == NULL) {
				Py_DECREF(iter);
				if (PyErr_Occurred())
					return NULL;
    				return PyFloat_FromDouble(f_result);
			}
        		if (PyFloat_CheckExact(item)) {
				PyFPE_START_PROTECT("add", Py_DECREF(item); Py_DECREF(iter); return 0)
				f_result += PyFloat_AS_DOUBLE(item);
				PyFPE_END_PROTECT(f_result)
				Py_DECREF(item);
				continue;
			}
        		if (PyLong_CheckExact(item)) {
				long value;
				int overflow;
				value = PyLong_AsLongAndOverflow(item, &overflow);
				if (!overflow) {
					PyFPE_START_PROTECT("add", Py_DECREF(item); Py_DECREF(iter); return 0)
					f_result += (double)value;
					PyFPE_END_PROTECT(f_result)
					Py_DECREF(item);
					continue;
				}
			}
			result = PyFloat_FromDouble(f_result);
			temp = PyNumber_Add(result, item);
			Py_DECREF(result);
			Py_DECREF(item);
			result = temp;
			if (result == NULL) {
				Py_DECREF(iter);
				return NULL;
			}
		}
	}
#endif

	for(;;) {
		item = PyIter_Next(iter);
		if (item == NULL) {
			/* error, or end-of-sequence */
			if (PyErr_Occurred()) {
				Py_DECREF(result);
				result = NULL;
			}
			break;
		}
		temp = PyNumber_Add(result, item);
		Py_DECREF(result);
		Py_DECREF(item);
		result = temp;
		if (result == NULL)
			break;
	}
	Py_DECREF(iter);
	return result;
}

PyDoc_STRVAR(sum_doc,
"sum(iterable[, start]) -> value\n\
\n\
Returns the sum of an iterable of numbers (NOT strings) plus the value\n\
of parameter 'start' (which defaults to 0).  When the iterable is\n\
empty, returns start.");


static PyObject *
builtin_isinstance(PyObject *self, PyObject *args)
{
	PyObject *inst;
	PyObject *cls;
	int retval;

	if (!PyArg_UnpackTuple(args, "isinstance", 2, 2, &inst, &cls))
		return NULL;

	retval = PyObject_IsInstance(inst, cls);
	if (retval < 0)
		return NULL;
	return PyBool_FromLong(retval);
}

PyDoc_STRVAR(isinstance_doc,
"isinstance(object, class-or-type-or-tuple) -> bool\n\
\n\
Return whether an object is an instance of a class or of a subclass thereof.\n\
With a type as second argument, return whether that is the object's type.\n\
The form using a tuple, isinstance(x, (A, B, ...)), is a shortcut for\n\
isinstance(x, A) or isinstance(x, B) or ... (etc.).");


static PyObject *
builtin_issubclass(PyObject *self, PyObject *args)
{
	PyObject *derived;
	PyObject *cls;
	int retval;

	if (!PyArg_UnpackTuple(args, "issubclass", 2, 2, &derived, &cls))
		return NULL;

	retval = PyObject_IsSubclass(derived, cls);
	if (retval < 0)
		return NULL;
	return PyBool_FromLong(retval);
}

PyDoc_STRVAR(issubclass_doc,
"issubclass(C, B) -> bool\n\
\n\
Return whether class C is a subclass (i.e., a derived class) of class B.\n\
When using a tuple as the second argument issubclass(X, (A, B, ...)),\n\
is a shortcut for issubclass(X, A) or issubclass(X, B) or ... (etc.).");


typedef struct {
	PyObject_HEAD
	Py_ssize_t	tuplesize;
	PyObject *ittuple;		/* tuple of iterators */
	PyObject *result;
} zipobject;

PyTypeObject PyZip_Type;

static PyObject *
zip_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	zipobject *lz;
	Py_ssize_t i;
	PyObject *ittuple;  /* tuple of iterators */
	PyObject *result;
	Py_ssize_t tuplesize = PySequence_Length(args);

	if (type == &PyZip_Type && !_PyArg_NoKeywords("zip()", kwds))
		return NULL;

	/* args must be a tuple */
	assert(PyTuple_Check(args));

	/* obtain iterators */
	ittuple = PyTuple_New(tuplesize);
	if (ittuple == NULL)
		return NULL;
	for (i=0; i < tuplesize; ++i) {
		PyObject *item = PyTuple_GET_ITEM(args, i);
		PyObject *it = PyObject_GetIter(item);
		if (it == NULL) {
			if (PyErr_ExceptionMatches(PyExc_TypeError))
				PyErr_Format(PyExc_TypeError,
				    "zip argument #%zd must support iteration",
				    i+1);
			Py_DECREF(ittuple);
			return NULL;
		}
		PyTuple_SET_ITEM(ittuple, i, it);
	}

	/* create a result holder */
	result = PyTuple_New(tuplesize);
	if (result == NULL) {
		Py_DECREF(ittuple);
		return NULL;
	}
	for (i=0 ; i < tuplesize ; i++) {
		Py_INCREF(Py_None);
		PyTuple_SET_ITEM(result, i, Py_None);
	}

	/* create zipobject structure */
	lz = (zipobject *)type->tp_alloc(type, 0);
	if (lz == NULL) {
		Py_DECREF(ittuple);
		Py_DECREF(result);
		return NULL;
	}
	lz->ittuple = ittuple;
	lz->tuplesize = tuplesize;
	lz->result = result;

	return (PyObject *)lz;
}

static void
zip_dealloc(zipobject *lz)
{
	PyObject_GC_UnTrack(lz);
	Py_XDECREF(lz->ittuple);
	Py_XDECREF(lz->result);
	Py_TYPE(lz)->tp_free(lz);
}

static int
zip_traverse(zipobject *lz, visitproc visit, void *arg)
{
	Py_VISIT(lz->ittuple);
	Py_VISIT(lz->result);
	return 0;
}

static PyObject *
zip_next(zipobject *lz)
{
	Py_ssize_t i;
	Py_ssize_t tuplesize = lz->tuplesize;
	PyObject *result = lz->result;
	PyObject *it;
	PyObject *item;
	PyObject *olditem;

	if (tuplesize == 0)
		return NULL;
	if (Py_REFCNT(result) == 1) {
		Py_INCREF(result);
		for (i=0 ; i < tuplesize ; i++) {
			it = PyTuple_GET_ITEM(lz->ittuple, i);
			assert(PyIter_Check(it));
			item = (*Py_TYPE(it)->tp_iternext)(it);
			if (item == NULL) {
				Py_DECREF(result);
				return NULL;
			}
			olditem = PyTuple_GET_ITEM(result, i);
			PyTuple_SET_ITEM(result, i, item);
			Py_DECREF(olditem);
		}
	} else {
		result = PyTuple_New(tuplesize);
		if (result == NULL)
			return NULL;
		for (i=0 ; i < tuplesize ; i++) {
			it = PyTuple_GET_ITEM(lz->ittuple, i);
			assert(PyIter_Check(it));
			item = (*Py_TYPE(it)->tp_iternext)(it);
			if (item == NULL) {
				Py_DECREF(result);
				return NULL;
			}
			PyTuple_SET_ITEM(result, i, item);
		}
	}
	return result;
}

PyDoc_STRVAR(zip_doc,
"zip(iter1 [,iter2 [...]]) --> zip object\n\
\n\
Return a zip object whose .__next__() method returns a tuple where\n\
the i-th element comes from the i-th iterable argument.  The .__next__()\n\
method continues until the shortest iterable in the argument sequence\n\
is exhausted and then it raises StopIteration.  Works like the zip()\n\
function but consumes less memory by returning an iterator instead of\n\
a list.");

PyTypeObject PyZip_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"zip",				/* tp_name */
	sizeof(zipobject),		/* tp_basicsize */
	0,				/* tp_itemsize */
	/* methods */
	(destructor)zip_dealloc,	/* tp_dealloc */
	0,				/* tp_print */
	0,				/* tp_getattr */
	0,				/* tp_setattr */
	0,				/* tp_compare */
	0,				/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	0,				/* tp_hash */
	0,				/* tp_call */
	0,				/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	0,				/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
		Py_TPFLAGS_BASETYPE,	/* tp_flags */
	zip_doc,			/* tp_doc */
	(traverseproc)zip_traverse,    /* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	PyObject_SelfIter,		/* tp_iter */
	(iternextfunc)zip_next,	/* tp_iternext */
	0,				/* tp_methods */
	0,				/* tp_members */
	0,				/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	0,				/* tp_init */
	PyType_GenericAlloc,		/* tp_alloc */
	zip_new,			/* tp_new */
	PyObject_GC_Del,		/* tp_free */
};


static PyMethodDef builtin_methods[] = {
 	{"__build_class__", (PyCFunction)builtin___build_class__,
         METH_VARARGS | METH_KEYWORDS, build_class_doc},
 	{"__import__",	(PyCFunction)builtin___import__, METH_VARARGS | METH_KEYWORDS, import_doc},
 	{"abs",		builtin_abs,        METH_O, abs_doc},
 	{"all",		builtin_all,        METH_O, all_doc},
 	{"any",		builtin_any,        METH_O, any_doc},
 	{"ascii",	builtin_ascii,      METH_O, ascii_doc},
	{"bin",		builtin_bin,	    METH_O, bin_doc},
 	{"chr",		builtin_chr,        METH_VARARGS, chr_doc},
 	{"cmp",		builtin_cmp,        METH_VARARGS, cmp_doc},
 	{"compile",	(PyCFunction)builtin_compile,    METH_VARARGS | METH_KEYWORDS, compile_doc},
 	{"delattr",	builtin_delattr,    METH_VARARGS, delattr_doc},
 	{"dir",		builtin_dir,        METH_VARARGS, dir_doc},
 	{"divmod",	builtin_divmod,     METH_VARARGS, divmod_doc},
 	{"eval",	builtin_eval,       METH_VARARGS, eval_doc},
	{"exec",        builtin_exec,       METH_VARARGS, exec_doc},
 	{"format",	builtin_format,     METH_VARARGS, format_doc},
 	{"getattr",	builtin_getattr,    METH_VARARGS, getattr_doc},
 	{"globals",	(PyCFunction)builtin_globals,    METH_NOARGS, globals_doc},
 	{"hasattr",	builtin_hasattr,    METH_VARARGS, hasattr_doc},
 	{"hash",	builtin_hash,       METH_O, hash_doc},
 	{"hex",		builtin_hex,        METH_O, hex_doc},
 	{"id",		builtin_id,         METH_O, id_doc},
 	{"input",	builtin_input,      METH_VARARGS, input_doc},
 	{"isinstance",  builtin_isinstance, METH_VARARGS, isinstance_doc},
 	{"issubclass",  builtin_issubclass, METH_VARARGS, issubclass_doc},
 	{"iter",	builtin_iter,       METH_VARARGS, iter_doc},
 	{"len",		builtin_len,        METH_O, len_doc},
 	{"locals",	(PyCFunction)builtin_locals,     METH_NOARGS, locals_doc},
 	{"max",		(PyCFunction)builtin_max,        METH_VARARGS | METH_KEYWORDS, max_doc},
 	{"min",		(PyCFunction)builtin_min,        METH_VARARGS | METH_KEYWORDS, min_doc},
	{"next",	(PyCFunction)builtin_next,       METH_VARARGS, next_doc},
 	{"oct",		builtin_oct,        METH_O, oct_doc},
 	{"ord",		builtin_ord,        METH_O, ord_doc},
 	{"pow",		builtin_pow,        METH_VARARGS, pow_doc},
 	{"print",	(PyCFunction)builtin_print,      METH_VARARGS | METH_KEYWORDS, print_doc},
 	{"repr",	builtin_repr,       METH_O, repr_doc},
 	{"round",	(PyCFunction)builtin_round,      METH_VARARGS | METH_KEYWORDS, round_doc},
 	{"setattr",	builtin_setattr,    METH_VARARGS, setattr_doc},
 	{"sorted",	(PyCFunction)builtin_sorted,     METH_VARARGS | METH_KEYWORDS, sorted_doc},
 	{"sum",		builtin_sum,        METH_VARARGS, sum_doc},
 	{"vars",	builtin_vars,       METH_VARARGS, vars_doc},
	{NULL,		NULL},
};

PyDoc_STRVAR(builtin_doc,
"Built-in functions, exceptions, and other objects.\n\
\n\
Noteworthy: None is the `nil' object; Ellipsis represents `...' in slices.");

static struct PyModuleDef builtinsmodule = {
	PyModuleDef_HEAD_INIT,
	"builtins",
	builtin_doc,
	-1, /* multiple "initialization" just copies the module dict. */
	builtin_methods,
	NULL,
	NULL,
	NULL,
	NULL
};


PyObject *
_PyBuiltin_Init(void)
{
	PyObject *mod, *dict, *debug;
	mod = PyModule_Create(&builtinsmodule);
	if (mod == NULL)
		return NULL;
	dict = PyModule_GetDict(mod);

#ifdef Py_TRACE_REFS
	/* "builtins" exposes a number of statically allocated objects
	 * that, before this code was added in 2.3, never showed up in
	 * the list of "all objects" maintained by Py_TRACE_REFS.  As a
	 * result, programs leaking references to None and False (etc)
	 * couldn't be diagnosed by examining sys.getobjects(0).
	 */
#define ADD_TO_ALL(OBJECT) _Py_AddToAllObjects((PyObject *)(OBJECT), 0)
#else
#define ADD_TO_ALL(OBJECT) (void)0
#endif

#define SETBUILTIN(NAME, OBJECT) \
	if (PyDict_SetItemString(dict, NAME, (PyObject *)OBJECT) < 0)	\
		return NULL;						\
	ADD_TO_ALL(OBJECT)

	SETBUILTIN("None",		Py_None);
	SETBUILTIN("Ellipsis",		Py_Ellipsis);
	SETBUILTIN("NotImplemented",	Py_NotImplemented);
	SETBUILTIN("False",		Py_False);
	SETBUILTIN("True",		Py_True);
	SETBUILTIN("bool",		&PyBool_Type);
	SETBUILTIN("memoryview",        &PyMemoryView_Type);
	SETBUILTIN("bytearray",		&PyByteArray_Type);
	SETBUILTIN("bytes",		&PyBytes_Type);
	SETBUILTIN("classmethod",	&PyClassMethod_Type);
#ifndef WITHOUT_COMPLEX
	SETBUILTIN("complex",		&PyComplex_Type);
#endif
	SETBUILTIN("dict",		&PyDict_Type);
 	SETBUILTIN("enumerate",		&PyEnum_Type);
 	SETBUILTIN("filter",		&PyFilter_Type);
	SETBUILTIN("float",		&PyFloat_Type);
	SETBUILTIN("frozenset",		&PyFrozenSet_Type);
	SETBUILTIN("property",		&PyProperty_Type);
	SETBUILTIN("int",		&PyLong_Type);
	SETBUILTIN("list",		&PyList_Type);
	SETBUILTIN("map",		&PyMap_Type);
	SETBUILTIN("object",		&PyBaseObject_Type);
	SETBUILTIN("range",		&PyRange_Type);
	SETBUILTIN("reversed",		&PyReversed_Type);
	SETBUILTIN("set",		&PySet_Type);
	SETBUILTIN("slice",		&PySlice_Type);
	SETBUILTIN("staticmethod",	&PyStaticMethod_Type);
	SETBUILTIN("str",		&PyUnicode_Type);
	SETBUILTIN("super",		&PySuper_Type);
	SETBUILTIN("tuple",		&PyTuple_Type);
	SETBUILTIN("type",		&PyType_Type);
	SETBUILTIN("zip",		&PyZip_Type);
	debug = PyBool_FromLong(Py_OptimizeFlag == 0);
	if (PyDict_SetItemString(dict, "__debug__", debug) < 0) {
		Py_XDECREF(debug);
		return NULL;
	}
	Py_XDECREF(debug);

	return mod;
#undef ADD_TO_ALL
#undef SETBUILTIN
}
