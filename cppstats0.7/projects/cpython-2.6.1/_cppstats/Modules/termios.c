#include "Python.h"
#define PyInit_termios inittermios
#if defined(__sgi)
#define CTRL(c) ((c)&037)
#endif
#include <termios.h>
#if defined(__osf__)
#include <termio.h>
#endif
#include <sys/ioctl.h>
#if defined(HAVE_SYS_MODEM_H)
#include <sys/modem.h>
#endif
#if defined(HAVE_SYS_BSDTTY_H)
#include <sys/bsdtty.h>
#endif
PyDoc_STRVAR(termios__doc__,
"This module provides an interface to the Posix calls for tty I/O control.\n\
For a complete description of these calls, see the Posix or Unix manual\n\
pages. It is only available for those Unix versions that support Posix\n\
termios style tty I/O control.\n\
\n\
All functions in this module take a file descriptor fd as their first\n\
argument. This can be an integer file descriptor, such as returned by\n\
sys.stdin.fileno(), or a file object, such as sys.stdin itself.");
static PyObject *TermiosError;
static int fdconv(PyObject* obj, void* p) {
int fd;
fd = PyObject_AsFileDescriptor(obj);
if (fd >= 0) {
*(int*)p = fd;
return 1;
}
return 0;
}
PyDoc_STRVAR(termios_tcgetattr__doc__,
"tcgetattr(fd) -> list_of_attrs\n\
\n\
Get the tty attributes for file descriptor fd, as follows:\n\
[iflag, oflag, cflag, lflag, ispeed, ospeed, cc] where cc is a list\n\
of the tty special characters (each a string of length 1, except the items\n\
with indices VMIN and VTIME, which are integers when these fields are\n\
defined). The interpretation of the flags and the speeds as well as the\n\
indexing in the cc array must be done using the symbolic constants defined\n\
in this module.");
static PyObject *
termios_tcgetattr(PyObject *self, PyObject *args) {
int fd;
struct termios mode;
PyObject *cc;
speed_t ispeed, ospeed;
PyObject *v;
int i;
char ch;
if (!PyArg_ParseTuple(args, "O&:tcgetattr",
fdconv, (void*)&fd))
return NULL;
if (tcgetattr(fd, &mode) == -1)
return PyErr_SetFromErrno(TermiosError);
ispeed = cfgetispeed(&mode);
ospeed = cfgetospeed(&mode);
cc = PyList_New(NCCS);
if (cc == NULL)
return NULL;
for (i = 0; i < NCCS; i++) {
ch = (char)mode.c_cc[i];
v = PyString_FromStringAndSize(&ch, 1);
if (v == NULL)
goto err;
PyList_SetItem(cc, i, v);
}
if ((mode.c_lflag & ICANON) == 0) {
v = PyInt_FromLong((long)mode.c_cc[VMIN]);
if (v == NULL)
goto err;
PyList_SetItem(cc, VMIN, v);
v = PyInt_FromLong((long)mode.c_cc[VTIME]);
if (v == NULL)
goto err;
PyList_SetItem(cc, VTIME, v);
}
if (!(v = PyList_New(7)))
goto err;
PyList_SetItem(v, 0, PyInt_FromLong((long)mode.c_iflag));
PyList_SetItem(v, 1, PyInt_FromLong((long)mode.c_oflag));
PyList_SetItem(v, 2, PyInt_FromLong((long)mode.c_cflag));
PyList_SetItem(v, 3, PyInt_FromLong((long)mode.c_lflag));
PyList_SetItem(v, 4, PyInt_FromLong((long)ispeed));
PyList_SetItem(v, 5, PyInt_FromLong((long)ospeed));
PyList_SetItem(v, 6, cc);
if (PyErr_Occurred()) {
Py_DECREF(v);
goto err;
}
return v;
err:
Py_DECREF(cc);
return NULL;
}
PyDoc_STRVAR(termios_tcsetattr__doc__,
"tcsetattr(fd, when, attributes) -> None\n\
\n\
Set the tty attributes for file descriptor fd.\n\
The attributes to be set are taken from the attributes argument, which\n\
is a list like the one returned by tcgetattr(). The when argument\n\
determines when the attributes are changed: termios.TCSANOW to\n\
change immediately, termios.TCSADRAIN to change after transmitting all\n\
queued output, or termios.TCSAFLUSH to change after transmitting all\n\
queued output and discarding all queued input. ");
static PyObject *
termios_tcsetattr(PyObject *self, PyObject *args) {
int fd, when;
struct termios mode;
speed_t ispeed, ospeed;
PyObject *term, *cc, *v;
int i;
if (!PyArg_ParseTuple(args, "O&iO:tcsetattr",
fdconv, &fd, &when, &term))
return NULL;
if (!PyList_Check(term) || PyList_Size(term) != 7) {
PyErr_SetString(PyExc_TypeError,
"tcsetattr, arg 3: must be 7 element list");
return NULL;
}
if (tcgetattr(fd, &mode) == -1)
return PyErr_SetFromErrno(TermiosError);
mode.c_iflag = (tcflag_t) PyInt_AsLong(PyList_GetItem(term, 0));
mode.c_oflag = (tcflag_t) PyInt_AsLong(PyList_GetItem(term, 1));
mode.c_cflag = (tcflag_t) PyInt_AsLong(PyList_GetItem(term, 2));
mode.c_lflag = (tcflag_t) PyInt_AsLong(PyList_GetItem(term, 3));
ispeed = (speed_t) PyInt_AsLong(PyList_GetItem(term, 4));
ospeed = (speed_t) PyInt_AsLong(PyList_GetItem(term, 5));
cc = PyList_GetItem(term, 6);
if (PyErr_Occurred())
return NULL;
if (!PyList_Check(cc) || PyList_Size(cc) != NCCS) {
PyErr_Format(PyExc_TypeError,
"tcsetattr: attributes[6] must be %d element list",
NCCS);
return NULL;
}
for (i = 0; i < NCCS; i++) {
v = PyList_GetItem(cc, i);
if (PyString_Check(v) && PyString_Size(v) == 1)
mode.c_cc[i] = (cc_t) * PyString_AsString(v);
else if (PyInt_Check(v))
mode.c_cc[i] = (cc_t) PyInt_AsLong(v);
else {
PyErr_SetString(PyExc_TypeError,
"tcsetattr: elements of attributes must be characters or integers");
return NULL;
}
}
if (cfsetispeed(&mode, (speed_t) ispeed) == -1)
return PyErr_SetFromErrno(TermiosError);
if (cfsetospeed(&mode, (speed_t) ospeed) == -1)
return PyErr_SetFromErrno(TermiosError);
if (tcsetattr(fd, when, &mode) == -1)
return PyErr_SetFromErrno(TermiosError);
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(termios_tcsendbreak__doc__,
"tcsendbreak(fd, duration) -> None\n\
\n\
Send a break on file descriptor fd.\n\
A zero duration sends a break for 0.25-0.5 seconds; a nonzero duration\n\
has a system dependent meaning.");
static PyObject *
termios_tcsendbreak(PyObject *self, PyObject *args) {
int fd, duration;
if (!PyArg_ParseTuple(args, "O&i:tcsendbreak",
fdconv, &fd, &duration))
return NULL;
if (tcsendbreak(fd, duration) == -1)
return PyErr_SetFromErrno(TermiosError);
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(termios_tcdrain__doc__,
"tcdrain(fd) -> None\n\
\n\
Wait until all output written to file descriptor fd has been transmitted.");
static PyObject *
termios_tcdrain(PyObject *self, PyObject *args) {
int fd;
if (!PyArg_ParseTuple(args, "O&:tcdrain",
fdconv, &fd))
return NULL;
if (tcdrain(fd) == -1)
return PyErr_SetFromErrno(TermiosError);
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(termios_tcflush__doc__,
"tcflush(fd, queue) -> None\n\
\n\
Discard queued data on file descriptor fd.\n\
The queue selector specifies which queue: termios.TCIFLUSH for the input\n\
queue, termios.TCOFLUSH for the output queue, or termios.TCIOFLUSH for\n\
both queues. ");
static PyObject *
termios_tcflush(PyObject *self, PyObject *args) {
int fd, queue;
if (!PyArg_ParseTuple(args, "O&i:tcflush",
fdconv, &fd, &queue))
return NULL;
if (tcflush(fd, queue) == -1)
return PyErr_SetFromErrno(TermiosError);
Py_INCREF(Py_None);
return Py_None;
}
PyDoc_STRVAR(termios_tcflow__doc__,
"tcflow(fd, action) -> None\n\
\n\
Suspend or resume input or output on file descriptor fd.\n\
The action argument can be termios.TCOOFF to suspend output,\n\
termios.TCOON to restart output, termios.TCIOFF to suspend input,\n\
or termios.TCION to restart input.");
static PyObject *
termios_tcflow(PyObject *self, PyObject *args) {
int fd, action;
if (!PyArg_ParseTuple(args, "O&i:tcflow",
fdconv, &fd, &action))
return NULL;
if (tcflow(fd, action) == -1)
return PyErr_SetFromErrno(TermiosError);
Py_INCREF(Py_None);
return Py_None;
}
static PyMethodDef termios_methods[] = {
{
"tcgetattr", termios_tcgetattr,
METH_VARARGS, termios_tcgetattr__doc__
},
{
"tcsetattr", termios_tcsetattr,
METH_VARARGS, termios_tcsetattr__doc__
},
{
"tcsendbreak", termios_tcsendbreak,
METH_VARARGS, termios_tcsendbreak__doc__
},
{
"tcdrain", termios_tcdrain,
METH_VARARGS, termios_tcdrain__doc__
},
{
"tcflush", termios_tcflush,
METH_VARARGS, termios_tcflush__doc__
},
{
"tcflow", termios_tcflow,
METH_VARARGS, termios_tcflow__doc__
},
{NULL, NULL}
};
#if defined(VSWTCH) && !defined(VSWTC)
#define VSWTC VSWTCH
#endif
#if defined(VSWTC) && !defined(VSWTCH)
#define VSWTCH VSWTC
#endif
static struct constant {
char *name;
long value;
} termios_constants[] = {
{"B0", B0},
{"B50", B50},
{"B75", B75},
{"B110", B110},
{"B134", B134},
{"B150", B150},
{"B200", B200},
{"B300", B300},
{"B600", B600},
{"B1200", B1200},
{"B1800", B1800},
{"B2400", B2400},
{"B4800", B4800},
{"B9600", B9600},
{"B19200", B19200},
{"B38400", B38400},
#if defined(B57600)
{"B57600", B57600},
#endif
#if defined(B115200)
{"B115200", B115200},
#endif
#if defined(B230400)
{"B230400", B230400},
#endif
#if defined(CBAUDEX)
{"CBAUDEX", CBAUDEX},
#endif
{"TCSANOW", TCSANOW},
{"TCSADRAIN", TCSADRAIN},
{"TCSAFLUSH", TCSAFLUSH},
{"TCIFLUSH", TCIFLUSH},
{"TCOFLUSH", TCOFLUSH},
{"TCIOFLUSH", TCIOFLUSH},
{"TCOOFF", TCOOFF},
{"TCOON", TCOON},
{"TCIOFF", TCIOFF},
{"TCION", TCION},
{"IGNBRK", IGNBRK},
{"BRKINT", BRKINT},
{"IGNPAR", IGNPAR},
{"PARMRK", PARMRK},
{"INPCK", INPCK},
{"ISTRIP", ISTRIP},
{"INLCR", INLCR},
{"IGNCR", IGNCR},
{"ICRNL", ICRNL},
#if defined(IUCLC)
{"IUCLC", IUCLC},
#endif
{"IXON", IXON},
{"IXANY", IXANY},
{"IXOFF", IXOFF},
#if defined(IMAXBEL)
{"IMAXBEL", IMAXBEL},
#endif
{"OPOST", OPOST},
#if defined(OLCUC)
{"OLCUC", OLCUC},
#endif
#if defined(ONLCR)
{"ONLCR", ONLCR},
#endif
#if defined(OCRNL)
{"OCRNL", OCRNL},
#endif
#if defined(ONOCR)
{"ONOCR", ONOCR},
#endif
#if defined(ONLRET)
{"ONLRET", ONLRET},
#endif
#if defined(OFILL)
{"OFILL", OFILL},
#endif
#if defined(OFDEL)
{"OFDEL", OFDEL},
#endif
#if defined(NLDLY)
{"NLDLY", NLDLY},
#endif
#if defined(CRDLY)
{"CRDLY", CRDLY},
#endif
#if defined(TABDLY)
{"TABDLY", TABDLY},
#endif
#if defined(BSDLY)
{"BSDLY", BSDLY},
#endif
#if defined(VTDLY)
{"VTDLY", VTDLY},
#endif
#if defined(FFDLY)
{"FFDLY", FFDLY},
#endif
#if defined(NL0)
{"NL0", NL0},
#endif
#if defined(NL1)
{"NL1", NL1},
#endif
#if defined(CR0)
{"CR0", CR0},
#endif
#if defined(CR1)
{"CR1", CR1},
#endif
#if defined(CR2)
{"CR2", CR2},
#endif
#if defined(CR3)
{"CR3", CR3},
#endif
#if defined(TAB0)
{"TAB0", TAB0},
#endif
#if defined(TAB1)
{"TAB1", TAB1},
#endif
#if defined(TAB2)
{"TAB2", TAB2},
#endif
#if defined(TAB3)
{"TAB3", TAB3},
#endif
#if defined(XTABS)
{"XTABS", XTABS},
#endif
#if defined(BS0)
{"BS0", BS0},
#endif
#if defined(BS1)
{"BS1", BS1},
#endif
#if defined(VT0)
{"VT0", VT0},
#endif
#if defined(VT1)
{"VT1", VT1},
#endif
#if defined(FF0)
{"FF0", FF0},
#endif
#if defined(FF1)
{"FF1", FF1},
#endif
{"CSIZE", CSIZE},
{"CSTOPB", CSTOPB},
{"CREAD", CREAD},
{"PARENB", PARENB},
{"PARODD", PARODD},
{"HUPCL", HUPCL},
{"CLOCAL", CLOCAL},
#if defined(CIBAUD)
{"CIBAUD", CIBAUD},
#endif
#if defined(CRTSCTS)
{"CRTSCTS", (long)CRTSCTS},
#endif
{"CS5", CS5},
{"CS6", CS6},
{"CS7", CS7},
{"CS8", CS8},
{"ISIG", ISIG},
{"ICANON", ICANON},
#if defined(XCASE)
{"XCASE", XCASE},
#endif
{"ECHO", ECHO},
{"ECHOE", ECHOE},
{"ECHOK", ECHOK},
{"ECHONL", ECHONL},
#if defined(ECHOCTL)
{"ECHOCTL", ECHOCTL},
#endif
#if defined(ECHOPRT)
{"ECHOPRT", ECHOPRT},
#endif
#if defined(ECHOKE)
{"ECHOKE", ECHOKE},
#endif
#if defined(FLUSHO)
{"FLUSHO", FLUSHO},
#endif
{"NOFLSH", NOFLSH},
{"TOSTOP", TOSTOP},
#if defined(PENDIN)
{"PENDIN", PENDIN},
#endif
{"IEXTEN", IEXTEN},
{"VINTR", VINTR},
{"VQUIT", VQUIT},
{"VERASE", VERASE},
{"VKILL", VKILL},
{"VEOF", VEOF},
{"VTIME", VTIME},
{"VMIN", VMIN},
#if defined(VSWTC)
{"VSWTC", VSWTC},
{"VSWTCH", VSWTCH},
#endif
{"VSTART", VSTART},
{"VSTOP", VSTOP},
{"VSUSP", VSUSP},
{"VEOL", VEOL},
#if defined(VREPRINT)
{"VREPRINT", VREPRINT},
#endif
#if defined(VDISCARD)
{"VDISCARD", VDISCARD},
#endif
#if defined(VWERASE)
{"VWERASE", VWERASE},
#endif
#if defined(VLNEXT)
{"VLNEXT", VLNEXT},
#endif
#if defined(VEOL2)
{"VEOL2", VEOL2},
#endif
#if defined(B460800)
{"B460800", B460800},
#endif
#if defined(CBAUD)
{"CBAUD", CBAUD},
#endif
#if defined(CDEL)
{"CDEL", CDEL},
#endif
#if defined(CDSUSP)
{"CDSUSP", CDSUSP},
#endif
#if defined(CEOF)
{"CEOF", CEOF},
#endif
#if defined(CEOL)
{"CEOL", CEOL},
#endif
#if defined(CEOL2)
{"CEOL2", CEOL2},
#endif
#if defined(CEOT)
{"CEOT", CEOT},
#endif
#if defined(CERASE)
{"CERASE", CERASE},
#endif
#if defined(CESC)
{"CESC", CESC},
#endif
#if defined(CFLUSH)
{"CFLUSH", CFLUSH},
#endif
#if defined(CINTR)
{"CINTR", CINTR},
#endif
#if defined(CKILL)
{"CKILL", CKILL},
#endif
#if defined(CLNEXT)
{"CLNEXT", CLNEXT},
#endif
#if defined(CNUL)
{"CNUL", CNUL},
#endif
#if defined(COMMON)
{"COMMON", COMMON},
#endif
#if defined(CQUIT)
{"CQUIT", CQUIT},
#endif
#if defined(CRPRNT)
{"CRPRNT", CRPRNT},
#endif
#if defined(CSTART)
{"CSTART", CSTART},
#endif
#if defined(CSTOP)
{"CSTOP", CSTOP},
#endif
#if defined(CSUSP)
{"CSUSP", CSUSP},
#endif
#if defined(CSWTCH)
{"CSWTCH", CSWTCH},
#endif
#if defined(CWERASE)
{"CWERASE", CWERASE},
#endif
#if defined(EXTA)
{"EXTA", EXTA},
#endif
#if defined(EXTB)
{"EXTB", EXTB},
#endif
#if defined(FIOASYNC)
{"FIOASYNC", FIOASYNC},
#endif
#if defined(FIOCLEX)
{"FIOCLEX", FIOCLEX},
#endif
#if defined(FIONBIO)
{"FIONBIO", FIONBIO},
#endif
#if defined(FIONCLEX)
{"FIONCLEX", FIONCLEX},
#endif
#if defined(FIONREAD)
{"FIONREAD", FIONREAD},
#endif
#if defined(IBSHIFT)
{"IBSHIFT", IBSHIFT},
#endif
#if defined(INIT_C_CC)
{"INIT_C_CC", INIT_C_CC},
#endif
#if defined(IOCSIZE_MASK)
{"IOCSIZE_MASK", IOCSIZE_MASK},
#endif
#if defined(IOCSIZE_SHIFT)
{"IOCSIZE_SHIFT", IOCSIZE_SHIFT},
#endif
#if defined(NCC)
{"NCC", NCC},
#endif
#if defined(NCCS)
{"NCCS", NCCS},
#endif
#if defined(NSWTCH)
{"NSWTCH", NSWTCH},
#endif
#if defined(N_MOUSE)
{"N_MOUSE", N_MOUSE},
#endif
#if defined(N_PPP)
{"N_PPP", N_PPP},
#endif
#if defined(N_SLIP)
{"N_SLIP", N_SLIP},
#endif
#if defined(N_STRIP)
{"N_STRIP", N_STRIP},
#endif
#if defined(N_TTY)
{"N_TTY", N_TTY},
#endif
#if defined(TCFLSH)
{"TCFLSH", TCFLSH},
#endif
#if defined(TCGETA)
{"TCGETA", TCGETA},
#endif
#if defined(TCGETS)
{"TCGETS", TCGETS},
#endif
#if defined(TCSBRK)
{"TCSBRK", TCSBRK},
#endif
#if defined(TCSBRKP)
{"TCSBRKP", TCSBRKP},
#endif
#if defined(TCSETA)
{"TCSETA", TCSETA},
#endif
#if defined(TCSETAF)
{"TCSETAF", TCSETAF},
#endif
#if defined(TCSETAW)
{"TCSETAW", TCSETAW},
#endif
#if defined(TCSETS)
{"TCSETS", TCSETS},
#endif
#if defined(TCSETSF)
{"TCSETSF", TCSETSF},
#endif
#if defined(TCSETSW)
{"TCSETSW", TCSETSW},
#endif
#if defined(TCXONC)
{"TCXONC", TCXONC},
#endif
#if defined(TIOCCONS)
{"TIOCCONS", TIOCCONS},
#endif
#if defined(TIOCEXCL)
{"TIOCEXCL", TIOCEXCL},
#endif
#if defined(TIOCGETD)
{"TIOCGETD", TIOCGETD},
#endif
#if defined(TIOCGICOUNT)
{"TIOCGICOUNT", TIOCGICOUNT},
#endif
#if defined(TIOCGLCKTRMIOS)
{"TIOCGLCKTRMIOS", TIOCGLCKTRMIOS},
#endif
#if defined(TIOCGPGRP)
{"TIOCGPGRP", TIOCGPGRP},
#endif
#if defined(TIOCGSERIAL)
{"TIOCGSERIAL", TIOCGSERIAL},
#endif
#if defined(TIOCGSOFTCAR)
{"TIOCGSOFTCAR", TIOCGSOFTCAR},
#endif
#if defined(TIOCGWINSZ)
{"TIOCGWINSZ", TIOCGWINSZ},
#endif
#if defined(TIOCINQ)
{"TIOCINQ", TIOCINQ},
#endif
#if defined(TIOCLINUX)
{"TIOCLINUX", TIOCLINUX},
#endif
#if defined(TIOCMBIC)
{"TIOCMBIC", TIOCMBIC},
#endif
#if defined(TIOCMBIS)
{"TIOCMBIS", TIOCMBIS},
#endif
#if defined(TIOCMGET)
{"TIOCMGET", TIOCMGET},
#endif
#if defined(TIOCMIWAIT)
{"TIOCMIWAIT", TIOCMIWAIT},
#endif
#if defined(TIOCMSET)
{"TIOCMSET", TIOCMSET},
#endif
#if defined(TIOCM_CAR)
{"TIOCM_CAR", TIOCM_CAR},
#endif
#if defined(TIOCM_CD)
{"TIOCM_CD", TIOCM_CD},
#endif
#if defined(TIOCM_CTS)
{"TIOCM_CTS", TIOCM_CTS},
#endif
#if defined(TIOCM_DSR)
{"TIOCM_DSR", TIOCM_DSR},
#endif
#if defined(TIOCM_DTR)
{"TIOCM_DTR", TIOCM_DTR},
#endif
#if defined(TIOCM_LE)
{"TIOCM_LE", TIOCM_LE},
#endif
#if defined(TIOCM_RI)
{"TIOCM_RI", TIOCM_RI},
#endif
#if defined(TIOCM_RNG)
{"TIOCM_RNG", TIOCM_RNG},
#endif
#if defined(TIOCM_RTS)
{"TIOCM_RTS", TIOCM_RTS},
#endif
#if defined(TIOCM_SR)
{"TIOCM_SR", TIOCM_SR},
#endif
#if defined(TIOCM_ST)
{"TIOCM_ST", TIOCM_ST},
#endif
#if defined(TIOCNOTTY)
{"TIOCNOTTY", TIOCNOTTY},
#endif
#if defined(TIOCNXCL)
{"TIOCNXCL", TIOCNXCL},
#endif
#if defined(TIOCOUTQ)
{"TIOCOUTQ", TIOCOUTQ},
#endif
#if defined(TIOCPKT)
{"TIOCPKT", TIOCPKT},
#endif
#if defined(TIOCPKT_DATA)
{"TIOCPKT_DATA", TIOCPKT_DATA},
#endif
#if defined(TIOCPKT_DOSTOP)
{"TIOCPKT_DOSTOP", TIOCPKT_DOSTOP},
#endif
#if defined(TIOCPKT_FLUSHREAD)
{"TIOCPKT_FLUSHREAD", TIOCPKT_FLUSHREAD},
#endif
#if defined(TIOCPKT_FLUSHWRITE)
{"TIOCPKT_FLUSHWRITE", TIOCPKT_FLUSHWRITE},
#endif
#if defined(TIOCPKT_NOSTOP)
{"TIOCPKT_NOSTOP", TIOCPKT_NOSTOP},
#endif
#if defined(TIOCPKT_START)
{"TIOCPKT_START", TIOCPKT_START},
#endif
#if defined(TIOCPKT_STOP)
{"TIOCPKT_STOP", TIOCPKT_STOP},
#endif
#if defined(TIOCSCTTY)
{"TIOCSCTTY", TIOCSCTTY},
#endif
#if defined(TIOCSERCONFIG)
{"TIOCSERCONFIG", TIOCSERCONFIG},
#endif
#if defined(TIOCSERGETLSR)
{"TIOCSERGETLSR", TIOCSERGETLSR},
#endif
#if defined(TIOCSERGETMULTI)
{"TIOCSERGETMULTI", TIOCSERGETMULTI},
#endif
#if defined(TIOCSERGSTRUCT)
{"TIOCSERGSTRUCT", TIOCSERGSTRUCT},
#endif
#if defined(TIOCSERGWILD)
{"TIOCSERGWILD", TIOCSERGWILD},
#endif
#if defined(TIOCSERSETMULTI)
{"TIOCSERSETMULTI", TIOCSERSETMULTI},
#endif
#if defined(TIOCSERSWILD)
{"TIOCSERSWILD", TIOCSERSWILD},
#endif
#if defined(TIOCSER_TEMT)
{"TIOCSER_TEMT", TIOCSER_TEMT},
#endif
#if defined(TIOCSETD)
{"TIOCSETD", TIOCSETD},
#endif
#if defined(TIOCSLCKTRMIOS)
{"TIOCSLCKTRMIOS", TIOCSLCKTRMIOS},
#endif
#if defined(TIOCSPGRP)
{"TIOCSPGRP", TIOCSPGRP},
#endif
#if defined(TIOCSSERIAL)
{"TIOCSSERIAL", TIOCSSERIAL},
#endif
#if defined(TIOCSSOFTCAR)
{"TIOCSSOFTCAR", TIOCSSOFTCAR},
#endif
#if defined(TIOCSTI)
{"TIOCSTI", TIOCSTI},
#endif
#if defined(TIOCSWINSZ)
{"TIOCSWINSZ", TIOCSWINSZ},
#endif
#if defined(TIOCTTYGSTRUCT)
{"TIOCTTYGSTRUCT", TIOCTTYGSTRUCT},
#endif
{NULL, 0}
};
PyMODINIT_FUNC
PyInit_termios(void) {
PyObject *m;
struct constant *constant = termios_constants;
m = Py_InitModule4("termios", termios_methods, termios__doc__,
(PyObject *)NULL, PYTHON_API_VERSION);
if (m == NULL)
return;
if (TermiosError == NULL) {
TermiosError = PyErr_NewException("termios.error", NULL, NULL);
}
Py_INCREF(TermiosError);
PyModule_AddObject(m, "error", TermiosError);
while (constant->name != NULL) {
PyModule_AddIntConstant(m, constant->name, constant->value);
++constant;
}
}
