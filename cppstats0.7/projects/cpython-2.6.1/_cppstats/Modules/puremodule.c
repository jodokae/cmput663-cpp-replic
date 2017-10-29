#include "Python.h"
#if defined(WITH_PURIFY) || defined(WITH_ALL_PURE)
#include <purify.h>
#define HAS_PURIFY_EXIT 0
#define PURE_PURIFY_VERSION 3
#endif
#if defined(WITH_QUANTIFY) || defined(WITH_ALL_PURE)
#include <quantify.h>
#define PURE_QUANTIFY_VERSION 2
#endif
#if defined(PURIFY_H) || defined(QUANTIFY_H)
#define COMMON_PURE_FUNCTIONS
#endif
typedef int (*VoidArgFunc)(void);
typedef int (*StringArgFunc)(char*);
typedef int (*PrintfishFunc)(const char*, ...);
typedef int (*StringIntArgFunc)(const char*, int);
static PyObject*
call_voidarg_function(VoidArgFunc func, PyObject *self, PyObject *args) {
int status;
if (!PyArg_ParseTuple(args, ""))
return NULL;
status = func();
return Py_BuildValue("i", status);
}
static PyObject*
call_stringarg_function(StringArgFunc func, PyObject *self, PyObject *args) {
int status;
char* stringarg;
if (!PyArg_ParseTuple(args, "s", &stringarg))
return NULL;
status = func(stringarg);
return Py_BuildValue("i", status);
}
static PyObject*
call_stringorint_function(StringArgFunc func, PyObject *self, PyObject *args) {
int status;
int intarg;
char* stringarg;
if (PyArg_ParseTuple(args, "i", &intarg))
status = func((char*)intarg);
else {
PyErr_Clear();
if (!PyArg_ParseTuple(args, "s", &stringarg))
return NULL;
else
status = func(stringarg);
}
return Py_BuildValue("i", status);
}
static PyObject*
call_printfish_function(PrintfishFunc func, PyObject *self, PyObject *args) {
int status;
char* argstring;
if (!PyArg_ParseTuple(args, "s", &argstring))
return NULL;
status = func("%s", argstring);
return Py_BuildValue("i", status);
}
static PyObject*
call_intasaddr_function(StringArgFunc func, PyObject *self, PyObject *args) {
long memrep;
int id;
if (!PyArg_ParseTuple(args, "l", &memrep))
return NULL;
id = func((char*)memrep);
return Py_BuildValue("i", id);
}
static PyObject*
call_stringandint_function(StringIntArgFunc func, PyObject *self,
PyObject *args) {
long srcrep;
int size;
int status;
if (!PyArg_ParseTuple(args, "li", &srcrep, &size))
return NULL;
status = func((char*)srcrep, size);
return Py_BuildValue("i", status);
}
#if defined(COMMON_PURE_FUNCTIONS)
static PyObject*
pure_pure_logfile_printf(PyObject* self, PyObject* args) {
return call_printfish_function(pure_logfile_printf, self, args);
}
static PyObject*
pure_pure_printf(PyObject* self, PyObject* args) {
return call_printfish_function(pure_printf, self, args);
}
static PyObject*
pure_pure_printf_with_banner(PyObject* self, PyObject* args) {
return call_printfish_function(pure_printf_with_banner, self, args);
}
#endif
#if defined(PURIFY_H)
static PyObject*
pure_purify_all_inuse(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_all_inuse, self, args);
}
static PyObject*
pure_purify_all_leaks(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_all_leaks, self, args);
}
static PyObject*
pure_purify_new_inuse(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_new_inuse, self, args);
}
static PyObject*
pure_purify_new_leaks(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_new_leaks, self, args);
}
static PyObject*
pure_purify_clear_inuse(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_clear_inuse, self, args);
}
static PyObject*
pure_purify_clear_leaks(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_clear_leaks, self, args);
}
static PyObject*
pure_purify_all_fds_inuse(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_all_fds_inuse, self, args);
}
static PyObject*
pure_purify_new_fds_inuse(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_new_fds_inuse, self, args);
}
static PyObject*
pure_purify_printf_with_call_chain(PyObject *self, PyObject *args) {
return call_printfish_function(purify_printf_with_call_chain,
self, args);
}
static PyObject*
pure_purify_set_pool_id(PyObject *self, PyObject *args) {
long memrep;
int id;
if (!PyArg_ParseTuple(args, "li:purify_set_pool_id", &memrep, &id))
return NULL;
purify_set_pool_id((char*)memrep, id);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject*
pure_purify_get_pool_id(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_get_pool_id, self, args);
}
static PyObject*
pure_purify_set_user_data(PyObject *self, PyObject *args) {
long memrep;
long datarep;
if (!PyArg_ParseTuple(args, "ll:purify_set_user_data", &memrep, &datarep))
return NULL;
purify_set_user_data((char*)memrep, (void*)datarep);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject*
pure_purify_get_user_data(PyObject *self, PyObject *args) {
long memrep;
void* data;
if (!PyArg_ParseTuple(args, "l:purify_get_user_data", &memrep))
return NULL;
data = purify_get_user_data((char*)memrep);
return Py_BuildValue("l", (long)data);
}
static PyObject* MapCallable = NULL;
static void
map_pool_callback(char* mem, int user_size, void *user_aux_data) {
long memrep = (long)mem;
long user_aux_data_rep = (long)user_aux_data;
PyObject* result;
PyObject* memobj = Py_BuildValue("lil", memrep, user_size,
user_aux_data_rep);
if (memobj == NULL)
return;
result = PyEval_CallObject(MapCallable, memobj);
Py_DECREF(result);
Py_DECREF(memobj);
}
static PyObject*
pure_purify_map_pool(PyObject *self, PyObject *args) {
PyObject* saved_callable = MapCallable;
PyObject* arg_callable;
int id;
if (!PyArg_ParseTuple(args, "iO:purify_map_pool", &id, &arg_callable))
return NULL;
if (!PyCallable_Check(arg_callable)) {
PyErr_SetString(PyExc_TypeError,
"Second argument must be callable");
return NULL;
}
MapCallable = arg_callable;
purify_map_pool(id, map_pool_callback);
MapCallable = saved_callable;
Py_INCREF(Py_None);
return Py_None;
}
static void
PurifyMapPoolIdCallback(int id) {
PyObject* result;
PyObject* intobj = Py_BuildValue("i", id);
if (intobj == NULL)
return;
result = PyEval_CallObject(MapCallable, intobj);
Py_DECREF(result);
Py_DECREF(intobj);
}
static PyObject*
pure_purify_map_pool_id(PyObject *self, PyObject *args) {
PyObject* saved_callable = MapCallable;
PyObject* arg_callable;
if (!PyArg_ParseTuple(args, "O:purify_map_pool_id", &arg_callable))
return NULL;
if (!PyCallable_Check(arg_callable)) {
PyErr_SetString(PyExc_TypeError, "Argument must be callable.");
return NULL;
}
MapCallable = arg_callable;
purify_map_pool_id(PurifyMapPoolIdCallback);
MapCallable = saved_callable;
Py_INCREF(Py_None);
return Py_None;
}
static PyObject*
pure_purify_new_messages(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_new_messages, self, args);
}
static PyObject*
pure_purify_all_messages(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_all_messages, self, args);
}
static PyObject*
pure_purify_clear_messages(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_clear_messages, self, args);
}
static PyObject*
pure_purify_clear_new_messages(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_clear_new_messages, self, args);
}
static PyObject*
pure_purify_start_batch(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_start_batch, self, args);
}
static PyObject*
pure_purify_start_batch_show_first(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_start_batch_show_first,
self, args);
}
static PyObject*
pure_purify_stop_batch(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_stop_batch, self, args);
}
static PyObject*
pure_purify_name_thread(PyObject *self, PyObject *args) {
int status;
char* stringarg;
if (!PyArg_ParseTuple(args, "s:purify_name_thread", &stringarg))
return NULL;
status = purify_name_thread(stringarg);
return Py_BuildValue("i", status);
}
static PyObject*
pure_purify_watch(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch, self, args);
}
static PyObject*
pure_purify_watch_1(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_1, self, args);
}
static PyObject*
pure_purify_watch_2(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_2, self, args);
}
static PyObject*
pure_purify_watch_4(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_4, self, args);
}
static PyObject*
pure_purify_watch_8(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_8, self, args);
}
static PyObject*
pure_purify_watch_w_1(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_w_1, self, args);
}
static PyObject*
pure_purify_watch_w_2(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_w_2, self, args);
}
static PyObject*
pure_purify_watch_w_4(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_w_4, self, args);
}
static PyObject*
pure_purify_watch_w_8(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_w_8, self, args);
}
static PyObject*
pure_purify_watch_r_1(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_r_1, self, args);
}
static PyObject*
pure_purify_watch_r_2(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_r_2, self, args);
}
static PyObject*
pure_purify_watch_r_4(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_r_4, self, args);
}
static PyObject*
pure_purify_watch_r_8(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_r_8, self, args);
}
static PyObject*
pure_purify_watch_rw_1(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_rw_1, self, args);
}
static PyObject*
pure_purify_watch_rw_2(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_rw_2, self, args);
}
static PyObject*
pure_purify_watch_rw_4(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_rw_4, self, args);
}
static PyObject*
pure_purify_watch_rw_8(PyObject *self, PyObject *args) {
return call_intasaddr_function(purify_watch_rw_8, self, args);
}
static PyObject*
pure_purify_watch_n(PyObject *self, PyObject *args) {
long addrrep;
unsigned int size;
char* type;
int status;
if (!PyArg_ParseTuple(args, "lis:purify_watch_n", &addrrep, &size, &type))
return NULL;
status = purify_watch_n((char*)addrrep, size, type);
return Py_BuildValue("i", status);
}
static PyObject*
pure_purify_watch_info(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_watch_info, self, args);
}
static PyObject*
pure_purify_watch_remove(PyObject *self, PyObject *args) {
int watchno;
int status;
if (!PyArg_ParseTuple(args, "i:purify_watch_remove", &watchno))
return NULL;
status = purify_watch_remove(watchno);
return Py_BuildValue("i", status);
}
static PyObject*
pure_purify_watch_remove_all(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_watch_remove_all, self, args);
}
static PyObject*
pure_purify_describe(PyObject *self, PyObject *args) {
long addrrep;
char* rtn;
if (!PyArg_ParseTuple(args, "l:purify_describe", &addrrep))
return NULL;
rtn = purify_describe((char*)addrrep);
return Py_BuildValue("l", (long)rtn);
}
static PyObject*
pure_purify_what_colors(PyObject *self, PyObject *args) {
long addrrep;
unsigned int size;
int status;
if (!PyArg_ParseTuple(args, "li:purify_what_colors", &addrrep, &size))
return NULL;
status = purify_what_colors((char*)addrrep, size);
return Py_BuildValue("i", status);
}
static PyObject*
pure_purify_is_running(PyObject *self, PyObject *args) {
return call_voidarg_function(purify_is_running, self, args);
}
static PyObject*
pure_purify_assert_is_readable(PyObject *self, PyObject *args) {
return call_stringandint_function(purify_assert_is_readable,
self, args);
}
static PyObject*
pure_purify_assert_is_writable(PyObject *self, PyObject *args) {
return call_stringandint_function(purify_assert_is_writable,
self, args);
}
#if HAS_PURIFY_EXIT
static PyObject*
pure_purify_exit(PyObject *self, PyObject *args) {
int status;
if (!PyArg_ParseTuple(args, "i:purify_exit", &status))
return NULL;
purify_exit(status);
Py_INCREF(Py_None);
return Py_None;
}
#endif
#endif
#if defined(QUANTIFY_H)
static PyObject*
pure_quantify_is_running(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_is_running, self, args);
}
static PyObject*
pure_quantify_help(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_help, self, args);
}
static PyObject*
pure_quantify_print_recording_state(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_print_recording_state,
self, args);
}
static PyObject*
pure_quantify_start_recording_data(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_start_recording_data,
self, args);
}
static PyObject*
pure_quantify_stop_recording_data(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_stop_recording_data, self, args);
}
static PyObject*
pure_quantify_is_recording_data(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_is_recording_data, self, args);
}
static PyObject*
pure_quantify_start_recording_system_calls(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_start_recording_system_calls,
self, args);
}
static PyObject*
pure_quantify_stop_recording_system_calls(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_stop_recording_system_calls,
self, args);
}
static PyObject*
pure_quantify_is_recording_system_calls(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_is_recording_system_calls,
self, args);
}
static PyObject*
pure_quantify_start_recording_system_call(PyObject *self, PyObject *args) {
return call_stringorint_function(quantify_start_recording_system_call,
self, args);
}
static PyObject*
pure_quantify_stop_recording_system_call(PyObject *self, PyObject *args) {
return call_stringorint_function(quantify_stop_recording_system_call,
self, args);
}
static PyObject*
pure_quantify_is_recording_system_call(PyObject *self, PyObject *args) {
return call_stringorint_function(quantify_is_recording_system_call,
self, args);
}
static PyObject*
pure_quantify_start_recording_dynamic_library_data(PyObject *self, PyObject *args) {
return call_voidarg_function(
quantify_start_recording_dynamic_library_data,
self, args);
}
static PyObject*
pure_quantify_stop_recording_dynamic_library_data(PyObject *self, PyObject *args) {
return call_voidarg_function(
quantify_stop_recording_dynamic_library_data,
self, args);
}
static PyObject*
pure_quantify_is_recording_dynamic_library_data(PyObject *self, PyObject *args) {
return call_voidarg_function(
quantify_is_recording_dynamic_library_data,
self, args);
}
static PyObject*
pure_quantify_start_recording_register_window_traps(PyObject *self, PyObject *args) {
return call_voidarg_function(
quantify_start_recording_register_window_traps,
self, args);
}
static PyObject*
pure_quantify_stop_recording_register_window_traps(PyObject *self, PyObject *args) {
return call_voidarg_function(
quantify_stop_recording_register_window_traps,
self, args);
}
static PyObject*
pure_quantify_is_recording_register_window_traps(PyObject *self, PyObject *args) {
return call_voidarg_function(
quantify_is_recording_register_window_traps,
self, args);
}
static PyObject*
pure_quantify_disable_recording_data(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_disable_recording_data,
self, args);
}
static PyObject*
pure_quantify_clear_data(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_clear_data, self, args);
}
static PyObject*
pure_quantify_save_data(PyObject *self, PyObject *args) {
return call_voidarg_function(quantify_save_data, self, args);
}
static PyObject*
pure_quantify_save_data_to_file(PyObject *self, PyObject *args) {
return call_stringarg_function(quantify_save_data_to_file, self, args);
}
static PyObject*
pure_quantify_add_annotation(PyObject *self, PyObject *args) {
return call_stringarg_function(quantify_add_annotation, self, args);
}
#endif
static struct PyMethodDef
pure_methods[] = {
#if defined(COMMON_PURE_FUNCTIONS)
{"pure_logfile_printf", pure_pure_logfile_printf, METH_VARARGS},
{"pure_printf", pure_pure_printf, METH_VARARGS},
{"pure_printf_with_banner", pure_pure_printf_with_banner, METH_VARARGS},
#endif
#if defined(PURIFY_H)
{"purify_all_inuse", pure_purify_all_inuse, METH_VARARGS},
{"purify_all_leaks", pure_purify_all_leaks, METH_VARARGS},
{"purify_new_inuse", pure_purify_new_inuse, METH_VARARGS},
{"purify_new_leaks", pure_purify_new_leaks, METH_VARARGS},
{"purify_clear_inuse", pure_purify_clear_inuse, METH_VARARGS},
{"purify_clear_leaks", pure_purify_clear_leaks, METH_VARARGS},
{"purify_all_fds_inuse", pure_purify_all_fds_inuse, METH_VARARGS},
{"purify_new_fds_inuse", pure_purify_new_fds_inuse, METH_VARARGS},
{"purify_logfile_printf", pure_pure_logfile_printf, METH_VARARGS},
{"purify_printf", pure_pure_printf, METH_VARARGS},
{"purify_printf_with_banner", pure_pure_printf_with_banner, METH_VARARGS},
{"purify_printf_with_call_chain", pure_purify_printf_with_call_chain, METH_VARARGS},
{"purify_set_pool_id", pure_purify_set_pool_id, METH_VARARGS},
{"purify_get_pool_id", pure_purify_get_pool_id, METH_VARARGS},
{"purify_set_user_data", pure_purify_set_user_data, METH_VARARGS},
{"purify_get_user_data", pure_purify_get_user_data, METH_VARARGS},
{"purify_map_pool", pure_purify_map_pool, METH_VARARGS},
{"purify_map_pool_id", pure_purify_map_pool_id, METH_VARARGS},
{"purify_new_messages", pure_purify_new_messages, METH_VARARGS},
{"purify_all_messages", pure_purify_all_messages, METH_VARARGS},
{"purify_clear_messages", pure_purify_clear_messages, METH_VARARGS},
{"purify_clear_new_messages", pure_purify_clear_new_messages, METH_VARARGS},
{"purify_start_batch", pure_purify_start_batch, METH_VARARGS},
{"purify_start_batch_show_first", pure_purify_start_batch_show_first, METH_VARARGS},
{"purify_stop_batch", pure_purify_stop_batch, METH_VARARGS},
{"purify_name_thread", pure_purify_name_thread, METH_VARARGS},
{"purify_watch", pure_purify_watch, METH_VARARGS},
{"purify_watch_1", pure_purify_watch_1, METH_VARARGS},
{"purify_watch_2", pure_purify_watch_2, METH_VARARGS},
{"purify_watch_4", pure_purify_watch_4, METH_VARARGS},
{"purify_watch_8", pure_purify_watch_8, METH_VARARGS},
{"purify_watch_w_1", pure_purify_watch_w_1, METH_VARARGS},
{"purify_watch_w_2", pure_purify_watch_w_2, METH_VARARGS},
{"purify_watch_w_4", pure_purify_watch_w_4, METH_VARARGS},
{"purify_watch_w_8", pure_purify_watch_w_8, METH_VARARGS},
{"purify_watch_r_1", pure_purify_watch_r_1, METH_VARARGS},
{"purify_watch_r_2", pure_purify_watch_r_2, METH_VARARGS},
{"purify_watch_r_4", pure_purify_watch_r_4, METH_VARARGS},
{"purify_watch_r_8", pure_purify_watch_r_8, METH_VARARGS},
{"purify_watch_rw_1", pure_purify_watch_rw_1, METH_VARARGS},
{"purify_watch_rw_2", pure_purify_watch_rw_2, METH_VARARGS},
{"purify_watch_rw_4", pure_purify_watch_rw_4, METH_VARARGS},
{"purify_watch_rw_8", pure_purify_watch_rw_8, METH_VARARGS},
{"purify_watch_n", pure_purify_watch_n, METH_VARARGS},
{"purify_watch_info", pure_purify_watch_info, METH_VARARGS},
{"purify_watch_remove", pure_purify_watch_remove, METH_VARARGS},
{"purify_watch_remove_all", pure_purify_watch_remove_all, METH_VARARGS},
{"purify_describe", pure_purify_describe, METH_VARARGS},
{"purify_what_colors", pure_purify_what_colors, METH_VARARGS},
{"purify_is_running", pure_purify_is_running, METH_VARARGS},
{"purify_assert_is_readable", pure_purify_assert_is_readable, METH_VARARGS},
{"purify_assert_is_writable", pure_purify_assert_is_writable, METH_VARARGS},
#if HAS_PURIFY_EXIT
{"purify_exit", pure_purify_exit, METH_VARARGS},
#endif
#endif
#if defined(QUANTIFY_H)
{"quantify_is_running", pure_quantify_is_running, METH_VARARGS},
{"quantify_help", pure_quantify_help, METH_VARARGS},
{"quantify_print_recording_state", pure_quantify_print_recording_state, METH_VARARGS},
{"quantify_start_recording_data", pure_quantify_start_recording_data, METH_VARARGS},
{"quantify_stop_recording_data", pure_quantify_stop_recording_data, METH_VARARGS},
{"quantify_is_recording_data", pure_quantify_is_recording_data, METH_VARARGS},
{
"quantify_start_recording_system_calls",
pure_quantify_start_recording_system_calls, METH_VARARGS
},
{
"quantify_stop_recording_system_calls",
pure_quantify_stop_recording_system_calls, METH_VARARGS
},
{
"quantify_is_recording_system_calls",
pure_quantify_is_recording_system_calls, METH_VARARGS
},
{
"quantify_start_recording_system_call",
pure_quantify_start_recording_system_call, METH_VARARGS
},
{
"quantify_stop_recording_system_call",
pure_quantify_stop_recording_system_call, METH_VARARGS
},
{
"quantify_is_recording_system_call",
pure_quantify_is_recording_system_call, METH_VARARGS
},
{
"quantify_start_recording_dynamic_library_data",
pure_quantify_start_recording_dynamic_library_data, METH_VARARGS
},
{
"quantify_stop_recording_dynamic_library_data",
pure_quantify_stop_recording_dynamic_library_data, METH_VARARGS
},
{
"quantify_is_recording_dynamic_library_data",
pure_quantify_is_recording_dynamic_library_data, METH_VARARGS
},
{
"quantify_start_recording_register_window_traps",
pure_quantify_start_recording_register_window_traps, METH_VARARGS
},
{
"quantify_stop_recording_register_window_traps",
pure_quantify_stop_recording_register_window_traps, METH_VARARGS
},
{
"quantify_is_recording_register_window_traps",
pure_quantify_is_recording_register_window_traps, METH_VARARGS
},
{
"quantify_disable_recording_data",
pure_quantify_disable_recording_data, METH_VARARGS
},
{"quantify_clear_data", pure_quantify_clear_data, METH_VARARGS},
{"quantify_save_data", pure_quantify_save_data, METH_VARARGS},
{"quantify_save_data_to_file", pure_quantify_save_data_to_file, METH_VARARGS},
{"quantify_add_annotation", pure_quantify_add_annotation, METH_VARARGS},
#endif
{NULL, NULL}
};
static void
ins(d, name, val)
PyObject *d;
char* name;
long val;
{
PyObject *v = PyInt_FromLong(val);
if (v) {
(void)PyDict_SetItemString(d, name, v);
Py_DECREF(v);
}
}
void
initpure() {
PyObject *m, *d;
if (PyErr_WarnPy3k("the pure module has been removed in "
"Python 3.0", 2) < 0)
return;
m = Py_InitModule("pure", pure_methods);
if (m == NULL)
return;
d = PyModule_GetDict(m);
#if defined(PURE_PURIFY_VERSION)
ins(d, "PURIFY_VERSION", PURE_PURIFY_VERSION);
#else
PyDict_SetItemString(d, "PURIFY_VERSION", Py_None);
#endif
#if defined(PURIFY_EXIT_ERRORS)
ins(d, "PURIFY_EXIT_ERRORS", PURIFY_EXIT_ERRORS);
#endif
#if defined(PURIFY_EXIT_LEAKS)
ins(d, "PURIFY_EXIT_LEAKS", PURIFY_EXIT_LEAKS);
#endif
#if defined(PURIFY_EXIT_PLEAKS)
ins(d, "PURIFY_EXIT_PLEAKS", PURIFY_EXIT_PLEAKS);
#endif
#if defined(PURE_QUANTIFY_VERSION)
ins(d, "QUANTIFY_VERSION", PURE_QUANTIFY_VERSION);
#else
PyDict_SetItemString(d, "QUANTIFY_VERSION", Py_None);
#endif
}