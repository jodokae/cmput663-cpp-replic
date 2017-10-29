#include <gl.h>
#include <device.h>
#if defined(__sgi)
extern int devport();
extern int textwritemask();
extern int pagewritemask();
extern int gewrite();
extern int gettp();
#endif
#include "Python.h"
#include "cgensupport.h"
static PyObject *
gl_qread(PyObject *self, PyObject *args) {
long retval;
short arg1 ;
Py_BEGIN_ALLOW_THREADS
retval = qread( & arg1 );
Py_END_ALLOW_THREADS {
PyObject *v = PyTuple_New( 2 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewlongobject(retval));
PyTuple_SetItem(v, 1, mknewshortobject(arg1));
return v;
}
}
static PyObject *
gl_varray(PyObject *self, PyObject *args) {
PyObject *v, *w=NULL;
int i, n, width;
double vec[3];
PyObject * (*getitem)(PyObject *, int);
if (!PyArg_GetObject(args, 1, 0, &v))
return NULL;
if (PyList_Check(v)) {
n = PyList_Size(v);
getitem = PyList_GetItem;
} else if (PyTuple_Check(v)) {
n = PyTuple_Size(v);
getitem = PyTuple_GetItem;
} else {
PyErr_BadArgument();
return NULL;
}
if (n == 0) {
Py_INCREF(Py_None);
return Py_None;
}
if (n > 0)
w = (*getitem)(v, 0);
width = 0;
if (w == NULL) {
} else if (PyList_Check(w)) {
width = PyList_Size(w);
} else if (PyTuple_Check(w)) {
width = PyTuple_Size(w);
}
switch (width) {
case 2:
vec[2] = 0.0;
case 3:
break;
default:
PyErr_BadArgument();
return NULL;
}
for (i = 0; i < n; i++) {
w = (*getitem)(v, i);
if (!PyArg_GetDoubleArray(w, 1, 0, width, vec))
return NULL;
v3d(vec);
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *gen_nvarray();
static PyObject *
gl_nvarray(PyObject *self, PyObject *args) {
return gen_nvarray(args, 0);
}
static PyObject *
gl_vnarray(PyObject *self, PyObject *args) {
return gen_nvarray(args, 1);
}
static PyObject *
gen_nvarray(PyObject *args, int inorm) {
PyObject *v, *w, *wnorm, *wvec;
int i, n;
float norm[3], vec[3];
PyObject * (*getitem)(PyObject *, int);
if (!PyArg_GetObject(args, 1, 0, &v))
return NULL;
if (PyList_Check(v)) {
n = PyList_Size(v);
getitem = PyList_GetItem;
} else if (PyTuple_Check(v)) {
n = PyTuple_Size(v);
getitem = PyTuple_GetItem;
} else {
PyErr_BadArgument();
return NULL;
}
for (i = 0; i < n; i++) {
w = (*getitem)(v, i);
if (!PyTuple_Check(w) || PyTuple_Size(w) != 2) {
PyErr_BadArgument();
return NULL;
}
wnorm = PyTuple_GetItem(w, inorm);
wvec = PyTuple_GetItem(w, 1 - inorm);
if (!PyArg_GetFloatArray(wnorm, 1, 0, 3, norm) ||
!PyArg_GetFloatArray(wvec, 1, 0, 3, vec))
return NULL;
n3f(norm);
v3f(vec);
}
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_nurbssurface(PyObject *self, PyObject *args) {
long arg1 ;
double * arg2 ;
long arg3 ;
double * arg4 ;
double *arg5 ;
long arg6 ;
long arg7 ;
long arg8 ;
long ncoords;
long s_byte_stride, t_byte_stride;
long s_nctl, t_nctl;
long s, t;
PyObject *v, *w, *pt;
double *pnext;
if (!PyArg_GetLongArraySize(args, 6, 0, &arg1))
return NULL;
if ((arg2 = PyMem_NEW(double, arg1 )) == NULL) {
return PyErr_NoMemory();
}
if (!PyArg_GetDoubleArray(args, 6, 0, arg1 , arg2))
return NULL;
if (!PyArg_GetLongArraySize(args, 6, 1, &arg3))
return NULL;
if ((arg4 = PyMem_NEW(double, arg3 )) == NULL) {
return PyErr_NoMemory();
}
if (!PyArg_GetDoubleArray(args, 6, 1, arg3 , arg4))
return NULL;
if (!PyArg_GetLong(args, 6, 3, &arg6))
return NULL;
if (!PyArg_GetLong(args, 6, 4, &arg7))
return NULL;
if (!PyArg_GetLong(args, 6, 5, &arg8))
return NULL;
if (arg8 == N_XYZ)
ncoords = 3;
else if (arg8 == N_XYZW)
ncoords = 4;
else {
PyErr_BadArgument();
return NULL;
}
s_nctl = arg1 - arg6;
t_nctl = arg3 - arg7;
if (!PyArg_GetObject(args, 6, 2, &v))
return NULL;
if (!PyList_Check(v) || PyList_Size(v) != s_nctl) {
PyErr_BadArgument();
return NULL;
}
if ((arg5 = PyMem_NEW(double, s_nctl*t_nctl*ncoords )) == NULL) {
return PyErr_NoMemory();
}
pnext = arg5;
for (s = 0; s < s_nctl; s++) {
w = PyList_GetItem(v, s);
if (w == NULL || !PyList_Check(w) ||
PyList_Size(w) != t_nctl) {
PyErr_BadArgument();
return NULL;
}
for (t = 0; t < t_nctl; t++) {
pt = PyList_GetItem(w, t);
if (!PyArg_GetDoubleArray(pt, 1, 0, ncoords, pnext))
return NULL;
pnext += ncoords;
}
}
s_byte_stride = sizeof(double) * ncoords;
t_byte_stride = s_byte_stride * s_nctl;
nurbssurface( arg1 , arg2 , arg3 , arg4 ,
s_byte_stride , t_byte_stride , arg5 , arg6 , arg7 , arg8 );
PyMem_DEL(arg2);
PyMem_DEL(arg4);
PyMem_DEL(arg5);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_nurbscurve(PyObject *self, PyObject *args) {
long arg1 ;
double * arg2 ;
long arg3 ;
double * arg4 ;
long arg5 ;
long arg6 ;
int ncoords, npoints;
int i;
PyObject *v;
double *pnext;
if (!PyArg_GetLongArraySize(args, 4, 0, &arg1))
return NULL;
if ((arg2 = PyMem_NEW(double, arg1 )) == NULL) {
return PyErr_NoMemory();
}
if (!PyArg_GetDoubleArray(args, 4, 0, arg1 , arg2))
return NULL;
if (!PyArg_GetLong(args, 4, 2, &arg5))
return NULL;
if (!PyArg_GetLong(args, 4, 3, &arg6))
return NULL;
if (arg6 == N_ST)
ncoords = 2;
else if (arg6 == N_STW)
ncoords = 3;
else {
PyErr_BadArgument();
return NULL;
}
npoints = arg1 - arg5;
if (!PyArg_GetObject(args, 4, 1, &v))
return NULL;
if (!PyList_Check(v) || PyList_Size(v) != npoints) {
PyErr_BadArgument();
return NULL;
}
if ((arg4 = PyMem_NEW(double, npoints*ncoords )) == NULL) {
return PyErr_NoMemory();
}
pnext = arg4;
for (i = 0; i < npoints; i++) {
if (!PyArg_GetDoubleArray(PyList_GetItem(v, i), 1, 0, ncoords, pnext))
return NULL;
pnext += ncoords;
}
arg3 = (sizeof(double)) * ncoords;
nurbscurve( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
PyMem_DEL(arg2);
PyMem_DEL(arg4);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pwlcurve(PyObject *self, PyObject *args) {
PyObject *v;
long type;
double *data, *pnext;
long npoints, ncoords;
int i;
if (!PyArg_GetObject(args, 2, 0, &v))
return NULL;
if (!PyArg_GetLong(args, 2, 1, &type))
return NULL;
if (!PyList_Check(v)) {
PyErr_BadArgument();
return NULL;
}
npoints = PyList_Size(v);
if (type == N_ST)
ncoords = 2;
else {
PyErr_BadArgument();
return NULL;
}
if ((data = PyMem_NEW(double, npoints*ncoords)) == NULL) {
return PyErr_NoMemory();
}
pnext = data;
for (i = 0; i < npoints; i++) {
if (!PyArg_GetDoubleArray(PyList_GetItem(v, i), 1, 0, ncoords, pnext))
return NULL;
pnext += ncoords;
}
pwlcurve(npoints, data, sizeof(double)*ncoords, type);
PyMem_DEL(data);
Py_INCREF(Py_None);
return Py_None;
}
static short *pickbuffer = NULL;
static long pickbuffersize;
static PyObject *
pick_select(PyObject *args, void (*func)()) {
if (!PyArg_GetLong(args, 1, 0, &pickbuffersize))
return NULL;
if (pickbuffer != NULL) {
PyErr_SetString(PyExc_RuntimeError,
"pick/gselect: already picking/selecting");
return NULL;
}
if ((pickbuffer = PyMem_NEW(short, pickbuffersize)) == NULL) {
return PyErr_NoMemory();
}
(*func)(pickbuffer, pickbuffersize);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
endpick_select(long (*func)()) {
PyObject *v, *w;
int i, nhits, n;
if (pickbuffer == NULL) {
PyErr_SetString(PyExc_RuntimeError,
"endpick/endselect: not in pick/select mode");
return NULL;
}
nhits = (*func)(pickbuffer);
if (nhits < 0) {
nhits = -nhits;
}
n = 0;
for (; nhits > 0; nhits--) {
n += 1 + pickbuffer[n];
}
v = PyList_New(n);
if (v == NULL)
return NULL;
for (i = 0; i < n; i++) {
w = PyInt_FromLong((long)pickbuffer[i]);
if (w == NULL) {
Py_DECREF(v);
return NULL;
}
PyList_SetItem(v, i, w);
}
PyMem_DEL(pickbuffer);
pickbuffer = NULL;
return v;
}
extern void pick(), gselect();
extern long endpick(), endselect();
static PyObject *gl_pick(PyObject *self, PyObject *args) {
return pick_select(args, pick);
}
static PyObject *gl_endpick(PyObject *self) {
return endpick_select(endpick);
}
static PyObject *gl_gselect(PyObject *self, PyObject *args) {
return pick_select(args, gselect);
}
static PyObject *gl_endselect(PyObject *self) {
return endpick_select(endselect);
}
static PyObject *
gl_getmatrix(PyObject *self, PyObject *args) {
Matrix arg1;
PyObject *v, *w;
int i, j;
getmatrix( arg1 );
v = PyList_New(16);
if (v == NULL) {
return PyErr_NoMemory();
}
for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
w = mknewfloatobject(arg1[i][j]);
if (w == NULL) {
Py_DECREF(v);
return NULL;
}
PyList_SetItem(v, i*4+j, w);
}
return v;
}
static PyObject *
gl_altgetmatrix(PyObject *self, PyObject *args) {
Matrix arg1;
PyObject *v, *w;
int i, j;
getmatrix( arg1 );
v = PyList_New(4);
if (v == NULL) {
return NULL;
}
for (i = 0; i < 4; i++) {
w = PyList_New(4);
if (w == NULL) {
Py_DECREF(v);
return NULL;
}
PyList_SetItem(v, i, w);
}
for (i = 0; i < 4; i++) {
for (j = 0; j < 4; j++) {
w = mknewfloatobject(arg1[i][j]);
if (w == NULL) {
Py_DECREF(v);
return NULL;
}
PyList_SetItem(PyList_GetItem(v, i), j, w);
}
}
return v;
}
static PyObject *
gl_lrectwrite(PyObject *self, PyObject *args) {
short x1 ;
short y1 ;
short x2 ;
short y2 ;
string parray ;
PyObject *s;
#if 0
int pixcount;
#endif
if (!PyArg_GetShort(args, 5, 0, &x1))
return NULL;
if (!PyArg_GetShort(args, 5, 1, &y1))
return NULL;
if (!PyArg_GetShort(args, 5, 2, &x2))
return NULL;
if (!PyArg_GetShort(args, 5, 3, &y2))
return NULL;
if (!PyArg_GetString(args, 5, 4, &parray))
return NULL;
if (!PyArg_GetObject(args, 5, 4, &s))
return NULL;
#if 0
pixcount = (long)(x2+1-x1) * (long)(y2+1-y1);
if (!PyString_Check(s) || PyString_Size(s) != pixcount*sizeof(long)) {
PyErr_SetString(PyExc_RuntimeError,
"string arg to lrectwrite has wrong size");
return NULL;
}
#endif
lrectwrite( x1 , y1 , x2 , y2 , (unsigned long *) parray );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lrectread(PyObject *self, PyObject *args) {
short x1 ;
short y1 ;
short x2 ;
short y2 ;
PyObject *parray;
int pixcount;
if (!PyArg_GetShort(args, 4, 0, &x1))
return NULL;
if (!PyArg_GetShort(args, 4, 1, &y1))
return NULL;
if (!PyArg_GetShort(args, 4, 2, &x2))
return NULL;
if (!PyArg_GetShort(args, 4, 3, &y2))
return NULL;
pixcount = (long)(x2+1-x1) * (long)(y2+1-y1);
parray = PyString_FromStringAndSize((char *)NULL, pixcount*sizeof(long));
if (parray == NULL)
return NULL;
lrectread(x1, y1, x2, y2, (unsigned long *) PyString_AsString(parray));
return parray;
}
static PyObject *
gl_readdisplay(PyObject *self, PyObject *args) {
short x1, y1, x2, y2;
unsigned long *parray, hints;
long size, size_ret;
PyObject *rv;
if ( !PyArg_Parse(args, "hhhhl", &x1, &y1, &x2, &y2, &hints) )
return 0;
size = (long)(x2+1-x1) * (long)(y2+1-y1);
rv = PyString_FromStringAndSize((char *)NULL, size*sizeof(long));
if ( rv == NULL )
return NULL;
parray = (unsigned long *)PyString_AsString(rv);
size_ret = readdisplay(x1, y1, x2, y2, parray, hints);
if ( size_ret != size ) {
printf("gl_readdisplay: got %ld pixels, expected %ld\n",
size_ret, size);
PyErr_SetString(PyExc_RuntimeError, "readdisplay returned unexpected length");
return NULL;
}
return rv;
}
static PyObject *
gl_packrect(PyObject *self, PyObject *args) {
long width, height, packfactor;
char *s;
PyObject *unpacked, *packed;
int pixcount, packedcount, x, y, r, g, b;
unsigned long pixel;
unsigned char *p;
unsigned long *parray;
if (!PyArg_GetLong(args, 4, 0, &width))
return NULL;
if (!PyArg_GetLong(args, 4, 1, &height))
return NULL;
if (!PyArg_GetLong(args, 4, 2, &packfactor))
return NULL;
if (!PyArg_GetString(args, 4, 3, &s))
return NULL;
if (!PyArg_GetObject(args, 4, 3, &unpacked))
return NULL;
if (width <= 0 || height <= 0 || packfactor <= 0) {
PyErr_SetString(PyExc_RuntimeError, "packrect args must be > 0");
return NULL;
}
pixcount = width*height;
packedcount = ((width+packfactor-1)/packfactor) *
((height+packfactor-1)/packfactor);
if (PyString_Size(unpacked) != pixcount*sizeof(long)) {
PyErr_SetString(PyExc_RuntimeError,
"string arg to packrect has wrong size");
return NULL;
}
packed = PyString_FromStringAndSize((char *)NULL, packedcount);
if (packed == NULL)
return NULL;
parray = (unsigned long *) PyString_AsString(unpacked);
p = (unsigned char *) PyString_AsString(packed);
for (y = 0; y < height; y += packfactor, parray += packfactor*width) {
for (x = 0; x < width; x += packfactor) {
pixel = parray[x];
r = pixel & 0xff;
g = (pixel >> 8) & 0xff;
b = (pixel >> 16) & 0xff;
*p++ = (30*r+59*g+11*b) / 100;
}
}
return packed;
}
static unsigned long unpacktab[256];
static int unpacktab_inited = 0;
static PyObject *
gl_unpackrect(PyObject *self, PyObject *args) {
long width, height, packfactor;
char *s;
PyObject *unpacked, *packed;
int pixcount, packedcount;
register unsigned char *p;
register unsigned long *parray;
if (!unpacktab_inited) {
register int white;
for (white = 256; --white >= 0; )
unpacktab[white] = white * 0x010101L;
unpacktab_inited++;
}
if (!PyArg_GetLong(args, 4, 0, &width))
return NULL;
if (!PyArg_GetLong(args, 4, 1, &height))
return NULL;
if (!PyArg_GetLong(args, 4, 2, &packfactor))
return NULL;
if (!PyArg_GetString(args, 4, 3, &s))
return NULL;
if (!PyArg_GetObject(args, 4, 3, &packed))
return NULL;
if (width <= 0 || height <= 0 || packfactor <= 0) {
PyErr_SetString(PyExc_RuntimeError, "packrect args must be > 0");
return NULL;
}
pixcount = width*height;
packedcount = ((width+packfactor-1)/packfactor) *
((height+packfactor-1)/packfactor);
if (PyString_Size(packed) != packedcount) {
PyErr_SetString(PyExc_RuntimeError,
"string arg to unpackrect has wrong size");
return NULL;
}
unpacked = PyString_FromStringAndSize((char *)NULL, pixcount*sizeof(long));
if (unpacked == NULL)
return NULL;
parray = (unsigned long *) PyString_AsString(unpacked);
p = (unsigned char *) PyString_AsString(packed);
if (packfactor == 1 && width*height > 0) {
register int x = width * height;
do {
*parray++ = unpacktab[*p++];
} while (--x >= 0);
} else {
register int y;
for (y = 0; y < height-packfactor+1;
y += packfactor, parray += packfactor*width) {
register int x;
for (x = 0; x < width-packfactor+1; x += packfactor) {
register unsigned long pixel = unpacktab[*p++];
register int i;
for (i = packfactor*width; (i-=width) >= 0;) {
register int j;
for (j = packfactor; --j >= 0; )
parray[i+x+j] = pixel;
}
}
}
}
return unpacked;
}
static PyObject *
gl_gversion(PyObject *self, PyObject *args) {
char buf[20];
gversion(buf);
return PyString_FromString(buf);
}
static PyObject *
gl_clear(PyObject *self, PyObject *args) {
__GLclear( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_getshade(PyObject *self, PyObject *args) {
long retval;
retval = getshade( );
return mknewlongobject(retval);
}
static PyObject *
gl_devport(PyObject *self, PyObject *args) {
short arg1 ;
long arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
devport( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rdr2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
rdr2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rectfs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
rectfs( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rects(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
rects( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rmv2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
rmv2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_noport(PyObject *self, PyObject *args) {
noport( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_popviewport(PyObject *self, PyObject *args) {
popviewport( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_clearhitcode(PyObject *self, PyObject *args) {
clearhitcode( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_closeobj(PyObject *self, PyObject *args) {
closeobj( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cursoff(PyObject *self, PyObject *args) {
cursoff( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_curson(PyObject *self, PyObject *args) {
curson( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_doublebuffer(PyObject *self, PyObject *args) {
doublebuffer( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_finish(PyObject *self, PyObject *args) {
finish( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_gconfig(PyObject *self, PyObject *args) {
gconfig( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_ginit(PyObject *self, PyObject *args) {
ginit( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_greset(PyObject *self, PyObject *args) {
greset( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_multimap(PyObject *self, PyObject *args) {
multimap( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_onemap(PyObject *self, PyObject *args) {
onemap( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_popattributes(PyObject *self, PyObject *args) {
popattributes( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_popmatrix(PyObject *self, PyObject *args) {
popmatrix( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pushattributes(PyObject *self, PyObject *args) {
pushattributes( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pushmatrix(PyObject *self, PyObject *args) {
pushmatrix( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pushviewport(PyObject *self, PyObject *args) {
pushviewport( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_qreset(PyObject *self, PyObject *args) {
qreset( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_RGBmode(PyObject *self, PyObject *args) {
RGBmode( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_singlebuffer(PyObject *self, PyObject *args) {
singlebuffer( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_swapbuffers(PyObject *self, PyObject *args) {
swapbuffers( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_gsync(PyObject *self, PyObject *args) {
gsync( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_gflush(PyObject *self, PyObject *args) {
gflush( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_tpon(PyObject *self, PyObject *args) {
tpon( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_tpoff(PyObject *self, PyObject *args) {
tpoff( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_clkon(PyObject *self, PyObject *args) {
clkon( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_clkoff(PyObject *self, PyObject *args) {
clkoff( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_ringbell(PyObject *self, PyObject *args) {
ringbell( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_gbegin(PyObject *self, PyObject *args) {
gbegin( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_textinit(PyObject *self, PyObject *args) {
textinit( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_initnames(PyObject *self, PyObject *args) {
initnames( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pclos(PyObject *self, PyObject *args) {
pclos( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_popname(PyObject *self, PyObject *args) {
popname( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_spclos(PyObject *self, PyObject *args) {
spclos( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_zclear(PyObject *self, PyObject *args) {
zclear( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_screenspace(PyObject *self, PyObject *args) {
screenspace( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_reshapeviewport(PyObject *self, PyObject *args) {
reshapeviewport( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_winpush(PyObject *self, PyObject *args) {
winpush( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_winpop(PyObject *self, PyObject *args) {
winpop( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_foreground(PyObject *self, PyObject *args) {
foreground( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endfullscrn(PyObject *self, PyObject *args) {
endfullscrn( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endpupmode(PyObject *self, PyObject *args) {
endpupmode( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_fullscrn(PyObject *self, PyObject *args) {
fullscrn( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pupmode(PyObject *self, PyObject *args) {
pupmode( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_winconstraints(PyObject *self, PyObject *args) {
winconstraints( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pagecolor(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
pagecolor( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_textcolor(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
textcolor( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_color(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
color( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_curveit(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
curveit( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_font(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
font( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_linewidth(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
linewidth( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setlinestyle(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
setlinestyle( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setmap(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
setmap( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_swapinterval(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
swapinterval( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_writemask(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
writemask( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_textwritemask(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
textwritemask( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_qdevice(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
qdevice( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_unqdevice(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
unqdevice( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_curvebasis(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
curvebasis( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_curveprecision(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
curveprecision( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_loadname(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
loadname( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_passthrough(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
passthrough( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pushname(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
pushname( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setmonitor(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
setmonitor( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setshade(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
setshade( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setpattern(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
setpattern( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pagewritemask(PyObject *self, PyObject *args) {
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
pagewritemask( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_callobj(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
callobj( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_delobj(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
delobj( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_editobj(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
editobj( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_makeobj(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
makeobj( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_maketag(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
maketag( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_chunksize(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
chunksize( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_compactify(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
compactify( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_deltag(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
deltag( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lsrepeat(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
lsrepeat( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_objinsert(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
objinsert( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_objreplace(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
objreplace( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_winclose(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
winclose( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_blanktime(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
blanktime( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_freepup(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
freepup( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_backbuffer(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
backbuffer( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_frontbuffer(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
frontbuffer( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lsbackup(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
lsbackup( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_resetls(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
resetls( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lampon(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
lampon( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lampoff(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
lampoff( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setbell(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
setbell( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_blankscreen(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
blankscreen( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_depthcue(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
depthcue( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_zbuffer(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
zbuffer( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_backface(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
backface( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cmov2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
cmov2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_draw2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
draw2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_move2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
move2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pnt2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
pnt2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_patchbasis(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
patchbasis( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_patchprecision(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
patchprecision( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pdr2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
pdr2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pmv2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
pmv2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpdr2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
rpdr2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpmv2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
rpmv2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpt2i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
xfpt2i( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_objdelete(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
objdelete( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_patchcurves(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
patchcurves( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_minsize(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
minsize( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_maxsize(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
maxsize( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_keepaspect(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
keepaspect( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_prefsize(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
prefsize( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_stepunit(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
stepunit( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_fudge(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
fudge( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_winmove(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
winmove( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_attachcursor(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
attachcursor( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_deflinestyle(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
deflinestyle( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_noise(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
noise( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_picksize(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
picksize( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_qenter(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
qenter( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setdepth(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
setdepth( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cmov2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
cmov2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_draw2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
draw2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_move2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
move2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pdr2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
pdr2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pmv2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
pmv2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pnt2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
pnt2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rdr2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
rdr2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rmv2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
rmv2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpdr2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
rpdr2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpmv2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
rpmv2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpt2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
xfpt2s( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cmov2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
cmov2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_draw2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
draw2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_move2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
move2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pnt2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
pnt2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pdr2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
pdr2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pmv2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
pmv2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rdr2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
rdr2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rmv2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
rmv2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpdr2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
rpdr2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpmv2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
rpmv2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpt2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
xfpt2( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_loadmatrix(PyObject *self, PyObject *args) {
float arg1 [ 4 ] [ 4 ] ;
if (!getifloatarray(args, 1, 0, 4 * 4 , (float *) arg1))
return NULL;
loadmatrix( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_multmatrix(PyObject *self, PyObject *args) {
float arg1 [ 4 ] [ 4 ] ;
if (!getifloatarray(args, 1, 0, 4 * 4 , (float *) arg1))
return NULL;
multmatrix( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_crv(PyObject *self, PyObject *args) {
float arg1 [ 4 ] [ 3 ] ;
if (!getifloatarray(args, 1, 0, 3 * 4 , (float *) arg1))
return NULL;
crv( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rcrv(PyObject *self, PyObject *args) {
float arg1 [ 4 ] [ 4 ] ;
if (!getifloatarray(args, 1, 0, 4 * 4 , (float *) arg1))
return NULL;
rcrv( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_addtopup(PyObject *self, PyObject *args) {
long arg1 ;
string arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getistringarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
addtopup( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_charstr(PyObject *self, PyObject *args) {
string arg1 ;
if (!getistringarg(args, 1, 0, &arg1))
return NULL;
charstr( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_getport(PyObject *self, PyObject *args) {
string arg1 ;
if (!getistringarg(args, 1, 0, &arg1))
return NULL;
getport( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_strwidth(PyObject *self, PyObject *args) {
long retval;
string arg1 ;
if (!getistringarg(args, 1, 0, &arg1))
return NULL;
retval = strwidth( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_winopen(PyObject *self, PyObject *args) {
long retval;
string arg1 ;
if (!getistringarg(args, 1, 0, &arg1))
return NULL;
retval = winopen( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_wintitle(PyObject *self, PyObject *args) {
string arg1 ;
if (!getistringarg(args, 1, 0, &arg1))
return NULL;
wintitle( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polf(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 3 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (float(*)[3]) PyMem_NEW(float , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 1, 0, 3 * arg1 , (float *) arg2))
return NULL;
polf( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polf2(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 2 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (float(*)[2]) PyMem_NEW(float , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 1, 0, 2 * arg1 , (float *) arg2))
return NULL;
polf2( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_poly(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 3 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (float(*)[3]) PyMem_NEW(float , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 1, 0, 3 * arg1 , (float *) arg2))
return NULL;
poly( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_poly2(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 2 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (float(*)[2]) PyMem_NEW(float , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 1, 0, 2 * arg1 , (float *) arg2))
return NULL;
poly2( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_crvn(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 3 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (float(*)[3]) PyMem_NEW(float , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 1, 0, 3 * arg1 , (float *) arg2))
return NULL;
crvn( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rcrvn(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 4 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 4;
if ((arg2 = (float(*)[4]) PyMem_NEW(float , 4 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 1, 0, 4 * arg1 , (float *) arg2))
return NULL;
rcrvn( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polf2i(PyObject *self, PyObject *args) {
long arg1 ;
long (* arg2) [ 2 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (long(*)[2]) PyMem_NEW(long , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getilongarray(args, 1, 0, 2 * arg1 , (long *) arg2))
return NULL;
polf2i( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polfi(PyObject *self, PyObject *args) {
long arg1 ;
long (* arg2) [ 3 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (long(*)[3]) PyMem_NEW(long , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getilongarray(args, 1, 0, 3 * arg1 , (long *) arg2))
return NULL;
polfi( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_poly2i(PyObject *self, PyObject *args) {
long arg1 ;
long (* arg2) [ 2 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (long(*)[2]) PyMem_NEW(long , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getilongarray(args, 1, 0, 2 * arg1 , (long *) arg2))
return NULL;
poly2i( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polyi(PyObject *self, PyObject *args) {
long arg1 ;
long (* arg2) [ 3 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (long(*)[3]) PyMem_NEW(long , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getilongarray(args, 1, 0, 3 * arg1 , (long *) arg2))
return NULL;
polyi( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polf2s(PyObject *self, PyObject *args) {
long arg1 ;
short (* arg2) [ 2 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (short(*)[2]) PyMem_NEW(short , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 1, 0, 2 * arg1 , (short *) arg2))
return NULL;
polf2s( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polfs(PyObject *self, PyObject *args) {
long arg1 ;
short (* arg2) [ 3 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (short(*)[3]) PyMem_NEW(short , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 1, 0, 3 * arg1 , (short *) arg2))
return NULL;
polfs( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polys(PyObject *self, PyObject *args) {
long arg1 ;
short (* arg2) [ 3 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (short(*)[3]) PyMem_NEW(short , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 1, 0, 3 * arg1 , (short *) arg2))
return NULL;
polys( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_poly2s(PyObject *self, PyObject *args) {
long arg1 ;
short (* arg2) [ 2 ] ;
if (!getilongarraysize(args, 1, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (short(*)[2]) PyMem_NEW(short , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 1, 0, 2 * arg1 , (short *) arg2))
return NULL;
poly2s( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_defcursor(PyObject *self, PyObject *args) {
short arg1 ;
unsigned short arg2 [ 128 ] ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarray(args, 2, 1, 128 , (short *) arg2))
return NULL;
defcursor( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_writepixels(PyObject *self, PyObject *args) {
short arg1 ;
unsigned short * arg2 ;
if (!getishortarraysize(args, 1, 0, &arg1))
return NULL;
if ((arg2 = PyMem_NEW(unsigned short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 1, 0, arg1 , (short *) arg2))
return NULL;
writepixels( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_defbasis(PyObject *self, PyObject *args) {
long arg1 ;
float arg2 [ 4 ] [ 4 ] ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarray(args, 2, 1, 4 * 4 , (float *) arg2))
return NULL;
defbasis( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_gewrite(PyObject *self, PyObject *args) {
short arg1 ;
short * arg2 ;
if (!getishortarraysize(args, 1, 0, &arg1))
return NULL;
if ((arg2 = PyMem_NEW(short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 1, 0, arg1 , arg2))
return NULL;
gewrite( arg1 , arg2 );
PyMem_DEL(arg2);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rotate(PyObject *self, PyObject *args) {
short arg1 ;
char arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getichararg(args, 2, 1, &arg2))
return NULL;
rotate( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rot(PyObject *self, PyObject *args) {
float arg1 ;
char arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getichararg(args, 2, 1, &arg2))
return NULL;
rot( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_circfi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
circfi( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_circi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
circi( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cmovi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
cmovi( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_drawi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
drawi( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_movei(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
movei( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pnti(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
pnti( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_newtag(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
newtag( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pdri(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
pdri( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pmvi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
pmvi( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rdri(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
rdri( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rmvi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
rmvi( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpdri(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
rpdri( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpmvi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
rpmvi( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpti(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
xfpti( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_circ(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
circ( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_circf(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
circf( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cmov(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
cmov( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_draw(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
draw( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_move(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
move( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pnt(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
pnt( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_scale(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
scale( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_translate(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
translate( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pdr(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
pdr( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pmv(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
pmv( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rdr(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
rdr( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rmv(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
rmv( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpdr(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
rpdr( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpmv(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
rpmv( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpt(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
if (!getifloatarg(args, 3, 0, &arg1))
return NULL;
if (!getifloatarg(args, 3, 1, &arg2))
return NULL;
if (!getifloatarg(args, 3, 2, &arg3))
return NULL;
xfpt( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_RGBcolor(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
RGBcolor( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_RGBwritemask(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
RGBwritemask( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setcursor(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
setcursor( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_tie(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
tie( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_circfs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
circfs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_circs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
circs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cmovs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
cmovs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_draws(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
draws( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_moves(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
moves( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pdrs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
pdrs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pmvs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
pmvs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pnts(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
pnts( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rdrs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
rdrs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rmvs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
rmvs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpdrs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
rpdrs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpmvs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
rpmvs( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpts(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
xfpts( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_curorigin(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
curorigin( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cyclemap(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
if (!getishortarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
cyclemap( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_patch(PyObject *self, PyObject *args) {
float arg1 [ 4 ] [ 4 ] ;
float arg2 [ 4 ] [ 4 ] ;
float arg3 [ 4 ] [ 4 ] ;
if (!getifloatarray(args, 3, 0, 4 * 4 , (float *) arg1))
return NULL;
if (!getifloatarray(args, 3, 1, 4 * 4 , (float *) arg2))
return NULL;
if (!getifloatarray(args, 3, 2, 4 * 4 , (float *) arg3))
return NULL;
patch( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_splf(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 3 ] ;
unsigned short * arg3 ;
if (!getilongarraysize(args, 2, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (float(*)[3]) PyMem_NEW(float , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 2, 0, 3 * arg1 , (float *) arg2))
return NULL;
if ((arg3 = PyMem_NEW(unsigned short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 1, arg1 , (short *) arg3))
return NULL;
splf( arg1 , arg2 , arg3 );
PyMem_DEL(arg2);
PyMem_DEL(arg3);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_splf2(PyObject *self, PyObject *args) {
long arg1 ;
float (* arg2) [ 2 ] ;
unsigned short * arg3 ;
if (!getilongarraysize(args, 2, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (float(*)[2]) PyMem_NEW(float , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 2, 0, 2 * arg1 , (float *) arg2))
return NULL;
if ((arg3 = PyMem_NEW(unsigned short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 1, arg1 , (short *) arg3))
return NULL;
splf2( arg1 , arg2 , arg3 );
PyMem_DEL(arg2);
PyMem_DEL(arg3);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_splfi(PyObject *self, PyObject *args) {
long arg1 ;
long (* arg2) [ 3 ] ;
unsigned short * arg3 ;
if (!getilongarraysize(args, 2, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (long(*)[3]) PyMem_NEW(long , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getilongarray(args, 2, 0, 3 * arg1 , (long *) arg2))
return NULL;
if ((arg3 = PyMem_NEW(unsigned short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 1, arg1 , (short *) arg3))
return NULL;
splfi( arg1 , arg2 , arg3 );
PyMem_DEL(arg2);
PyMem_DEL(arg3);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_splf2i(PyObject *self, PyObject *args) {
long arg1 ;
long (* arg2) [ 2 ] ;
unsigned short * arg3 ;
if (!getilongarraysize(args, 2, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (long(*)[2]) PyMem_NEW(long , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getilongarray(args, 2, 0, 2 * arg1 , (long *) arg2))
return NULL;
if ((arg3 = PyMem_NEW(unsigned short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 1, arg1 , (short *) arg3))
return NULL;
splf2i( arg1 , arg2 , arg3 );
PyMem_DEL(arg2);
PyMem_DEL(arg3);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_splfs(PyObject *self, PyObject *args) {
long arg1 ;
short (* arg2) [ 3 ] ;
unsigned short * arg3 ;
if (!getilongarraysize(args, 2, 0, &arg1))
return NULL;
arg1 = arg1 / 3;
if ((arg2 = (short(*)[3]) PyMem_NEW(short , 3 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 0, 3 * arg1 , (short *) arg2))
return NULL;
if ((arg3 = PyMem_NEW(unsigned short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 1, arg1 , (short *) arg3))
return NULL;
splfs( arg1 , arg2 , arg3 );
PyMem_DEL(arg2);
PyMem_DEL(arg3);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_splf2s(PyObject *self, PyObject *args) {
long arg1 ;
short (* arg2) [ 2 ] ;
unsigned short * arg3 ;
if (!getilongarraysize(args, 2, 0, &arg1))
return NULL;
arg1 = arg1 / 2;
if ((arg2 = (short(*)[2]) PyMem_NEW(short , 2 * arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 0, 2 * arg1 , (short *) arg2))
return NULL;
if ((arg3 = PyMem_NEW(unsigned short , arg1 )) == NULL)
return PyErr_NoMemory();
if (!getishortarray(args, 2, 1, arg1 , (short *) arg3))
return NULL;
splf2s( arg1 , arg2 , arg3 );
PyMem_DEL(arg2);
PyMem_DEL(arg3);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rpatch(PyObject *self, PyObject *args) {
float arg1 [ 4 ] [ 4 ] ;
float arg2 [ 4 ] [ 4 ] ;
float arg3 [ 4 ] [ 4 ] ;
float arg4 [ 4 ] [ 4 ] ;
if (!getifloatarray(args, 4, 0, 4 * 4 , (float *) arg1))
return NULL;
if (!getifloatarray(args, 4, 1, 4 * 4 , (float *) arg2))
return NULL;
if (!getifloatarray(args, 4, 2, 4 * 4 , (float *) arg3))
return NULL;
if (!getifloatarray(args, 4, 3, 4 * 4 , (float *) arg4))
return NULL;
rpatch( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_ortho2(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
if (!getifloatarg(args, 4, 0, &arg1))
return NULL;
if (!getifloatarg(args, 4, 1, &arg2))
return NULL;
if (!getifloatarg(args, 4, 2, &arg3))
return NULL;
if (!getifloatarg(args, 4, 3, &arg4))
return NULL;
ortho2( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rect(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
if (!getifloatarg(args, 4, 0, &arg1))
return NULL;
if (!getifloatarg(args, 4, 1, &arg2))
return NULL;
if (!getifloatarg(args, 4, 2, &arg3))
return NULL;
if (!getifloatarg(args, 4, 3, &arg4))
return NULL;
rect( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rectf(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
if (!getifloatarg(args, 4, 0, &arg1))
return NULL;
if (!getifloatarg(args, 4, 1, &arg2))
return NULL;
if (!getifloatarg(args, 4, 2, &arg3))
return NULL;
if (!getifloatarg(args, 4, 3, &arg4))
return NULL;
rectf( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpt4(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
if (!getifloatarg(args, 4, 0, &arg1))
return NULL;
if (!getifloatarg(args, 4, 1, &arg2))
return NULL;
if (!getifloatarg(args, 4, 2, &arg3))
return NULL;
if (!getifloatarg(args, 4, 3, &arg4))
return NULL;
xfpt4( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_textport(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
textport( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_mapcolor(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
mapcolor( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_scrmask(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
scrmask( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setvaluator(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
setvaluator( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_viewport(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
viewport( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_shaderange(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
shaderange( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpt4s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
xfpt4s( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rectfi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
long arg4 ;
if (!getilongarg(args, 4, 0, &arg1))
return NULL;
if (!getilongarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
rectfi( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_recti(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
long arg4 ;
if (!getilongarg(args, 4, 0, &arg1))
return NULL;
if (!getilongarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
recti( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_xfpt4i(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
long arg4 ;
if (!getilongarg(args, 4, 0, &arg1))
return NULL;
if (!getilongarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
xfpt4i( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_prefposition(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
long arg4 ;
if (!getilongarg(args, 4, 0, &arg1))
return NULL;
if (!getilongarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
prefposition( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_arc(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
short arg4 ;
short arg5 ;
if (!getifloatarg(args, 5, 0, &arg1))
return NULL;
if (!getifloatarg(args, 5, 1, &arg2))
return NULL;
if (!getifloatarg(args, 5, 2, &arg3))
return NULL;
if (!getishortarg(args, 5, 3, &arg4))
return NULL;
if (!getishortarg(args, 5, 4, &arg5))
return NULL;
arc( arg1 , arg2 , arg3 , arg4 , arg5 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_arcf(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
short arg4 ;
short arg5 ;
if (!getifloatarg(args, 5, 0, &arg1))
return NULL;
if (!getifloatarg(args, 5, 1, &arg2))
return NULL;
if (!getifloatarg(args, 5, 2, &arg3))
return NULL;
if (!getishortarg(args, 5, 3, &arg4))
return NULL;
if (!getishortarg(args, 5, 4, &arg5))
return NULL;
arcf( arg1 , arg2 , arg3 , arg4 , arg5 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_arcfi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
short arg4 ;
short arg5 ;
if (!getilongarg(args, 5, 0, &arg1))
return NULL;
if (!getilongarg(args, 5, 1, &arg2))
return NULL;
if (!getilongarg(args, 5, 2, &arg3))
return NULL;
if (!getishortarg(args, 5, 3, &arg4))
return NULL;
if (!getishortarg(args, 5, 4, &arg5))
return NULL;
arcfi( arg1 , arg2 , arg3 , arg4 , arg5 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_arci(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
short arg4 ;
short arg5 ;
if (!getilongarg(args, 5, 0, &arg1))
return NULL;
if (!getilongarg(args, 5, 1, &arg2))
return NULL;
if (!getilongarg(args, 5, 2, &arg3))
return NULL;
if (!getishortarg(args, 5, 3, &arg4))
return NULL;
if (!getishortarg(args, 5, 4, &arg5))
return NULL;
arci( arg1 , arg2 , arg3 , arg4 , arg5 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bbox2(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
float arg3 ;
float arg4 ;
float arg5 ;
float arg6 ;
if (!getishortarg(args, 6, 0, &arg1))
return NULL;
if (!getishortarg(args, 6, 1, &arg2))
return NULL;
if (!getifloatarg(args, 6, 2, &arg3))
return NULL;
if (!getifloatarg(args, 6, 3, &arg4))
return NULL;
if (!getifloatarg(args, 6, 4, &arg5))
return NULL;
if (!getifloatarg(args, 6, 5, &arg6))
return NULL;
bbox2( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bbox2i(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
long arg3 ;
long arg4 ;
long arg5 ;
long arg6 ;
if (!getishortarg(args, 6, 0, &arg1))
return NULL;
if (!getishortarg(args, 6, 1, &arg2))
return NULL;
if (!getilongarg(args, 6, 2, &arg3))
return NULL;
if (!getilongarg(args, 6, 3, &arg4))
return NULL;
if (!getilongarg(args, 6, 4, &arg5))
return NULL;
if (!getilongarg(args, 6, 5, &arg6))
return NULL;
bbox2i( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bbox2s(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
short arg6 ;
if (!getishortarg(args, 6, 0, &arg1))
return NULL;
if (!getishortarg(args, 6, 1, &arg2))
return NULL;
if (!getishortarg(args, 6, 2, &arg3))
return NULL;
if (!getishortarg(args, 6, 3, &arg4))
return NULL;
if (!getishortarg(args, 6, 4, &arg5))
return NULL;
if (!getishortarg(args, 6, 5, &arg6))
return NULL;
bbox2s( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_blink(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
if (!getishortarg(args, 5, 0, &arg1))
return NULL;
if (!getishortarg(args, 5, 1, &arg2))
return NULL;
if (!getishortarg(args, 5, 2, &arg3))
return NULL;
if (!getishortarg(args, 5, 3, &arg4))
return NULL;
if (!getishortarg(args, 5, 4, &arg5))
return NULL;
blink( arg1 , arg2 , arg3 , arg4 , arg5 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_ortho(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
float arg5 ;
float arg6 ;
if (!getifloatarg(args, 6, 0, &arg1))
return NULL;
if (!getifloatarg(args, 6, 1, &arg2))
return NULL;
if (!getifloatarg(args, 6, 2, &arg3))
return NULL;
if (!getifloatarg(args, 6, 3, &arg4))
return NULL;
if (!getifloatarg(args, 6, 4, &arg5))
return NULL;
if (!getifloatarg(args, 6, 5, &arg6))
return NULL;
ortho( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_window(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
float arg5 ;
float arg6 ;
if (!getifloatarg(args, 6, 0, &arg1))
return NULL;
if (!getifloatarg(args, 6, 1, &arg2))
return NULL;
if (!getifloatarg(args, 6, 2, &arg3))
return NULL;
if (!getifloatarg(args, 6, 3, &arg4))
return NULL;
if (!getifloatarg(args, 6, 4, &arg5))
return NULL;
if (!getifloatarg(args, 6, 5, &arg6))
return NULL;
window( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lookat(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
float arg5 ;
float arg6 ;
short arg7 ;
if (!getifloatarg(args, 7, 0, &arg1))
return NULL;
if (!getifloatarg(args, 7, 1, &arg2))
return NULL;
if (!getifloatarg(args, 7, 2, &arg3))
return NULL;
if (!getifloatarg(args, 7, 3, &arg4))
return NULL;
if (!getifloatarg(args, 7, 4, &arg5))
return NULL;
if (!getifloatarg(args, 7, 5, &arg6))
return NULL;
if (!getishortarg(args, 7, 6, &arg7))
return NULL;
lookat( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 , arg7 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_perspective(PyObject *self, PyObject *args) {
short arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getifloatarg(args, 4, 1, &arg2))
return NULL;
if (!getifloatarg(args, 4, 2, &arg3))
return NULL;
if (!getifloatarg(args, 4, 3, &arg4))
return NULL;
perspective( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_polarview(PyObject *self, PyObject *args) {
float arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getifloatarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
polarview( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_arcfs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
if (!getishortarg(args, 5, 0, &arg1))
return NULL;
if (!getishortarg(args, 5, 1, &arg2))
return NULL;
if (!getishortarg(args, 5, 2, &arg3))
return NULL;
if (!getishortarg(args, 5, 3, &arg4))
return NULL;
if (!getishortarg(args, 5, 4, &arg5))
return NULL;
arcfs( arg1 , arg2 , arg3 , arg4 , arg5 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_arcs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
if (!getishortarg(args, 5, 0, &arg1))
return NULL;
if (!getishortarg(args, 5, 1, &arg2))
return NULL;
if (!getishortarg(args, 5, 2, &arg3))
return NULL;
if (!getishortarg(args, 5, 3, &arg4))
return NULL;
if (!getishortarg(args, 5, 4, &arg5))
return NULL;
arcs( arg1 , arg2 , arg3 , arg4 , arg5 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rectcopy(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
short arg6 ;
if (!getishortarg(args, 6, 0, &arg1))
return NULL;
if (!getishortarg(args, 6, 1, &arg2))
return NULL;
if (!getishortarg(args, 6, 2, &arg3))
return NULL;
if (!getishortarg(args, 6, 3, &arg4))
return NULL;
if (!getishortarg(args, 6, 4, &arg5))
return NULL;
if (!getishortarg(args, 6, 5, &arg6))
return NULL;
rectcopy( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_RGBcursor(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
short arg6 ;
short arg7 ;
if (!getishortarg(args, 7, 0, &arg1))
return NULL;
if (!getishortarg(args, 7, 1, &arg2))
return NULL;
if (!getishortarg(args, 7, 2, &arg3))
return NULL;
if (!getishortarg(args, 7, 3, &arg4))
return NULL;
if (!getishortarg(args, 7, 4, &arg5))
return NULL;
if (!getishortarg(args, 7, 5, &arg6))
return NULL;
if (!getishortarg(args, 7, 6, &arg7))
return NULL;
RGBcursor( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 , arg7 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_getbutton(PyObject *self, PyObject *args) {
long retval;
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
retval = getbutton( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_getcmmode(PyObject *self, PyObject *args) {
long retval;
retval = getcmmode( );
return mknewlongobject(retval);
}
static PyObject *
gl_getlsbackup(PyObject *self, PyObject *args) {
long retval;
retval = getlsbackup( );
return mknewlongobject(retval);
}
static PyObject *
gl_getresetls(PyObject *self, PyObject *args) {
long retval;
retval = getresetls( );
return mknewlongobject(retval);
}
static PyObject *
gl_getdcm(PyObject *self, PyObject *args) {
long retval;
retval = getdcm( );
return mknewlongobject(retval);
}
static PyObject *
gl_getzbuffer(PyObject *self, PyObject *args) {
long retval;
retval = getzbuffer( );
return mknewlongobject(retval);
}
static PyObject *
gl_ismex(PyObject *self, PyObject *args) {
long retval;
retval = ismex( );
return mknewlongobject(retval);
}
static PyObject *
gl_isobj(PyObject *self, PyObject *args) {
long retval;
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
retval = isobj( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_isqueued(PyObject *self, PyObject *args) {
long retval;
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
retval = isqueued( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_istag(PyObject *self, PyObject *args) {
long retval;
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
retval = istag( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_genobj(PyObject *self, PyObject *args) {
long retval;
retval = genobj( );
return mknewlongobject(retval);
}
static PyObject *
gl_gentag(PyObject *self, PyObject *args) {
long retval;
retval = gentag( );
return mknewlongobject(retval);
}
static PyObject *
gl_getbuffer(PyObject *self, PyObject *args) {
long retval;
retval = getbuffer( );
return mknewlongobject(retval);
}
static PyObject *
gl_getcolor(PyObject *self, PyObject *args) {
long retval;
retval = getcolor( );
return mknewlongobject(retval);
}
static PyObject *
gl_getdisplaymode(PyObject *self, PyObject *args) {
long retval;
retval = getdisplaymode( );
return mknewlongobject(retval);
}
static PyObject *
gl_getfont(PyObject *self, PyObject *args) {
long retval;
retval = getfont( );
return mknewlongobject(retval);
}
static PyObject *
gl_getheight(PyObject *self, PyObject *args) {
long retval;
retval = getheight( );
return mknewlongobject(retval);
}
static PyObject *
gl_gethitcode(PyObject *self, PyObject *args) {
long retval;
retval = gethitcode( );
return mknewlongobject(retval);
}
static PyObject *
gl_getlstyle(PyObject *self, PyObject *args) {
long retval;
retval = getlstyle( );
return mknewlongobject(retval);
}
static PyObject *
gl_getlwidth(PyObject *self, PyObject *args) {
long retval;
retval = getlwidth( );
return mknewlongobject(retval);
}
static PyObject *
gl_getmap(PyObject *self, PyObject *args) {
long retval;
retval = getmap( );
return mknewlongobject(retval);
}
static PyObject *
gl_getplanes(PyObject *self, PyObject *args) {
long retval;
retval = getplanes( );
return mknewlongobject(retval);
}
static PyObject *
gl_getwritemask(PyObject *self, PyObject *args) {
long retval;
retval = getwritemask( );
return mknewlongobject(retval);
}
static PyObject *
gl_qtest(PyObject *self, PyObject *args) {
long retval;
retval = qtest( );
return mknewlongobject(retval);
}
static PyObject *
gl_getlsrepeat(PyObject *self, PyObject *args) {
long retval;
retval = getlsrepeat( );
return mknewlongobject(retval);
}
static PyObject *
gl_getmonitor(PyObject *self, PyObject *args) {
long retval;
retval = getmonitor( );
return mknewlongobject(retval);
}
static PyObject *
gl_getopenobj(PyObject *self, PyObject *args) {
long retval;
retval = getopenobj( );
return mknewlongobject(retval);
}
static PyObject *
gl_getpattern(PyObject *self, PyObject *args) {
long retval;
retval = getpattern( );
return mknewlongobject(retval);
}
static PyObject *
gl_winget(PyObject *self, PyObject *args) {
long retval;
retval = winget( );
return mknewlongobject(retval);
}
static PyObject *
gl_winattach(PyObject *self, PyObject *args) {
long retval;
retval = winattach( );
return mknewlongobject(retval);
}
static PyObject *
gl_getothermonitor(PyObject *self, PyObject *args) {
long retval;
retval = getothermonitor( );
return mknewlongobject(retval);
}
static PyObject *
gl_newpup(PyObject *self, PyObject *args) {
long retval;
retval = newpup( );
return mknewlongobject(retval);
}
static PyObject *
gl_getvaluator(PyObject *self, PyObject *args) {
long retval;
short arg1 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
retval = getvaluator( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_winset(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
winset( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_dopup(PyObject *self, PyObject *args) {
long retval;
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
retval = dopup( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_getdepth(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
getdepth( & arg1 , & arg2 );
{
PyObject *v = PyTuple_New( 2 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject(arg2));
return v;
}
}
static PyObject *
gl_getcpos(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
getcpos( & arg1 , & arg2 );
{
PyObject *v = PyTuple_New( 2 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject(arg2));
return v;
}
}
static PyObject *
gl_getsize(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
getsize( & arg1 , & arg2 );
{
PyObject *v = PyTuple_New( 2 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewlongobject(arg1));
PyTuple_SetItem(v, 1, mknewlongobject(arg2));
return v;
}
}
static PyObject *
gl_getorigin(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
getorigin( & arg1 , & arg2 );
{
PyObject *v = PyTuple_New( 2 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewlongobject(arg1));
PyTuple_SetItem(v, 1, mknewlongobject(arg2));
return v;
}
}
static PyObject *
gl_getviewport(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
getviewport( & arg1 , & arg2 , & arg3 , & arg4 );
{
PyObject *v = PyTuple_New( 4 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject(arg2));
PyTuple_SetItem(v, 2, mknewshortobject(arg3));
PyTuple_SetItem(v, 3, mknewshortobject(arg4));
return v;
}
}
static PyObject *
gl_gettp(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
gettp( & arg1 , & arg2 , & arg3 , & arg4 );
{
PyObject *v = PyTuple_New( 4 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject(arg2));
PyTuple_SetItem(v, 2, mknewshortobject(arg3));
PyTuple_SetItem(v, 3, mknewshortobject(arg4));
return v;
}
}
static PyObject *
gl_getgpos(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
getgpos( & arg1 , & arg2 , & arg3 , & arg4 );
{
PyObject *v = PyTuple_New( 4 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewfloatobject(arg1));
PyTuple_SetItem(v, 1, mknewfloatobject(arg2));
PyTuple_SetItem(v, 2, mknewfloatobject(arg3));
PyTuple_SetItem(v, 3, mknewfloatobject(arg4));
return v;
}
}
static PyObject *
gl_winposition(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
long arg4 ;
if (!getilongarg(args, 4, 0, &arg1))
return NULL;
if (!getilongarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
winposition( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_gRGBcolor(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
gRGBcolor( & arg1 , & arg2 , & arg3 );
{
PyObject *v = PyTuple_New( 3 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject(arg2));
PyTuple_SetItem(v, 2, mknewshortobject(arg3));
return v;
}
}
static PyObject *
gl_gRGBmask(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
gRGBmask( & arg1 , & arg2 , & arg3 );
{
PyObject *v = PyTuple_New( 3 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject(arg2));
PyTuple_SetItem(v, 2, mknewshortobject(arg3));
return v;
}
}
static PyObject *
gl_getscrmask(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
getscrmask( & arg1 , & arg2 , & arg3 , & arg4 );
{
PyObject *v = PyTuple_New( 4 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject(arg2));
PyTuple_SetItem(v, 2, mknewshortobject(arg3));
PyTuple_SetItem(v, 3, mknewshortobject(arg4));
return v;
}
}
static PyObject *
gl_getmcolor(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 1, 0, &arg1))
return NULL;
getmcolor( arg1 , & arg2 , & arg3 , & arg4 );
{
PyObject *v = PyTuple_New( 3 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg2));
PyTuple_SetItem(v, 1, mknewshortobject(arg3));
PyTuple_SetItem(v, 2, mknewshortobject(arg4));
return v;
}
}
static PyObject *
gl_mapw(PyObject *self, PyObject *args) {
long arg1 ;
short arg2 ;
short arg3 ;
float arg4 ;
float arg5 ;
float arg6 ;
float arg7 ;
float arg8 ;
float arg9 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
mapw( arg1 , arg2 , arg3 , & arg4 , & arg5 , & arg6 , & arg7 , & arg8 , & arg9 );
{
PyObject *v = PyTuple_New( 6 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewfloatobject(arg4));
PyTuple_SetItem(v, 1, mknewfloatobject(arg5));
PyTuple_SetItem(v, 2, mknewfloatobject(arg6));
PyTuple_SetItem(v, 3, mknewfloatobject(arg7));
PyTuple_SetItem(v, 4, mknewfloatobject(arg8));
PyTuple_SetItem(v, 5, mknewfloatobject(arg9));
return v;
}
}
static PyObject *
gl_mapw2(PyObject *self, PyObject *args) {
long arg1 ;
short arg2 ;
short arg3 ;
float arg4 ;
float arg5 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getishortarg(args, 3, 1, &arg2))
return NULL;
if (!getishortarg(args, 3, 2, &arg3))
return NULL;
mapw2( arg1 , arg2 , arg3 , & arg4 , & arg5 );
{
PyObject *v = PyTuple_New( 2 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewfloatobject(arg4));
PyTuple_SetItem(v, 1, mknewfloatobject(arg5));
return v;
}
}
static PyObject *
gl_getcursor(PyObject *self, PyObject *args) {
short arg1 ;
unsigned short arg2 ;
unsigned short arg3 ;
long arg4 ;
getcursor( & arg1 , & arg2 , & arg3 , & arg4 );
{
PyObject *v = PyTuple_New( 4 );
if (v == NULL) return NULL;
PyTuple_SetItem(v, 0, mknewshortobject(arg1));
PyTuple_SetItem(v, 1, mknewshortobject((short) arg2));
PyTuple_SetItem(v, 2, mknewshortobject((short) arg3));
PyTuple_SetItem(v, 3, mknewlongobject(arg4));
return v;
}
}
static PyObject *
gl_cmode(PyObject *self, PyObject *args) {
cmode( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_concave(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
concave( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_curstype(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
curstype( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_drawmode(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
drawmode( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_gammaramp(PyObject *self, PyObject *args) {
short arg1 [ 256 ] ;
short arg2 [ 256 ] ;
short arg3 [ 256 ] ;
if (!getishortarray(args, 3, 0, 256 , arg1))
return NULL;
if (!getishortarray(args, 3, 1, 256 , arg2))
return NULL;
if (!getishortarray(args, 3, 2, 256 , arg3))
return NULL;
gammaramp( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_getbackface(PyObject *self, PyObject *args) {
long retval;
retval = getbackface( );
return mknewlongobject(retval);
}
static PyObject *
gl_getdescender(PyObject *self, PyObject *args) {
long retval;
retval = getdescender( );
return mknewlongobject(retval);
}
static PyObject *
gl_getdrawmode(PyObject *self, PyObject *args) {
long retval;
retval = getdrawmode( );
return mknewlongobject(retval);
}
static PyObject *
gl_getmmode(PyObject *self, PyObject *args) {
long retval;
retval = getmmode( );
return mknewlongobject(retval);
}
static PyObject *
gl_getsm(PyObject *self, PyObject *args) {
long retval;
retval = getsm( );
return mknewlongobject(retval);
}
static PyObject *
gl_getvideo(PyObject *self, PyObject *args) {
long retval;
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
retval = getvideo( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_imakebackground(PyObject *self, PyObject *args) {
imakebackground( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lmbind(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
if (!getishortarg(args, 2, 0, &arg1))
return NULL;
if (!getishortarg(args, 2, 1, &arg2))
return NULL;
lmbind( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lmdef(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
float * arg4 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarraysize(args, 3, 2, &arg3))
return NULL;
if ((arg4 = PyMem_NEW(float , arg3 )) == NULL)
return PyErr_NoMemory();
if (!getifloatarray(args, 3, 2, arg3 , arg4))
return NULL;
lmdef( arg1 , arg2 , arg3 , arg4 );
PyMem_DEL(arg4);
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_mmode(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
mmode( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_normal(PyObject *self, PyObject *args) {
float arg1 [ 3 ] ;
if (!getifloatarray(args, 1, 0, 3 , arg1))
return NULL;
normal( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_overlay(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
overlay( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_RGBrange(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
short arg6 ;
short arg7 ;
short arg8 ;
if (!getishortarg(args, 8, 0, &arg1))
return NULL;
if (!getishortarg(args, 8, 1, &arg2))
return NULL;
if (!getishortarg(args, 8, 2, &arg3))
return NULL;
if (!getishortarg(args, 8, 3, &arg4))
return NULL;
if (!getishortarg(args, 8, 4, &arg5))
return NULL;
if (!getishortarg(args, 8, 5, &arg6))
return NULL;
if (!getishortarg(args, 8, 6, &arg7))
return NULL;
if (!getishortarg(args, 8, 7, &arg8))
return NULL;
RGBrange( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 , arg7 , arg8 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setvideo(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
setvideo( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_shademodel(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
shademodel( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_underlay(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
underlay( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bgnclosedline(PyObject *self, PyObject *args) {
bgnclosedline( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bgnline(PyObject *self, PyObject *args) {
bgnline( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bgnpoint(PyObject *self, PyObject *args) {
bgnpoint( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bgnpolygon(PyObject *self, PyObject *args) {
bgnpolygon( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bgnsurface(PyObject *self, PyObject *args) {
bgnsurface( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bgntmesh(PyObject *self, PyObject *args) {
bgntmesh( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_bgntrim(PyObject *self, PyObject *args) {
bgntrim( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endclosedline(PyObject *self, PyObject *args) {
endclosedline( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endline(PyObject *self, PyObject *args) {
endline( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endpoint(PyObject *self, PyObject *args) {
endpoint( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endpolygon(PyObject *self, PyObject *args) {
endpolygon( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endsurface(PyObject *self, PyObject *args) {
endsurface( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endtmesh(PyObject *self, PyObject *args) {
endtmesh( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_endtrim(PyObject *self, PyObject *args) {
endtrim( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_blendfunction(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
blendfunction( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_c3f(PyObject *self, PyObject *args) {
float arg1 [ 3 ] ;
if (!getifloatarray(args, 1, 0, 3 , arg1))
return NULL;
c3f( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_c3i(PyObject *self, PyObject *args) {
long arg1 [ 3 ] ;
if (!getilongarray(args, 1, 0, 3 , arg1))
return NULL;
c3i( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_c3s(PyObject *self, PyObject *args) {
short arg1 [ 3 ] ;
if (!getishortarray(args, 1, 0, 3 , arg1))
return NULL;
c3s( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_c4f(PyObject *self, PyObject *args) {
float arg1 [ 4 ] ;
if (!getifloatarray(args, 1, 0, 4 , arg1))
return NULL;
c4f( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_c4i(PyObject *self, PyObject *args) {
long arg1 [ 4 ] ;
if (!getilongarray(args, 1, 0, 4 , arg1))
return NULL;
c4i( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_c4s(PyObject *self, PyObject *args) {
short arg1 [ 4 ] ;
if (!getishortarray(args, 1, 0, 4 , arg1))
return NULL;
c4s( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_colorf(PyObject *self, PyObject *args) {
float arg1 ;
if (!getifloatarg(args, 1, 0, &arg1))
return NULL;
colorf( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_cpack(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
cpack( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_czclear(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
czclear( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_dglclose(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
dglclose( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_dglopen(PyObject *self, PyObject *args) {
long retval;
string arg1 ;
long arg2 ;
if (!getistringarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
retval = dglopen( arg1 , arg2 );
return mknewlongobject(retval);
}
static PyObject *
gl_getgdesc(PyObject *self, PyObject *args) {
long retval;
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
retval = getgdesc( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_getnurbsproperty(PyObject *self, PyObject *args) {
long arg1 ;
float arg2 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
getnurbsproperty( arg1 , & arg2 );
return mknewfloatobject(arg2);
}
static PyObject *
gl_glcompat(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
glcompat( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_iconsize(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
iconsize( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_icontitle(PyObject *self, PyObject *args) {
string arg1 ;
if (!getistringarg(args, 1, 0, &arg1))
return NULL;
icontitle( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lRGBrange(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
short arg5 ;
short arg6 ;
long arg7 ;
long arg8 ;
if (!getishortarg(args, 8, 0, &arg1))
return NULL;
if (!getishortarg(args, 8, 1, &arg2))
return NULL;
if (!getishortarg(args, 8, 2, &arg3))
return NULL;
if (!getishortarg(args, 8, 3, &arg4))
return NULL;
if (!getishortarg(args, 8, 4, &arg5))
return NULL;
if (!getishortarg(args, 8, 5, &arg6))
return NULL;
if (!getilongarg(args, 8, 6, &arg7))
return NULL;
if (!getilongarg(args, 8, 7, &arg8))
return NULL;
lRGBrange( arg1 , arg2 , arg3 , arg4 , arg5 , arg6 , arg7 , arg8 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_linesmooth(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
linesmooth( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lmcolor(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
lmcolor( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_logicop(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
logicop( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lsetdepth(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
lsetdepth( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_lshaderange(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
long arg3 ;
long arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
lshaderange( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_n3f(PyObject *self, PyObject *args) {
float arg1 [ 3 ] ;
if (!getifloatarray(args, 1, 0, 3 , arg1))
return NULL;
n3f( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_noborder(PyObject *self, PyObject *args) {
noborder( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pntsmooth(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
pntsmooth( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_readsource(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
readsource( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_rectzoom(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
if (!getifloatarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
rectzoom( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_sbox(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
if (!getifloatarg(args, 4, 0, &arg1))
return NULL;
if (!getifloatarg(args, 4, 1, &arg2))
return NULL;
if (!getifloatarg(args, 4, 2, &arg3))
return NULL;
if (!getifloatarg(args, 4, 3, &arg4))
return NULL;
sbox( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_sboxi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
long arg4 ;
if (!getilongarg(args, 4, 0, &arg1))
return NULL;
if (!getilongarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
sboxi( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_sboxs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
sboxs( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_sboxf(PyObject *self, PyObject *args) {
float arg1 ;
float arg2 ;
float arg3 ;
float arg4 ;
if (!getifloatarg(args, 4, 0, &arg1))
return NULL;
if (!getifloatarg(args, 4, 1, &arg2))
return NULL;
if (!getifloatarg(args, 4, 2, &arg3))
return NULL;
if (!getifloatarg(args, 4, 3, &arg4))
return NULL;
sboxf( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_sboxfi(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
long arg4 ;
if (!getilongarg(args, 4, 0, &arg1))
return NULL;
if (!getilongarg(args, 4, 1, &arg2))
return NULL;
if (!getilongarg(args, 4, 2, &arg3))
return NULL;
if (!getilongarg(args, 4, 3, &arg4))
return NULL;
sboxfi( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_sboxfs(PyObject *self, PyObject *args) {
short arg1 ;
short arg2 ;
short arg3 ;
short arg4 ;
if (!getishortarg(args, 4, 0, &arg1))
return NULL;
if (!getishortarg(args, 4, 1, &arg2))
return NULL;
if (!getishortarg(args, 4, 2, &arg3))
return NULL;
if (!getishortarg(args, 4, 3, &arg4))
return NULL;
sboxfs( arg1 , arg2 , arg3 , arg4 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setnurbsproperty(PyObject *self, PyObject *args) {
long arg1 ;
float arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getifloatarg(args, 2, 1, &arg2))
return NULL;
setnurbsproperty( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_setpup(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
long arg3 ;
if (!getilongarg(args, 3, 0, &arg1))
return NULL;
if (!getilongarg(args, 3, 1, &arg2))
return NULL;
if (!getilongarg(args, 3, 2, &arg3))
return NULL;
setpup( arg1 , arg2 , arg3 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_smoothline(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
smoothline( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_subpixel(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
subpixel( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_swaptmesh(PyObject *self, PyObject *args) {
swaptmesh( );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_swinopen(PyObject *self, PyObject *args) {
long retval;
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
retval = swinopen( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_v2f(PyObject *self, PyObject *args) {
float arg1 [ 2 ] ;
if (!getifloatarray(args, 1, 0, 2 , arg1))
return NULL;
v2f( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v2i(PyObject *self, PyObject *args) {
long arg1 [ 2 ] ;
if (!getilongarray(args, 1, 0, 2 , arg1))
return NULL;
v2i( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v2s(PyObject *self, PyObject *args) {
short arg1 [ 2 ] ;
if (!getishortarray(args, 1, 0, 2 , arg1))
return NULL;
v2s( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v3f(PyObject *self, PyObject *args) {
float arg1 [ 3 ] ;
if (!getifloatarray(args, 1, 0, 3 , arg1))
return NULL;
v3f( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v3i(PyObject *self, PyObject *args) {
long arg1 [ 3 ] ;
if (!getilongarray(args, 1, 0, 3 , arg1))
return NULL;
v3i( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v3s(PyObject *self, PyObject *args) {
short arg1 [ 3 ] ;
if (!getishortarray(args, 1, 0, 3 , arg1))
return NULL;
v3s( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v4f(PyObject *self, PyObject *args) {
float arg1 [ 4 ] ;
if (!getifloatarray(args, 1, 0, 4 , arg1))
return NULL;
v4f( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v4i(PyObject *self, PyObject *args) {
long arg1 [ 4 ] ;
if (!getilongarray(args, 1, 0, 4 , arg1))
return NULL;
v4i( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v4s(PyObject *self, PyObject *args) {
short arg1 [ 4 ] ;
if (!getishortarray(args, 1, 0, 4 , arg1))
return NULL;
v4s( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_videocmd(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
videocmd( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_windepth(PyObject *self, PyObject *args) {
long retval;
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
retval = windepth( arg1 );
return mknewlongobject(retval);
}
static PyObject *
gl_wmpack(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
wmpack( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_zdraw(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
zdraw( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_zfunction(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
zfunction( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_zsource(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
zsource( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_zwritemask(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
zwritemask( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v2d(PyObject *self, PyObject *args) {
double arg1 [ 2 ] ;
if (!getidoublearray(args, 1, 0, 2 , arg1))
return NULL;
v2d( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v3d(PyObject *self, PyObject *args) {
double arg1 [ 3 ] ;
if (!getidoublearray(args, 1, 0, 3 , arg1))
return NULL;
v3d( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_v4d(PyObject *self, PyObject *args) {
double arg1 [ 4 ] ;
if (!getidoublearray(args, 1, 0, 4 , arg1))
return NULL;
v4d( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_pixmode(PyObject *self, PyObject *args) {
long arg1 ;
long arg2 ;
if (!getilongarg(args, 2, 0, &arg1))
return NULL;
if (!getilongarg(args, 2, 1, &arg2))
return NULL;
pixmode( arg1 , arg2 );
Py_INCREF(Py_None);
return Py_None;
}
static PyObject *
gl_qgetfd(PyObject *self, PyObject *args) {
long retval;
retval = qgetfd( );
return mknewlongobject(retval);
}
static PyObject *
gl_dither(PyObject *self, PyObject *args) {
long arg1 ;
if (!getilongarg(args, 1, 0, &arg1))
return NULL;
dither( arg1 );
Py_INCREF(Py_None);
return Py_None;
}
static struct PyMethodDef gl_methods[] = {
{"qread", gl_qread, METH_OLDARGS},
{"varray", gl_varray, METH_OLDARGS},
{"nvarray", gl_nvarray, METH_OLDARGS},
{"vnarray", gl_vnarray, METH_OLDARGS},
{"nurbssurface", gl_nurbssurface, METH_OLDARGS},
{"nurbscurve", gl_nurbscurve, METH_OLDARGS},
{"pwlcurve", gl_pwlcurve, METH_OLDARGS},
{"pick", gl_pick, METH_OLDARGS},
{"endpick", gl_endpick, METH_NOARGS},
{"gselect", gl_gselect, METH_OLDARGS},
{"endselect", gl_endselect, METH_NOARGS},
{"getmatrix", gl_getmatrix, METH_OLDARGS},
{"altgetmatrix", gl_altgetmatrix, METH_OLDARGS},
{"lrectwrite", gl_lrectwrite, METH_OLDARGS},
{"lrectread", gl_lrectread, METH_OLDARGS},
{"readdisplay", gl_readdisplay, METH_OLDARGS},
{"packrect", gl_packrect, METH_OLDARGS},
{"unpackrect", gl_unpackrect, METH_OLDARGS},
{"gversion", gl_gversion, METH_OLDARGS},
{"clear", gl_clear, METH_OLDARGS},
{"getshade", gl_getshade, METH_OLDARGS},
{"devport", gl_devport, METH_OLDARGS},
{"rdr2i", gl_rdr2i, METH_OLDARGS},
{"rectfs", gl_rectfs, METH_OLDARGS},
{"rects", gl_rects, METH_OLDARGS},
{"rmv2i", gl_rmv2i, METH_OLDARGS},
{"noport", gl_noport, METH_OLDARGS},
{"popviewport", gl_popviewport, METH_OLDARGS},
{"clearhitcode", gl_clearhitcode, METH_OLDARGS},
{"closeobj", gl_closeobj, METH_OLDARGS},
{"cursoff", gl_cursoff, METH_OLDARGS},
{"curson", gl_curson, METH_OLDARGS},
{"doublebuffer", gl_doublebuffer, METH_OLDARGS},
{"finish", gl_finish, METH_OLDARGS},
{"gconfig", gl_gconfig, METH_OLDARGS},
{"ginit", gl_ginit, METH_OLDARGS},
{"greset", gl_greset, METH_OLDARGS},
{"multimap", gl_multimap, METH_OLDARGS},
{"onemap", gl_onemap, METH_OLDARGS},
{"popattributes", gl_popattributes, METH_OLDARGS},
{"popmatrix", gl_popmatrix, METH_OLDARGS},
{"pushattributes", gl_pushattributes,METH_OLDARGS},
{"pushmatrix", gl_pushmatrix, METH_OLDARGS},
{"pushviewport", gl_pushviewport, METH_OLDARGS},
{"qreset", gl_qreset, METH_OLDARGS},
{"RGBmode", gl_RGBmode, METH_OLDARGS},
{"singlebuffer", gl_singlebuffer, METH_OLDARGS},
{"swapbuffers", gl_swapbuffers, METH_OLDARGS},
{"gsync", gl_gsync, METH_OLDARGS},
{"gflush", gl_gflush, METH_OLDARGS},
{"tpon", gl_tpon, METH_OLDARGS},
{"tpoff", gl_tpoff, METH_OLDARGS},
{"clkon", gl_clkon, METH_OLDARGS},
{"clkoff", gl_clkoff, METH_OLDARGS},
{"ringbell", gl_ringbell, METH_OLDARGS},
{"gbegin", gl_gbegin, METH_OLDARGS},
{"textinit", gl_textinit, METH_OLDARGS},
{"initnames", gl_initnames, METH_OLDARGS},
{"pclos", gl_pclos, METH_OLDARGS},
{"popname", gl_popname, METH_OLDARGS},
{"spclos", gl_spclos, METH_OLDARGS},
{"zclear", gl_zclear, METH_OLDARGS},
{"screenspace", gl_screenspace, METH_OLDARGS},
{"reshapeviewport", gl_reshapeviewport, METH_OLDARGS},
{"winpush", gl_winpush, METH_OLDARGS},
{"winpop", gl_winpop, METH_OLDARGS},
{"foreground", gl_foreground, METH_OLDARGS},
{"endfullscrn", gl_endfullscrn, METH_OLDARGS},
{"endpupmode", gl_endpupmode, METH_OLDARGS},
{"fullscrn", gl_fullscrn, METH_OLDARGS},
{"pupmode", gl_pupmode, METH_OLDARGS},
{"winconstraints", gl_winconstraints, METH_OLDARGS},
{"pagecolor", gl_pagecolor, METH_OLDARGS},
{"textcolor", gl_textcolor, METH_OLDARGS},
{"color", gl_color, METH_OLDARGS},
{"curveit", gl_curveit, METH_OLDARGS},
{"font", gl_font, METH_OLDARGS},
{"linewidth", gl_linewidth, METH_OLDARGS},
{"setlinestyle", gl_setlinestyle, METH_OLDARGS},
{"setmap", gl_setmap, METH_OLDARGS},
{"swapinterval", gl_swapinterval, METH_OLDARGS},
{"writemask", gl_writemask, METH_OLDARGS},
{"textwritemask", gl_textwritemask, METH_OLDARGS},
{"qdevice", gl_qdevice, METH_OLDARGS},
{"unqdevice", gl_unqdevice, METH_OLDARGS},
{"curvebasis", gl_curvebasis, METH_OLDARGS},
{"curveprecision", gl_curveprecision,METH_OLDARGS},
{"loadname", gl_loadname, METH_OLDARGS},
{"passthrough", gl_passthrough, METH_OLDARGS},
{"pushname", gl_pushname, METH_OLDARGS},
{"setmonitor", gl_setmonitor, METH_OLDARGS},
{"setshade", gl_setshade, METH_OLDARGS},
{"setpattern", gl_setpattern, METH_OLDARGS},
{"pagewritemask", gl_pagewritemask, METH_OLDARGS},
{"callobj", gl_callobj, METH_OLDARGS},
{"delobj", gl_delobj, METH_OLDARGS},
{"editobj", gl_editobj, METH_OLDARGS},
{"makeobj", gl_makeobj, METH_OLDARGS},
{"maketag", gl_maketag, METH_OLDARGS},
{"chunksize", gl_chunksize, METH_OLDARGS},
{"compactify", gl_compactify, METH_OLDARGS},
{"deltag", gl_deltag, METH_OLDARGS},
{"lsrepeat", gl_lsrepeat, METH_OLDARGS},
{"objinsert", gl_objinsert, METH_OLDARGS},
{"objreplace", gl_objreplace, METH_OLDARGS},
{"winclose", gl_winclose, METH_OLDARGS},
{"blanktime", gl_blanktime, METH_OLDARGS},
{"freepup", gl_freepup, METH_OLDARGS},
{"backbuffer", gl_backbuffer, METH_OLDARGS},
{"frontbuffer", gl_frontbuffer, METH_OLDARGS},
{"lsbackup", gl_lsbackup, METH_OLDARGS},
{"resetls", gl_resetls, METH_OLDARGS},
{"lampon", gl_lampon, METH_OLDARGS},
{"lampoff", gl_lampoff, METH_OLDARGS},
{"setbell", gl_setbell, METH_OLDARGS},
{"blankscreen", gl_blankscreen, METH_OLDARGS},
{"depthcue", gl_depthcue, METH_OLDARGS},
{"zbuffer", gl_zbuffer, METH_OLDARGS},
{"backface", gl_backface, METH_OLDARGS},
{"cmov2i", gl_cmov2i, METH_OLDARGS},
{"draw2i", gl_draw2i, METH_OLDARGS},
{"move2i", gl_move2i, METH_OLDARGS},
{"pnt2i", gl_pnt2i, METH_OLDARGS},
{"patchbasis", gl_patchbasis, METH_OLDARGS},
{"patchprecision", gl_patchprecision, METH_OLDARGS},
{"pdr2i", gl_pdr2i, METH_OLDARGS},
{"pmv2i", gl_pmv2i, METH_OLDARGS},
{"rpdr2i", gl_rpdr2i, METH_OLDARGS},
{"rpmv2i", gl_rpmv2i, METH_OLDARGS},
{"xfpt2i", gl_xfpt2i, METH_OLDARGS},
{"objdelete", gl_objdelete, METH_OLDARGS},
{"patchcurves", gl_patchcurves, METH_OLDARGS},
{"minsize", gl_minsize, METH_OLDARGS},
{"maxsize", gl_maxsize, METH_OLDARGS},
{"keepaspect", gl_keepaspect, METH_OLDARGS},
{"prefsize", gl_prefsize, METH_OLDARGS},
{"stepunit", gl_stepunit, METH_OLDARGS},
{"fudge", gl_fudge, METH_OLDARGS},
{"winmove", gl_winmove, METH_OLDARGS},
{"attachcursor", gl_attachcursor, METH_OLDARGS},
{"deflinestyle", gl_deflinestyle, METH_OLDARGS},
{"noise", gl_noise, METH_OLDARGS},
{"picksize", gl_picksize, METH_OLDARGS},
{"qenter", gl_qenter, METH_OLDARGS},
{"setdepth", gl_setdepth, METH_OLDARGS},
{"cmov2s", gl_cmov2s, METH_OLDARGS},
{"draw2s", gl_draw2s, METH_OLDARGS},
{"move2s", gl_move2s, METH_OLDARGS},
{"pdr2s", gl_pdr2s, METH_OLDARGS},
{"pmv2s", gl_pmv2s, METH_OLDARGS},
{"pnt2s", gl_pnt2s, METH_OLDARGS},
{"rdr2s", gl_rdr2s, METH_OLDARGS},
{"rmv2s", gl_rmv2s, METH_OLDARGS},
{"rpdr2s", gl_rpdr2s, METH_OLDARGS},
{"rpmv2s", gl_rpmv2s, METH_OLDARGS},
{"xfpt2s", gl_xfpt2s, METH_OLDARGS},
{"cmov2", gl_cmov2, METH_OLDARGS},
{"draw2", gl_draw2, METH_OLDARGS},
{"move2", gl_move2, METH_OLDARGS},
{"pnt2", gl_pnt2, METH_OLDARGS},
{"pdr2", gl_pdr2, METH_OLDARGS},
{"pmv2", gl_pmv2, METH_OLDARGS},
{"rdr2", gl_rdr2, METH_OLDARGS},
{"rmv2", gl_rmv2, METH_OLDARGS},
{"rpdr2", gl_rpdr2, METH_OLDARGS},
{"rpmv2", gl_rpmv2, METH_OLDARGS},
{"xfpt2", gl_xfpt2, METH_OLDARGS},
{"loadmatrix", gl_loadmatrix, METH_OLDARGS},
{"multmatrix", gl_multmatrix, METH_OLDARGS},
{"crv", gl_crv, METH_OLDARGS},
{"rcrv", gl_rcrv, METH_OLDARGS},
{"addtopup", gl_addtopup, METH_OLDARGS},
{"charstr", gl_charstr, METH_OLDARGS},
{"getport", gl_getport, METH_OLDARGS},
{"strwidth", gl_strwidth, METH_OLDARGS},
{"winopen", gl_winopen, METH_OLDARGS},
{"wintitle", gl_wintitle, METH_OLDARGS},
{"polf", gl_polf, METH_OLDARGS},
{"polf2", gl_polf2, METH_OLDARGS},
{"poly", gl_poly, METH_OLDARGS},
{"poly2", gl_poly2, METH_OLDARGS},
{"crvn", gl_crvn, METH_OLDARGS},
{"rcrvn", gl_rcrvn, METH_OLDARGS},
{"polf2i", gl_polf2i, METH_OLDARGS},
{"polfi", gl_polfi, METH_OLDARGS},
{"poly2i", gl_poly2i, METH_OLDARGS},
{"polyi", gl_polyi, METH_OLDARGS},
{"polf2s", gl_polf2s, METH_OLDARGS},
{"polfs", gl_polfs, METH_OLDARGS},
{"polys", gl_polys, METH_OLDARGS},
{"poly2s", gl_poly2s, METH_OLDARGS},
{"defcursor", gl_defcursor, METH_OLDARGS},
{"writepixels", gl_writepixels, METH_OLDARGS},
{"defbasis", gl_defbasis, METH_OLDARGS},
{"gewrite", gl_gewrite, METH_OLDARGS},
{"rotate", gl_rotate, METH_OLDARGS},
{"rot", gl_rot, METH_OLDARGS},
{"circfi", gl_circfi, METH_OLDARGS},
{"circi", gl_circi, METH_OLDARGS},
{"cmovi", gl_cmovi, METH_OLDARGS},
{"drawi", gl_drawi, METH_OLDARGS},
{"movei", gl_movei, METH_OLDARGS},
{"pnti", gl_pnti, METH_OLDARGS},
{"newtag", gl_newtag, METH_OLDARGS},
{"pdri", gl_pdri, METH_OLDARGS},
{"pmvi", gl_pmvi, METH_OLDARGS},
{"rdri", gl_rdri, METH_OLDARGS},
{"rmvi", gl_rmvi, METH_OLDARGS},
{"rpdri", gl_rpdri, METH_OLDARGS},
{"rpmvi", gl_rpmvi, METH_OLDARGS},
{"xfpti", gl_xfpti, METH_OLDARGS},
{"circ", gl_circ, METH_OLDARGS},
{"circf", gl_circf, METH_OLDARGS},
{"cmov", gl_cmov, METH_OLDARGS},
{"draw", gl_draw, METH_OLDARGS},
{"move", gl_move, METH_OLDARGS},
{"pnt", gl_pnt, METH_OLDARGS},
{"scale", gl_scale, METH_OLDARGS},
{"translate", gl_translate, METH_OLDARGS},
{"pdr", gl_pdr, METH_OLDARGS},
{"pmv", gl_pmv, METH_OLDARGS},
{"rdr", gl_rdr, METH_OLDARGS},
{"rmv", gl_rmv, METH_OLDARGS},
{"rpdr", gl_rpdr, METH_OLDARGS},
{"rpmv", gl_rpmv, METH_OLDARGS},
{"xfpt", gl_xfpt, METH_OLDARGS},
{"RGBcolor", gl_RGBcolor, METH_OLDARGS},
{"RGBwritemask", gl_RGBwritemask, METH_OLDARGS},
{"setcursor", gl_setcursor, METH_OLDARGS},
{"tie", gl_tie, METH_OLDARGS},
{"circfs", gl_circfs, METH_OLDARGS},
{"circs", gl_circs, METH_OLDARGS},
{"cmovs", gl_cmovs, METH_OLDARGS},
{"draws", gl_draws, METH_OLDARGS},
{"moves", gl_moves, METH_OLDARGS},
{"pdrs", gl_pdrs, METH_OLDARGS},
{"pmvs", gl_pmvs, METH_OLDARGS},
{"pnts", gl_pnts, METH_OLDARGS},
{"rdrs", gl_rdrs, METH_OLDARGS},
{"rmvs", gl_rmvs, METH_OLDARGS},
{"rpdrs", gl_rpdrs, METH_OLDARGS},
{"rpmvs", gl_rpmvs, METH_OLDARGS},
{"xfpts", gl_xfpts, METH_OLDARGS},
{"curorigin", gl_curorigin, METH_OLDARGS},
{"cyclemap", gl_cyclemap, METH_OLDARGS},
{"patch", gl_patch, METH_OLDARGS},
{"splf", gl_splf, METH_OLDARGS},
{"splf2", gl_splf2, METH_OLDARGS},
{"splfi", gl_splfi, METH_OLDARGS},
{"splf2i", gl_splf2i, METH_OLDARGS},
{"splfs", gl_splfs, METH_OLDARGS},
{"splf2s", gl_splf2s, METH_OLDARGS},
{"rpatch", gl_rpatch, METH_OLDARGS},
{"ortho2", gl_ortho2, METH_OLDARGS},
{"rect", gl_rect, METH_OLDARGS},
{"rectf", gl_rectf, METH_OLDARGS},
{"xfpt4", gl_xfpt4, METH_OLDARGS},
{"textport", gl_textport, METH_OLDARGS},
{"mapcolor", gl_mapcolor, METH_OLDARGS},
{"scrmask", gl_scrmask, METH_OLDARGS},
{"setvaluator", gl_setvaluator, METH_OLDARGS},
{"viewport", gl_viewport, METH_OLDARGS},
{"shaderange", gl_shaderange, METH_OLDARGS},
{"xfpt4s", gl_xfpt4s, METH_OLDARGS},
{"rectfi", gl_rectfi, METH_OLDARGS},
{"recti", gl_recti, METH_OLDARGS},
{"xfpt4i", gl_xfpt4i, METH_OLDARGS},
{"prefposition", gl_prefposition, METH_OLDARGS},
{"arc", gl_arc, METH_OLDARGS},
{"arcf", gl_arcf, METH_OLDARGS},
{"arcfi", gl_arcfi, METH_OLDARGS},
{"arci", gl_arci, METH_OLDARGS},
{"bbox2", gl_bbox2, METH_OLDARGS},
{"bbox2i", gl_bbox2i, METH_OLDARGS},
{"bbox2s", gl_bbox2s, METH_OLDARGS},
{"blink", gl_blink, METH_OLDARGS},
{"ortho", gl_ortho, METH_OLDARGS},
{"window", gl_window, METH_OLDARGS},
{"lookat", gl_lookat, METH_OLDARGS},
{"perspective", gl_perspective, METH_OLDARGS},
{"polarview", gl_polarview, METH_OLDARGS},
{"arcfs", gl_arcfs, METH_OLDARGS},
{"arcs", gl_arcs, METH_OLDARGS},
{"rectcopy", gl_rectcopy, METH_OLDARGS},
{"RGBcursor", gl_RGBcursor, METH_OLDARGS},
{"getbutton", gl_getbutton, METH_OLDARGS},
{"getcmmode", gl_getcmmode, METH_OLDARGS},
{"getlsbackup", gl_getlsbackup, METH_OLDARGS},
{"getresetls", gl_getresetls, METH_OLDARGS},
{"getdcm", gl_getdcm, METH_OLDARGS},
{"getzbuffer", gl_getzbuffer, METH_OLDARGS},
{"ismex", gl_ismex, METH_OLDARGS},
{"isobj", gl_isobj, METH_OLDARGS},
{"isqueued", gl_isqueued, METH_OLDARGS},
{"istag", gl_istag, METH_OLDARGS},
{"genobj", gl_genobj, METH_OLDARGS},
{"gentag", gl_gentag, METH_OLDARGS},
{"getbuffer", gl_getbuffer, METH_OLDARGS},
{"getcolor", gl_getcolor, METH_OLDARGS},
{"getdisplaymode", gl_getdisplaymode, METH_OLDARGS},
{"getfont", gl_getfont, METH_OLDARGS},
{"getheight", gl_getheight, METH_OLDARGS},
{"gethitcode", gl_gethitcode, METH_OLDARGS},
{"getlstyle", gl_getlstyle, METH_OLDARGS},
{"getlwidth", gl_getlwidth, METH_OLDARGS},
{"getmap", gl_getmap, METH_OLDARGS},
{"getplanes", gl_getplanes, METH_OLDARGS},
{"getwritemask", gl_getwritemask, METH_OLDARGS},
{"qtest", gl_qtest, METH_OLDARGS},
{"getlsrepeat", gl_getlsrepeat, METH_OLDARGS},
{"getmonitor", gl_getmonitor, METH_OLDARGS},
{"getopenobj", gl_getopenobj, METH_OLDARGS},
{"getpattern", gl_getpattern, METH_OLDARGS},
{"winget", gl_winget, METH_OLDARGS},
{"winattach", gl_winattach, METH_OLDARGS},
{"getothermonitor", gl_getothermonitor, METH_OLDARGS},
{"newpup", gl_newpup, METH_OLDARGS},
{"getvaluator", gl_getvaluator, METH_OLDARGS},
{"winset", gl_winset, METH_OLDARGS},
{"dopup", gl_dopup, METH_OLDARGS},
{"getdepth", gl_getdepth, METH_OLDARGS},
{"getcpos", gl_getcpos, METH_OLDARGS},
{"getsize", gl_getsize, METH_OLDARGS},
{"getorigin", gl_getorigin, METH_OLDARGS},
{"getviewport", gl_getviewport, METH_OLDARGS},
{"gettp", gl_gettp, METH_OLDARGS},
{"getgpos", gl_getgpos, METH_OLDARGS},
{"winposition", gl_winposition, METH_OLDARGS},
{"gRGBcolor", gl_gRGBcolor, METH_OLDARGS},
{"gRGBmask", gl_gRGBmask, METH_OLDARGS},
{"getscrmask", gl_getscrmask, METH_OLDARGS},
{"getmcolor", gl_getmcolor, METH_OLDARGS},
{"mapw", gl_mapw, METH_OLDARGS},
{"mapw2", gl_mapw2, METH_OLDARGS},
{"getcursor", gl_getcursor, METH_OLDARGS},
{"cmode", gl_cmode, METH_OLDARGS},
{"concave", gl_concave, METH_OLDARGS},
{"curstype", gl_curstype, METH_OLDARGS},
{"drawmode", gl_drawmode, METH_OLDARGS},
{"gammaramp", gl_gammaramp, METH_OLDARGS},
{"getbackface", gl_getbackface, METH_OLDARGS},
{"getdescender", gl_getdescender, METH_OLDARGS},
{"getdrawmode", gl_getdrawmode, METH_OLDARGS},
{"getmmode", gl_getmmode, METH_OLDARGS},
{"getsm", gl_getsm, METH_OLDARGS},
{"getvideo", gl_getvideo, METH_OLDARGS},
{"imakebackground", gl_imakebackground, METH_OLDARGS},
{"lmbind", gl_lmbind, METH_OLDARGS},
{"lmdef", gl_lmdef, METH_OLDARGS},
{"mmode", gl_mmode, METH_OLDARGS},
{"normal", gl_normal, METH_OLDARGS},
{"overlay", gl_overlay, METH_OLDARGS},
{"RGBrange", gl_RGBrange, METH_OLDARGS},
{"setvideo", gl_setvideo, METH_OLDARGS},
{"shademodel", gl_shademodel, METH_OLDARGS},
{"underlay", gl_underlay, METH_OLDARGS},
{"bgnclosedline", gl_bgnclosedline, METH_OLDARGS},
{"bgnline", gl_bgnline, METH_OLDARGS},
{"bgnpoint", gl_bgnpoint, METH_OLDARGS},
{"bgnpolygon", gl_bgnpolygon, METH_OLDARGS},
{"bgnsurface", gl_bgnsurface, METH_OLDARGS},
{"bgntmesh", gl_bgntmesh, METH_OLDARGS},
{"bgntrim", gl_bgntrim, METH_OLDARGS},
{"endclosedline", gl_endclosedline, METH_OLDARGS},
{"endline", gl_endline, METH_OLDARGS},
{"endpoint", gl_endpoint, METH_OLDARGS},
{"endpolygon", gl_endpolygon, METH_OLDARGS},
{"endsurface", gl_endsurface, METH_OLDARGS},
{"endtmesh", gl_endtmesh, METH_OLDARGS},
{"endtrim", gl_endtrim, METH_OLDARGS},
{"blendfunction", gl_blendfunction, METH_OLDARGS},
{"c3f", gl_c3f, METH_OLDARGS},
{"c3i", gl_c3i, METH_OLDARGS},
{"c3s", gl_c3s, METH_OLDARGS},
{"c4f", gl_c4f, METH_OLDARGS},
{"c4i", gl_c4i, METH_OLDARGS},
{"c4s", gl_c4s, METH_OLDARGS},
{"colorf", gl_colorf, METH_OLDARGS},
{"cpack", gl_cpack, METH_OLDARGS},
{"czclear", gl_czclear, METH_OLDARGS},
{"dglclose", gl_dglclose, METH_OLDARGS},
{"dglopen", gl_dglopen, METH_OLDARGS},
{"getgdesc", gl_getgdesc, METH_OLDARGS},
{"getnurbsproperty", gl_getnurbsproperty, METH_OLDARGS},
{"glcompat", gl_glcompat, METH_OLDARGS},
{"iconsize", gl_iconsize, METH_OLDARGS},
{"icontitle", gl_icontitle, METH_OLDARGS},
{"lRGBrange", gl_lRGBrange, METH_OLDARGS},
{"linesmooth", gl_linesmooth, METH_OLDARGS},
{"lmcolor", gl_lmcolor, METH_OLDARGS},
{"logicop", gl_logicop, METH_OLDARGS},
{"lsetdepth", gl_lsetdepth, METH_OLDARGS},
{"lshaderange", gl_lshaderange, METH_OLDARGS},
{"n3f", gl_n3f, METH_OLDARGS},
{"noborder", gl_noborder, METH_OLDARGS},
{"pntsmooth", gl_pntsmooth, METH_OLDARGS},
{"readsource", gl_readsource, METH_OLDARGS},
{"rectzoom", gl_rectzoom, METH_OLDARGS},
{"sbox", gl_sbox, METH_OLDARGS},
{"sboxi", gl_sboxi, METH_OLDARGS},
{"sboxs", gl_sboxs, METH_OLDARGS},
{"sboxf", gl_sboxf, METH_OLDARGS},
{"sboxfi", gl_sboxfi, METH_OLDARGS},
{"sboxfs", gl_sboxfs, METH_OLDARGS},
{"setnurbsproperty", gl_setnurbsproperty, METH_OLDARGS},
{"setpup", gl_setpup, METH_OLDARGS},
{"smoothline", gl_smoothline, METH_OLDARGS},
{"subpixel", gl_subpixel, METH_OLDARGS},
{"swaptmesh", gl_swaptmesh, METH_OLDARGS},
{"swinopen", gl_swinopen, METH_OLDARGS},
{"v2f", gl_v2f, METH_OLDARGS},
{"v2i", gl_v2i, METH_OLDARGS},
{"v2s", gl_v2s, METH_OLDARGS},
{"v3f", gl_v3f, METH_OLDARGS},
{"v3i", gl_v3i, METH_OLDARGS},
{"v3s", gl_v3s, METH_OLDARGS},
{"v4f", gl_v4f, METH_OLDARGS},
{"v4i", gl_v4i, METH_OLDARGS},
{"v4s", gl_v4s, METH_OLDARGS},
{"videocmd", gl_videocmd, METH_OLDARGS},
{"windepth", gl_windepth, METH_OLDARGS},
{"wmpack", gl_wmpack, METH_OLDARGS},
{"zdraw", gl_zdraw, METH_OLDARGS},
{"zfunction", gl_zfunction, METH_OLDARGS},
{"zsource", gl_zsource, METH_OLDARGS},
{"zwritemask", gl_zwritemask, METH_OLDARGS},
{"v2d", gl_v2d, METH_OLDARGS},
{"v3d", gl_v3d, METH_OLDARGS},
{"v4d", gl_v4d, METH_OLDARGS},
{"pixmode", gl_pixmode, METH_OLDARGS},
{"qgetfd", gl_qgetfd, METH_OLDARGS},
{"dither", gl_dither, METH_OLDARGS},
{NULL, NULL}
};
void
initgl(void) {
if (PyErr_WarnPy3k("the gl module has been removed in "
"Python 3.0", 2) < 0)
return;
(void) Py_InitModule("gl", gl_methods);
}