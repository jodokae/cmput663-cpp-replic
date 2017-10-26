static PyObject *
TESTNAME(PyObject *error(const char*)) {
const int NBITS = sizeof(TYPENAME) * 8;
unsigned TYPENAME base;
PyObject *pyresult;
int i;
base = 1;
for (i = 0;
i < NBITS + 1;
++i, base <<= 1) {
int j;
for (j = 0; j < 6; ++j) {
TYPENAME in, out;
unsigned TYPENAME uin, uout;
uin = j < 3 ? base
: (unsigned TYPENAME)(-(TYPENAME)base);
uin += (unsigned TYPENAME)(TYPENAME)(j % 3 - 1);
pyresult = F_U_TO_PY(uin);
if (pyresult == NULL)
return error(
"unsigned unexpected null result");
uout = F_PY_TO_U(pyresult);
if (uout == (unsigned TYPENAME)-1 && PyErr_Occurred())
return error(
"unsigned unexpected -1 result");
if (uout != uin)
return error(
"unsigned output != input");
UNBIND(pyresult);
in = (TYPENAME)uin;
pyresult = F_S_TO_PY(in);
if (pyresult == NULL)
return error(
"signed unexpected null result");
out = F_PY_TO_S(pyresult);
if (out == (TYPENAME)-1 && PyErr_Occurred())
return error(
"signed unexpected -1 result");
if (out != in)
return error(
"signed output != input");
UNBIND(pyresult);
}
}
{
PyObject *one, *x, *y;
TYPENAME out;
unsigned TYPENAME uout;
one = PyLong_FromLong(1);
if (one == NULL)
return error(
"unexpected NULL from PyLong_FromLong");
x = PyNumber_Negative(one);
if (x == NULL)
return error(
"unexpected NULL from PyNumber_Negative");
uout = F_PY_TO_U(x);
if (uout != (unsigned TYPENAME)-1 || !PyErr_Occurred())
return error(
"PyLong_AsUnsignedXXX(-1) didn't complain");
PyErr_Clear();
UNBIND(x);
y = PyLong_FromLong((long)NBITS);
if (y == NULL)
return error(
"unexpected NULL from PyLong_FromLong");
x = PyNumber_Lshift(one, y);
UNBIND(y);
if (x == NULL)
return error(
"unexpected NULL from PyNumber_Lshift");
uout = F_PY_TO_U(x);
if (uout != (unsigned TYPENAME)-1 || !PyErr_Occurred())
return error(
"PyLong_AsUnsignedXXX(2**NBITS) didn't "
"complain");
PyErr_Clear();
y = PyNumber_Rshift(x, one);
UNBIND(x);
if (y == NULL)
return error(
"unexpected NULL from PyNumber_Rshift");
out = F_PY_TO_S(y);
if (out != (TYPENAME)-1 || !PyErr_Occurred())
return error(
"PyLong_AsXXX(2**(NBITS-1)) didn't "
"complain");
PyErr_Clear();
x = PyNumber_Negative(y);
UNBIND(y);
if (x == NULL)
return error(
"unexpected NULL from PyNumber_Negative");
y = PyNumber_Subtract(x, one);
UNBIND(x);
if (y == NULL)
return error(
"unexpected NULL from PyNumber_Subtract");
out = F_PY_TO_S(y);
if (out != (TYPENAME)-1 || !PyErr_Occurred())
return error(
"PyLong_AsXXX(-2**(NBITS-1)-1) didn't "
"complain");
PyErr_Clear();
UNBIND(y);
Py_XDECREF(x);
Py_XDECREF(y);
Py_DECREF(one);
}
Py_INCREF(Py_None);
return Py_None;
}
