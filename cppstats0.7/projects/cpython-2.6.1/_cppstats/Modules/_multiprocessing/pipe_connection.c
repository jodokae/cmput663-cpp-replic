#include "multiprocessing.h"
#define CLOSE(h) CloseHandle(h)
static Py_ssize_t
conn_send_string(ConnectionObject *conn, char *string, size_t length) {
DWORD amount_written;
BOOL ret;
Py_BEGIN_ALLOW_THREADS
ret = WriteFile(conn->handle, string, length, &amount_written, NULL);
Py_END_ALLOW_THREADS
return ret ? MP_SUCCESS : MP_STANDARD_ERROR;
}
static Py_ssize_t
conn_recv_string(ConnectionObject *conn, char *buffer,
size_t buflength, char **newbuffer, size_t maxlength) {
DWORD left, length, full_length, err;
BOOL ret;
*newbuffer = NULL;
Py_BEGIN_ALLOW_THREADS
ret = ReadFile(conn->handle, buffer, MIN(buflength, maxlength),
&length, NULL);
Py_END_ALLOW_THREADS
if (ret)
return length;
err = GetLastError();
if (err != ERROR_MORE_DATA) {
if (err == ERROR_BROKEN_PIPE)
return MP_END_OF_FILE;
return MP_STANDARD_ERROR;
}
if (!PeekNamedPipe(conn->handle, NULL, 0, NULL, NULL, &left))
return MP_STANDARD_ERROR;
full_length = length + left;
if (full_length > maxlength)
return MP_BAD_MESSAGE_LENGTH;
*newbuffer = PyMem_Malloc(full_length);
if (*newbuffer == NULL)
return MP_MEMORY_ERROR;
memcpy(*newbuffer, buffer, length);
Py_BEGIN_ALLOW_THREADS
ret = ReadFile(conn->handle, *newbuffer+length, left, &length, NULL);
Py_END_ALLOW_THREADS
if (ret) {
assert(length == left);
return full_length;
} else {
PyMem_Free(*newbuffer);
return MP_STANDARD_ERROR;
}
}
#define conn_poll(conn, timeout) conn_poll_save(conn, timeout, _save)
static int
conn_poll_save(ConnectionObject *conn, double timeout, PyThreadState *_save) {
DWORD bytes, deadline, delay;
int difference, res;
BOOL block = FALSE;
if (!PeekNamedPipe(conn->handle, NULL, 0, NULL, &bytes, NULL))
return MP_STANDARD_ERROR;
if (timeout == 0.0)
return bytes > 0;
if (timeout < 0.0)
block = TRUE;
else
deadline = GetTickCount() + (DWORD)(1000 * timeout + 0.5);
Sleep(0);
for (delay = 1 ; ; delay += 1) {
if (!PeekNamedPipe(conn->handle, NULL, 0, NULL, &bytes, NULL))
return MP_STANDARD_ERROR;
else if (bytes > 0)
return TRUE;
if (!block) {
difference = deadline - GetTickCount();
if (difference < 0)
return FALSE;
if ((int)delay > difference)
delay = difference;
}
if (delay > 20)
delay = 20;
Sleep(delay);
Py_BLOCK_THREADS
res = PyErr_CheckSignals();
Py_UNBLOCK_THREADS
if (res)
return MP_EXCEPTION_HAS_BEEN_SET;
}
}
#define CONNECTION_NAME "PipeConnection"
#define CONNECTION_TYPE PipeConnectionType
#include "connection.h"