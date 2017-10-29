#include "Python.h"
#if !defined(__LP64__)
#include "pymactoolbox.h"
#define PyMac_PRECHECK(rtn) do { if ( &rtn == NULL ) {PyErr_SetString(PyExc_NotImplementedError, "Not available in this shared library/OS version"); return NULL; }} while(0)
#include <Carbon/Carbon.h>
static PyObject *TXNObj_New(TXNObject);
static int TXNObj_Convert(PyObject *, TXNObject *);
static PyObject *TXNFontMenuObj_New(TXNFontMenuObject);
static int TXNFontMenuObj_Convert(PyObject *, TXNFontMenuObject *);
#if defined(NOTYET_USE_TOOLBOX_OBJECT_GLUE)
#endif
static int
OptFSSpecPtr_Convert(PyObject *v, FSSpec **p_itself) {
static FSSpec fss;
if (v == Py_None) {
*p_itself = NULL;
return 1;
}
*p_itself = &fss;
return PyMac_GetFSSpec(v, *p_itself);
}
static int
OptGWorldObj_Convert(PyObject *v, GWorldPtr *p_itself) {
if (v == Py_None) {
*p_itself = NULL;
return 1;
}
return GWorldObj_Convert(v, p_itself);
}
static PyObject *Mlte_Error;
PyTypeObject TXNObject_Type;
#define TXNObj_Check(x) ((x)->ob_type == &TXNObject_Type || PyObject_TypeCheck((x), &TXNObject_Type))
typedef struct TXNObjectObject {
PyObject_HEAD
TXNObject ob_itself;
} TXNObjectObject;
PyObject *TXNObj_New(TXNObject itself) {
TXNObjectObject *it;
if (itself == NULL) return PyMac_Error(resNotFound);
it = PyObject_NEW(TXNObjectObject, &TXNObject_Type);
if (it == NULL) return NULL;
it->ob_itself = itself;
return (PyObject *)it;
}
int TXNObj_Convert(PyObject *v, TXNObject *p_itself) {
if (!TXNObj_Check(v)) {
PyErr_SetString(PyExc_TypeError, "TXNObject required");
return 0;
}
*p_itself = ((TXNObjectObject *)v)->ob_itself;
return 1;
}
static void TXNObj_dealloc(TXNObjectObject *self) {
self->ob_type->tp_free((PyObject *)self);
}
static PyObject *TXNObj_TXNDeleteObject(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNDeleteObject)
PyMac_PRECHECK(TXNDeleteObject);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNDeleteObject(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNResizeFrame(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
UInt32 iWidth;
UInt32 iHeight;
TXNFrameID iTXNFrameID;
#if !defined(TXNResizeFrame)
PyMac_PRECHECK(TXNResizeFrame);
#endif
if (!PyArg_ParseTuple(_args, "lll",
&iWidth,
&iHeight,
&iTXNFrameID))
return NULL;
TXNResizeFrame(_self->ob_itself,
iWidth,
iHeight,
iTXNFrameID);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNSetFrameBounds(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
SInt32 iTop;
SInt32 iLeft;
SInt32 iBottom;
SInt32 iRight;
TXNFrameID iTXNFrameID;
#if !defined(TXNSetFrameBounds)
PyMac_PRECHECK(TXNSetFrameBounds);
#endif
if (!PyArg_ParseTuple(_args, "lllll",
&iTop,
&iLeft,
&iBottom,
&iRight,
&iTXNFrameID))
return NULL;
TXNSetFrameBounds(_self->ob_itself,
iTop,
iLeft,
iBottom,
iRight,
iTXNFrameID);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNKeyDown(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
EventRecord iEvent;
#if !defined(TXNKeyDown)
PyMac_PRECHECK(TXNKeyDown);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetEventRecord, &iEvent))
return NULL;
TXNKeyDown(_self->ob_itself,
&iEvent);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNAdjustCursor(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
RgnHandle ioCursorRgn;
#if !defined(TXNAdjustCursor)
PyMac_PRECHECK(TXNAdjustCursor);
#endif
if (!PyArg_ParseTuple(_args, "O&",
OptResObj_Convert, &ioCursorRgn))
return NULL;
TXNAdjustCursor(_self->ob_itself,
ioCursorRgn);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNClick(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
EventRecord iEvent;
#if !defined(TXNClick)
PyMac_PRECHECK(TXNClick);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetEventRecord, &iEvent))
return NULL;
TXNClick(_self->ob_itself,
&iEvent);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNSelectAll(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNSelectAll)
PyMac_PRECHECK(TXNSelectAll);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNSelectAll(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNFocus(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean iBecomingFocused;
#if !defined(TXNFocus)
PyMac_PRECHECK(TXNFocus);
#endif
if (!PyArg_ParseTuple(_args, "b",
&iBecomingFocused))
return NULL;
TXNFocus(_self->ob_itself,
iBecomingFocused);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNUpdate(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNUpdate)
PyMac_PRECHECK(TXNUpdate);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNUpdate(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNDraw(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
GWorldPtr iDrawPort;
#if !defined(TXNDraw)
PyMac_PRECHECK(TXNDraw);
#endif
if (!PyArg_ParseTuple(_args, "O&",
OptGWorldObj_Convert, &iDrawPort))
return NULL;
TXNDraw(_self->ob_itself,
iDrawPort);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNForceUpdate(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNForceUpdate)
PyMac_PRECHECK(TXNForceUpdate);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNForceUpdate(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNGetSleepTicks(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
UInt32 _rv;
#if !defined(TXNGetSleepTicks)
PyMac_PRECHECK(TXNGetSleepTicks);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNGetSleepTicks(_self->ob_itself);
_res = Py_BuildValue("l",
_rv);
return _res;
}
static PyObject *TXNObj_TXNIdle(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNIdle)
PyMac_PRECHECK(TXNIdle);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNIdle(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNGrowWindow(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
EventRecord iEvent;
#if !defined(TXNGrowWindow)
PyMac_PRECHECK(TXNGrowWindow);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetEventRecord, &iEvent))
return NULL;
TXNGrowWindow(_self->ob_itself,
&iEvent);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNZoomWindow(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
SInt16 iPart;
#if !defined(TXNZoomWindow)
PyMac_PRECHECK(TXNZoomWindow);
#endif
if (!PyArg_ParseTuple(_args, "h",
&iPart))
return NULL;
TXNZoomWindow(_self->ob_itself,
iPart);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNCanUndo(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean _rv;
TXNActionKey oTXNActionKey;
#if !defined(TXNCanUndo)
PyMac_PRECHECK(TXNCanUndo);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNCanUndo(_self->ob_itself,
&oTXNActionKey);
_res = Py_BuildValue("bl",
_rv,
oTXNActionKey);
return _res;
}
static PyObject *TXNObj_TXNUndo(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNUndo)
PyMac_PRECHECK(TXNUndo);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNUndo(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNCanRedo(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean _rv;
TXNActionKey oTXNActionKey;
#if !defined(TXNCanRedo)
PyMac_PRECHECK(TXNCanRedo);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNCanRedo(_self->ob_itself,
&oTXNActionKey);
_res = Py_BuildValue("bl",
_rv,
oTXNActionKey);
return _res;
}
static PyObject *TXNObj_TXNRedo(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNRedo)
PyMac_PRECHECK(TXNRedo);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNRedo(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNCut(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNCut)
PyMac_PRECHECK(TXNCut);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNCut(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNCopy(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNCopy)
PyMac_PRECHECK(TXNCopy);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNCopy(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNPaste(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNPaste)
PyMac_PRECHECK(TXNPaste);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNPaste(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNClear(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNClear)
PyMac_PRECHECK(TXNClear);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNClear(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNGetSelection(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
TXNOffset oStartOffset;
TXNOffset oEndOffset;
#if !defined(TXNGetSelection)
PyMac_PRECHECK(TXNGetSelection);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNGetSelection(_self->ob_itself,
&oStartOffset,
&oEndOffset);
_res = Py_BuildValue("ll",
oStartOffset,
oEndOffset);
return _res;
}
static PyObject *TXNObj_TXNShowSelection(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean iShowEnd;
#if !defined(TXNShowSelection)
PyMac_PRECHECK(TXNShowSelection);
#endif
if (!PyArg_ParseTuple(_args, "b",
&iShowEnd))
return NULL;
TXNShowSelection(_self->ob_itself,
iShowEnd);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNIsSelectionEmpty(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean _rv;
#if !defined(TXNIsSelectionEmpty)
PyMac_PRECHECK(TXNIsSelectionEmpty);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNIsSelectionEmpty(_self->ob_itself);
_res = Py_BuildValue("b",
_rv);
return _res;
}
static PyObject *TXNObj_TXNSetSelection(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNOffset iStartOffset;
TXNOffset iEndOffset;
#if !defined(TXNSetSelection)
PyMac_PRECHECK(TXNSetSelection);
#endif
if (!PyArg_ParseTuple(_args, "ll",
&iStartOffset,
&iEndOffset))
return NULL;
_err = TXNSetSelection(_self->ob_itself,
iStartOffset,
iEndOffset);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNCountRunsInRange(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNOffset iStartOffset;
TXNOffset iEndOffset;
ItemCount oRunCount;
#if !defined(TXNCountRunsInRange)
PyMac_PRECHECK(TXNCountRunsInRange);
#endif
if (!PyArg_ParseTuple(_args, "ll",
&iStartOffset,
&iEndOffset))
return NULL;
_err = TXNCountRunsInRange(_self->ob_itself,
iStartOffset,
iEndOffset,
&oRunCount);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("l",
oRunCount);
return _res;
}
static PyObject *TXNObj_TXNDataSize(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
ByteCount _rv;
#if !defined(TXNDataSize)
PyMac_PRECHECK(TXNDataSize);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNDataSize(_self->ob_itself);
_res = Py_BuildValue("l",
_rv);
return _res;
}
static PyObject *TXNObj_TXNGetData(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNOffset iStartOffset;
TXNOffset iEndOffset;
Handle oDataHandle;
#if !defined(TXNGetData)
PyMac_PRECHECK(TXNGetData);
#endif
if (!PyArg_ParseTuple(_args, "ll",
&iStartOffset,
&iEndOffset))
return NULL;
_err = TXNGetData(_self->ob_itself,
iStartOffset,
iEndOffset,
&oDataHandle);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
ResObj_New, oDataHandle);
return _res;
}
static PyObject *TXNObj_TXNGetDataEncoded(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNOffset iStartOffset;
TXNOffset iEndOffset;
Handle oDataHandle;
TXNDataType iEncoding;
#if !defined(TXNGetDataEncoded)
PyMac_PRECHECK(TXNGetDataEncoded);
#endif
if (!PyArg_ParseTuple(_args, "llO&",
&iStartOffset,
&iEndOffset,
PyMac_GetOSType, &iEncoding))
return NULL;
_err = TXNGetDataEncoded(_self->ob_itself,
iStartOffset,
iEndOffset,
&oDataHandle,
iEncoding);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
ResObj_New, oDataHandle);
return _res;
}
static PyObject *TXNObj_TXNSetDataFromFile(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
SInt16 iFileRefNum;
OSType iFileType;
ByteCount iFileLength;
TXNOffset iStartOffset;
TXNOffset iEndOffset;
#if !defined(TXNSetDataFromFile)
PyMac_PRECHECK(TXNSetDataFromFile);
#endif
if (!PyArg_ParseTuple(_args, "hO&lll",
&iFileRefNum,
PyMac_GetOSType, &iFileType,
&iFileLength,
&iStartOffset,
&iEndOffset))
return NULL;
_err = TXNSetDataFromFile(_self->ob_itself,
iFileRefNum,
iFileType,
iFileLength,
iStartOffset,
iEndOffset);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNGetChangeCount(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
ItemCount _rv;
#if !defined(TXNGetChangeCount)
PyMac_PRECHECK(TXNGetChangeCount);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNGetChangeCount(_self->ob_itself);
_res = Py_BuildValue("l",
_rv);
return _res;
}
static PyObject *TXNObj_TXNSave(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNFileType iType;
OSType iResType;
TXNPermanentTextEncodingType iPermanentEncoding;
FSSpec iFileSpecification;
SInt16 iDataReference;
SInt16 iResourceReference;
#if !defined(TXNSave)
PyMac_PRECHECK(TXNSave);
#endif
if (!PyArg_ParseTuple(_args, "O&O&lO&hh",
PyMac_GetOSType, &iType,
PyMac_GetOSType, &iResType,
&iPermanentEncoding,
PyMac_GetFSSpec, &iFileSpecification,
&iDataReference,
&iResourceReference))
return NULL;
_err = TXNSave(_self->ob_itself,
iType,
iResType,
iPermanentEncoding,
&iFileSpecification,
iDataReference,
iResourceReference);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNRevert(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNRevert)
PyMac_PRECHECK(TXNRevert);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNRevert(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNPageSetup(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNPageSetup)
PyMac_PRECHECK(TXNPageSetup);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNPageSetup(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNPrint(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNPrint)
PyMac_PRECHECK(TXNPrint);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNPrint(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNGetViewRect(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Rect oViewRect;
#if !defined(TXNGetViewRect)
PyMac_PRECHECK(TXNGetViewRect);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNGetViewRect(_self->ob_itself,
&oViewRect);
_res = Py_BuildValue("O&",
PyMac_BuildRect, &oViewRect);
return _res;
}
static PyObject *TXNObj_TXNSetViewRect(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Rect iViewRect;
#if !defined(TXNSetViewRect)
PyMac_PRECHECK(TXNSetViewRect);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetRect, &iViewRect))
return NULL;
TXNSetViewRect(_self->ob_itself,
&iViewRect);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNAttachObjectToWindow(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
GWorldPtr iWindow;
Boolean iIsActualWindow;
#if !defined(TXNAttachObjectToWindow)
PyMac_PRECHECK(TXNAttachObjectToWindow);
#endif
if (!PyArg_ParseTuple(_args, "O&b",
GWorldObj_Convert, &iWindow,
&iIsActualWindow))
return NULL;
_err = TXNAttachObjectToWindow(_self->ob_itself,
iWindow,
iIsActualWindow);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNIsObjectAttachedToWindow(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean _rv;
#if !defined(TXNIsObjectAttachedToWindow)
PyMac_PRECHECK(TXNIsObjectAttachedToWindow);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNIsObjectAttachedToWindow(_self->ob_itself);
_res = Py_BuildValue("b",
_rv);
return _res;
}
static PyObject *TXNObj_TXNDragTracker(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
TXNFrameID iTXNFrameID;
DragTrackingMessage iMessage;
WindowPtr iWindow;
DragReference iDragReference;
Boolean iDifferentObjectSameWindow;
#if !defined(TXNDragTracker)
PyMac_PRECHECK(TXNDragTracker);
#endif
if (!PyArg_ParseTuple(_args, "lhO&O&b",
&iTXNFrameID,
&iMessage,
WinObj_Convert, &iWindow,
DragObj_Convert, &iDragReference,
&iDifferentObjectSameWindow))
return NULL;
_err = TXNDragTracker(_self->ob_itself,
iTXNFrameID,
iMessage,
iWindow,
iDragReference,
iDifferentObjectSameWindow);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNDragReceiver(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSErr _err;
TXNFrameID iTXNFrameID;
WindowPtr iWindow;
DragReference iDragReference;
Boolean iDifferentObjectSameWindow;
#if !defined(TXNDragReceiver)
PyMac_PRECHECK(TXNDragReceiver);
#endif
if (!PyArg_ParseTuple(_args, "lO&O&b",
&iTXNFrameID,
WinObj_Convert, &iWindow,
DragObj_Convert, &iDragReference,
&iDifferentObjectSameWindow))
return NULL;
_err = TXNDragReceiver(_self->ob_itself,
iTXNFrameID,
iWindow,
iDragReference,
iDifferentObjectSameWindow);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNActivate(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNFrameID iTXNFrameID;
TXNScrollBarState iActiveState;
#if !defined(TXNActivate)
PyMac_PRECHECK(TXNActivate);
#endif
if (!PyArg_ParseTuple(_args, "ll",
&iTXNFrameID,
&iActiveState))
return NULL;
_err = TXNActivate(_self->ob_itself,
iTXNFrameID,
iActiveState);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNEchoMode(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
UniChar iEchoCharacter;
TextEncoding iEncoding;
Boolean iOn;
#if !defined(TXNEchoMode)
PyMac_PRECHECK(TXNEchoMode);
#endif
if (!PyArg_ParseTuple(_args, "hlb",
&iEchoCharacter,
&iEncoding,
&iOn))
return NULL;
_err = TXNEchoMode(_self->ob_itself,
iEchoCharacter,
iEncoding,
iOn);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNDoFontMenuSelection(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNFontMenuObject iTXNFontMenuObject;
SInt16 iMenuID;
SInt16 iMenuItem;
#if !defined(TXNDoFontMenuSelection)
PyMac_PRECHECK(TXNDoFontMenuSelection);
#endif
if (!PyArg_ParseTuple(_args, "O&hh",
TXNFontMenuObj_Convert, &iTXNFontMenuObject,
&iMenuID,
&iMenuItem))
return NULL;
_err = TXNDoFontMenuSelection(_self->ob_itself,
iTXNFontMenuObject,
iMenuID,
iMenuItem);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNPrepareFontMenu(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNFontMenuObject iTXNFontMenuObject;
#if !defined(TXNPrepareFontMenu)
PyMac_PRECHECK(TXNPrepareFontMenu);
#endif
if (!PyArg_ParseTuple(_args, "O&",
TXNFontMenuObj_Convert, &iTXNFontMenuObject))
return NULL;
_err = TXNPrepareFontMenu(_self->ob_itself,
iTXNFontMenuObject);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *TXNObj_TXNPointToOffset(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
Point iPoint;
TXNOffset oOffset;
#if !defined(TXNPointToOffset)
PyMac_PRECHECK(TXNPointToOffset);
#endif
if (!PyArg_ParseTuple(_args, "O&",
PyMac_GetPoint, &iPoint))
return NULL;
_err = TXNPointToOffset(_self->ob_itself,
iPoint,
&oOffset);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("l",
oOffset);
return _res;
}
static PyObject *TXNObj_TXNOffsetToPoint(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNOffset iOffset;
Point oPoint;
#if !defined(TXNOffsetToPoint)
PyMac_PRECHECK(TXNOffsetToPoint);
#endif
if (!PyArg_ParseTuple(_args, "l",
&iOffset))
return NULL;
_err = TXNOffsetToPoint(_self->ob_itself,
iOffset,
&oPoint);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
PyMac_BuildPoint, oPoint);
return _res;
}
static PyObject *TXNObj_TXNGetLineCount(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
ItemCount oLineTotal;
#if !defined(TXNGetLineCount)
PyMac_PRECHECK(TXNGetLineCount);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNGetLineCount(_self->ob_itself,
&oLineTotal);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("l",
oLineTotal);
return _res;
}
static PyObject *TXNObj_TXNGetLineMetrics(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
UInt32 iLineNumber;
Fixed oLineWidth;
Fixed oLineHeight;
#if !defined(TXNGetLineMetrics)
PyMac_PRECHECK(TXNGetLineMetrics);
#endif
if (!PyArg_ParseTuple(_args, "l",
&iLineNumber))
return NULL;
_err = TXNGetLineMetrics(_self->ob_itself,
iLineNumber,
&oLineWidth,
&oLineHeight);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&O&",
PyMac_BuildFixed, oLineWidth,
PyMac_BuildFixed, oLineHeight);
return _res;
}
static PyObject *TXNObj_TXNIsObjectAttachedToSpecificWindow(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
WindowPtr iWindow;
Boolean oAttached;
#if !defined(TXNIsObjectAttachedToSpecificWindow)
PyMac_PRECHECK(TXNIsObjectAttachedToSpecificWindow);
#endif
if (!PyArg_ParseTuple(_args, "O&",
WinObj_Convert, &iWindow))
return NULL;
_err = TXNIsObjectAttachedToSpecificWindow(_self->ob_itself,
iWindow,
&oAttached);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("b",
oAttached);
return _res;
}
static PyObject *TXNObj_TXNRecalcTextLayout(TXNObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNRecalcTextLayout)
PyMac_PRECHECK(TXNRecalcTextLayout);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNRecalcTextLayout(_self->ob_itself);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyMethodDef TXNObj_methods[] = {
{
"TXNDeleteObject", (PyCFunction)TXNObj_TXNDeleteObject, 1,
PyDoc_STR("() -> None")
},
{
"TXNResizeFrame", (PyCFunction)TXNObj_TXNResizeFrame, 1,
PyDoc_STR("(UInt32 iWidth, UInt32 iHeight, TXNFrameID iTXNFrameID) -> None")
},
{
"TXNSetFrameBounds", (PyCFunction)TXNObj_TXNSetFrameBounds, 1,
PyDoc_STR("(SInt32 iTop, SInt32 iLeft, SInt32 iBottom, SInt32 iRight, TXNFrameID iTXNFrameID) -> None")
},
{
"TXNKeyDown", (PyCFunction)TXNObj_TXNKeyDown, 1,
PyDoc_STR("(EventRecord iEvent) -> None")
},
{
"TXNAdjustCursor", (PyCFunction)TXNObj_TXNAdjustCursor, 1,
PyDoc_STR("(RgnHandle ioCursorRgn) -> None")
},
{
"TXNClick", (PyCFunction)TXNObj_TXNClick, 1,
PyDoc_STR("(EventRecord iEvent) -> None")
},
{
"TXNSelectAll", (PyCFunction)TXNObj_TXNSelectAll, 1,
PyDoc_STR("() -> None")
},
{
"TXNFocus", (PyCFunction)TXNObj_TXNFocus, 1,
PyDoc_STR("(Boolean iBecomingFocused) -> None")
},
{
"TXNUpdate", (PyCFunction)TXNObj_TXNUpdate, 1,
PyDoc_STR("() -> None")
},
{
"TXNDraw", (PyCFunction)TXNObj_TXNDraw, 1,
PyDoc_STR("(GWorldPtr iDrawPort) -> None")
},
{
"TXNForceUpdate", (PyCFunction)TXNObj_TXNForceUpdate, 1,
PyDoc_STR("() -> None")
},
{
"TXNGetSleepTicks", (PyCFunction)TXNObj_TXNGetSleepTicks, 1,
PyDoc_STR("() -> (UInt32 _rv)")
},
{
"TXNIdle", (PyCFunction)TXNObj_TXNIdle, 1,
PyDoc_STR("() -> None")
},
{
"TXNGrowWindow", (PyCFunction)TXNObj_TXNGrowWindow, 1,
PyDoc_STR("(EventRecord iEvent) -> None")
},
{
"TXNZoomWindow", (PyCFunction)TXNObj_TXNZoomWindow, 1,
PyDoc_STR("(SInt16 iPart) -> None")
},
{
"TXNCanUndo", (PyCFunction)TXNObj_TXNCanUndo, 1,
PyDoc_STR("() -> (Boolean _rv, TXNActionKey oTXNActionKey)")
},
{
"TXNUndo", (PyCFunction)TXNObj_TXNUndo, 1,
PyDoc_STR("() -> None")
},
{
"TXNCanRedo", (PyCFunction)TXNObj_TXNCanRedo, 1,
PyDoc_STR("() -> (Boolean _rv, TXNActionKey oTXNActionKey)")
},
{
"TXNRedo", (PyCFunction)TXNObj_TXNRedo, 1,
PyDoc_STR("() -> None")
},
{
"TXNCut", (PyCFunction)TXNObj_TXNCut, 1,
PyDoc_STR("() -> None")
},
{
"TXNCopy", (PyCFunction)TXNObj_TXNCopy, 1,
PyDoc_STR("() -> None")
},
{
"TXNPaste", (PyCFunction)TXNObj_TXNPaste, 1,
PyDoc_STR("() -> None")
},
{
"TXNClear", (PyCFunction)TXNObj_TXNClear, 1,
PyDoc_STR("() -> None")
},
{
"TXNGetSelection", (PyCFunction)TXNObj_TXNGetSelection, 1,
PyDoc_STR("() -> (TXNOffset oStartOffset, TXNOffset oEndOffset)")
},
{
"TXNShowSelection", (PyCFunction)TXNObj_TXNShowSelection, 1,
PyDoc_STR("(Boolean iShowEnd) -> None")
},
{
"TXNIsSelectionEmpty", (PyCFunction)TXNObj_TXNIsSelectionEmpty, 1,
PyDoc_STR("() -> (Boolean _rv)")
},
{
"TXNSetSelection", (PyCFunction)TXNObj_TXNSetSelection, 1,
PyDoc_STR("(TXNOffset iStartOffset, TXNOffset iEndOffset) -> None")
},
{
"TXNCountRunsInRange", (PyCFunction)TXNObj_TXNCountRunsInRange, 1,
PyDoc_STR("(TXNOffset iStartOffset, TXNOffset iEndOffset) -> (ItemCount oRunCount)")
},
{
"TXNDataSize", (PyCFunction)TXNObj_TXNDataSize, 1,
PyDoc_STR("() -> (ByteCount _rv)")
},
{
"TXNGetData", (PyCFunction)TXNObj_TXNGetData, 1,
PyDoc_STR("(TXNOffset iStartOffset, TXNOffset iEndOffset) -> (Handle oDataHandle)")
},
{
"TXNGetDataEncoded", (PyCFunction)TXNObj_TXNGetDataEncoded, 1,
PyDoc_STR("(TXNOffset iStartOffset, TXNOffset iEndOffset, TXNDataType iEncoding) -> (Handle oDataHandle)")
},
{
"TXNSetDataFromFile", (PyCFunction)TXNObj_TXNSetDataFromFile, 1,
PyDoc_STR("(SInt16 iFileRefNum, OSType iFileType, ByteCount iFileLength, TXNOffset iStartOffset, TXNOffset iEndOffset) -> None")
},
{
"TXNGetChangeCount", (PyCFunction)TXNObj_TXNGetChangeCount, 1,
PyDoc_STR("() -> (ItemCount _rv)")
},
{
"TXNSave", (PyCFunction)TXNObj_TXNSave, 1,
PyDoc_STR("(TXNFileType iType, OSType iResType, TXNPermanentTextEncodingType iPermanentEncoding, FSSpec iFileSpecification, SInt16 iDataReference, SInt16 iResourceReference) -> None")
},
{
"TXNRevert", (PyCFunction)TXNObj_TXNRevert, 1,
PyDoc_STR("() -> None")
},
{
"TXNPageSetup", (PyCFunction)TXNObj_TXNPageSetup, 1,
PyDoc_STR("() -> None")
},
{
"TXNPrint", (PyCFunction)TXNObj_TXNPrint, 1,
PyDoc_STR("() -> None")
},
{
"TXNGetViewRect", (PyCFunction)TXNObj_TXNGetViewRect, 1,
PyDoc_STR("() -> (Rect oViewRect)")
},
{
"TXNSetViewRect", (PyCFunction)TXNObj_TXNSetViewRect, 1,
PyDoc_STR("(Rect iViewRect) -> None")
},
{
"TXNAttachObjectToWindow", (PyCFunction)TXNObj_TXNAttachObjectToWindow, 1,
PyDoc_STR("(GWorldPtr iWindow, Boolean iIsActualWindow) -> None")
},
{
"TXNIsObjectAttachedToWindow", (PyCFunction)TXNObj_TXNIsObjectAttachedToWindow, 1,
PyDoc_STR("() -> (Boolean _rv)")
},
{
"TXNDragTracker", (PyCFunction)TXNObj_TXNDragTracker, 1,
PyDoc_STR("(TXNFrameID iTXNFrameID, DragTrackingMessage iMessage, WindowPtr iWindow, DragReference iDragReference, Boolean iDifferentObjectSameWindow) -> None")
},
{
"TXNDragReceiver", (PyCFunction)TXNObj_TXNDragReceiver, 1,
PyDoc_STR("(TXNFrameID iTXNFrameID, WindowPtr iWindow, DragReference iDragReference, Boolean iDifferentObjectSameWindow) -> None")
},
{
"TXNActivate", (PyCFunction)TXNObj_TXNActivate, 1,
PyDoc_STR("(TXNFrameID iTXNFrameID, TXNScrollBarState iActiveState) -> None")
},
{
"TXNEchoMode", (PyCFunction)TXNObj_TXNEchoMode, 1,
PyDoc_STR("(UniChar iEchoCharacter, TextEncoding iEncoding, Boolean iOn) -> None")
},
{
"TXNDoFontMenuSelection", (PyCFunction)TXNObj_TXNDoFontMenuSelection, 1,
PyDoc_STR("(TXNFontMenuObject iTXNFontMenuObject, SInt16 iMenuID, SInt16 iMenuItem) -> None")
},
{
"TXNPrepareFontMenu", (PyCFunction)TXNObj_TXNPrepareFontMenu, 1,
PyDoc_STR("(TXNFontMenuObject iTXNFontMenuObject) -> None")
},
{
"TXNPointToOffset", (PyCFunction)TXNObj_TXNPointToOffset, 1,
PyDoc_STR("(Point iPoint) -> (TXNOffset oOffset)")
},
{
"TXNOffsetToPoint", (PyCFunction)TXNObj_TXNOffsetToPoint, 1,
PyDoc_STR("(TXNOffset iOffset) -> (Point oPoint)")
},
{
"TXNGetLineCount", (PyCFunction)TXNObj_TXNGetLineCount, 1,
PyDoc_STR("() -> (ItemCount oLineTotal)")
},
{
"TXNGetLineMetrics", (PyCFunction)TXNObj_TXNGetLineMetrics, 1,
PyDoc_STR("(UInt32 iLineNumber) -> (Fixed oLineWidth, Fixed oLineHeight)")
},
{
"TXNIsObjectAttachedToSpecificWindow", (PyCFunction)TXNObj_TXNIsObjectAttachedToSpecificWindow, 1,
PyDoc_STR("(WindowPtr iWindow) -> (Boolean oAttached)")
},
{
"TXNRecalcTextLayout", (PyCFunction)TXNObj_TXNRecalcTextLayout, 1,
PyDoc_STR("() -> None")
},
{NULL, NULL, 0}
};
#define TXNObj_getsetlist NULL
#define TXNObj_compare NULL
#define TXNObj_repr NULL
#define TXNObj_hash NULL
#define TXNObj_tp_init 0
#define TXNObj_tp_alloc PyType_GenericAlloc
static PyObject *TXNObj_tp_new(PyTypeObject *type, PyObject *_args, PyObject *_kwds) {
PyObject *_self;
TXNObject itself;
char *kw[] = {"itself", 0};
if (!PyArg_ParseTupleAndKeywords(_args, _kwds, "O&", kw, TXNObj_Convert, &itself)) return NULL;
if ((_self = type->tp_alloc(type, 0)) == NULL) return NULL;
((TXNObjectObject *)_self)->ob_itself = itself;
return _self;
}
#define TXNObj_tp_free PyObject_Del
PyTypeObject TXNObject_Type = {
PyObject_HEAD_INIT(NULL)
0,
"_Mlte.TXNObject",
sizeof(TXNObjectObject),
0,
(destructor) TXNObj_dealloc,
0,
(getattrfunc)0,
(setattrfunc)0,
(cmpfunc) TXNObj_compare,
(reprfunc) TXNObj_repr,
(PyNumberMethods *)0,
(PySequenceMethods *)0,
(PyMappingMethods *)0,
(hashfunc) TXNObj_hash,
0,
0,
PyObject_GenericGetAttr,
PyObject_GenericSetAttr,
0,
Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
0,
0,
0,
0,
0,
0,
0,
TXNObj_methods,
0,
TXNObj_getsetlist,
0,
0,
0,
0,
0,
TXNObj_tp_init,
TXNObj_tp_alloc,
TXNObj_tp_new,
TXNObj_tp_free,
};
PyTypeObject TXNFontMenuObject_Type;
#define TXNFontMenuObj_Check(x) ((x)->ob_type == &TXNFontMenuObject_Type || PyObject_TypeCheck((x), &TXNFontMenuObject_Type))
typedef struct TXNFontMenuObjectObject {
PyObject_HEAD
TXNFontMenuObject ob_itself;
} TXNFontMenuObjectObject;
PyObject *TXNFontMenuObj_New(TXNFontMenuObject itself) {
TXNFontMenuObjectObject *it;
if (itself == NULL) return PyMac_Error(resNotFound);
it = PyObject_NEW(TXNFontMenuObjectObject, &TXNFontMenuObject_Type);
if (it == NULL) return NULL;
it->ob_itself = itself;
return (PyObject *)it;
}
int TXNFontMenuObj_Convert(PyObject *v, TXNFontMenuObject *p_itself) {
if (!TXNFontMenuObj_Check(v)) {
PyErr_SetString(PyExc_TypeError, "TXNFontMenuObject required");
return 0;
}
*p_itself = ((TXNFontMenuObjectObject *)v)->ob_itself;
return 1;
}
static void TXNFontMenuObj_dealloc(TXNFontMenuObjectObject *self) {
self->ob_type->tp_free((PyObject *)self);
}
static PyObject *TXNFontMenuObj_TXNGetFontMenuHandle(TXNFontMenuObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
MenuHandle oFontMenuHandle;
#if !defined(TXNGetFontMenuHandle)
PyMac_PRECHECK(TXNGetFontMenuHandle);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNGetFontMenuHandle(_self->ob_itself,
&oFontMenuHandle);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
MenuObj_New, oFontMenuHandle);
return _res;
}
static PyObject *TXNFontMenuObj_TXNDisposeFontMenuObject(TXNFontMenuObjectObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNDisposeFontMenuObject)
PyMac_PRECHECK(TXNDisposeFontMenuObject);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNDisposeFontMenuObject(_self->ob_itself);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyMethodDef TXNFontMenuObj_methods[] = {
{
"TXNGetFontMenuHandle", (PyCFunction)TXNFontMenuObj_TXNGetFontMenuHandle, 1,
PyDoc_STR("() -> (MenuHandle oFontMenuHandle)")
},
{
"TXNDisposeFontMenuObject", (PyCFunction)TXNFontMenuObj_TXNDisposeFontMenuObject, 1,
PyDoc_STR("() -> None")
},
{NULL, NULL, 0}
};
#define TXNFontMenuObj_getsetlist NULL
#define TXNFontMenuObj_compare NULL
#define TXNFontMenuObj_repr NULL
#define TXNFontMenuObj_hash NULL
#define TXNFontMenuObj_tp_init 0
#define TXNFontMenuObj_tp_alloc PyType_GenericAlloc
static PyObject *TXNFontMenuObj_tp_new(PyTypeObject *type, PyObject *_args, PyObject *_kwds) {
PyObject *_self;
TXNFontMenuObject itself;
char *kw[] = {"itself", 0};
if (!PyArg_ParseTupleAndKeywords(_args, _kwds, "O&", kw, TXNFontMenuObj_Convert, &itself)) return NULL;
if ((_self = type->tp_alloc(type, 0)) == NULL) return NULL;
((TXNFontMenuObjectObject *)_self)->ob_itself = itself;
return _self;
}
#define TXNFontMenuObj_tp_free PyObject_Del
PyTypeObject TXNFontMenuObject_Type = {
PyObject_HEAD_INIT(NULL)
0,
"_Mlte.TXNFontMenuObject",
sizeof(TXNFontMenuObjectObject),
0,
(destructor) TXNFontMenuObj_dealloc,
0,
(getattrfunc)0,
(setattrfunc)0,
(cmpfunc) TXNFontMenuObj_compare,
(reprfunc) TXNFontMenuObj_repr,
(PyNumberMethods *)0,
(PySequenceMethods *)0,
(PyMappingMethods *)0,
(hashfunc) TXNFontMenuObj_hash,
0,
0,
PyObject_GenericGetAttr,
PyObject_GenericSetAttr,
0,
Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
0,
0,
0,
0,
0,
0,
0,
TXNFontMenuObj_methods,
0,
TXNFontMenuObj_getsetlist,
0,
0,
0,
0,
0,
TXNFontMenuObj_tp_init,
TXNFontMenuObj_tp_alloc,
TXNFontMenuObj_tp_new,
TXNFontMenuObj_tp_free,
};
static PyObject *Mlte_TXNNewObject(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
FSSpec * iFileSpec;
WindowPtr iWindow;
Rect iFrame;
TXNFrameOptions iFrameOptions;
TXNFrameType iFrameType;
TXNFileType iFileType;
TXNPermanentTextEncodingType iPermanentEncoding;
TXNObject oTXNObject;
TXNFrameID oTXNFrameID;
#if !defined(TXNNewObject)
PyMac_PRECHECK(TXNNewObject);
#endif
if (!PyArg_ParseTuple(_args, "O&O&O&llO&l",
OptFSSpecPtr_Convert, &iFileSpec,
WinObj_Convert, &iWindow,
PyMac_GetRect, &iFrame,
&iFrameOptions,
&iFrameType,
PyMac_GetOSType, &iFileType,
&iPermanentEncoding))
return NULL;
_err = TXNNewObject(iFileSpec,
iWindow,
&iFrame,
iFrameOptions,
iFrameType,
iFileType,
iPermanentEncoding,
&oTXNObject,
&oTXNFrameID,
(TXNObjectRefcon)0);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&l",
TXNObj_New, oTXNObject,
oTXNFrameID);
return _res;
}
static PyObject *Mlte_TXNTerminateTextension(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
#if !defined(TXNTerminateTextension)
PyMac_PRECHECK(TXNTerminateTextension);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
TXNTerminateTextension();
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Mlte_TXNIsScrapPastable(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
Boolean _rv;
#if !defined(TXNIsScrapPastable)
PyMac_PRECHECK(TXNIsScrapPastable);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNIsScrapPastable();
_res = Py_BuildValue("b",
_rv);
return _res;
}
static PyObject *Mlte_TXNConvertToPublicScrap(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNConvertToPublicScrap)
PyMac_PRECHECK(TXNConvertToPublicScrap);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNConvertToPublicScrap();
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Mlte_TXNConvertFromPublicScrap(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
#if !defined(TXNConvertFromPublicScrap)
PyMac_PRECHECK(TXNConvertFromPublicScrap);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_err = TXNConvertFromPublicScrap();
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
static PyObject *Mlte_TXNNewFontMenuObject(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
MenuHandle iFontMenuHandle;
SInt16 iMenuID;
SInt16 iStartHierMenuID;
TXNFontMenuObject oTXNFontMenuObject;
#if !defined(TXNNewFontMenuObject)
PyMac_PRECHECK(TXNNewFontMenuObject);
#endif
if (!PyArg_ParseTuple(_args, "O&hh",
MenuObj_Convert, &iFontMenuHandle,
&iMenuID,
&iStartHierMenuID))
return NULL;
_err = TXNNewFontMenuObject(iFontMenuHandle,
iMenuID,
iStartHierMenuID,
&oTXNFontMenuObject);
if (_err != noErr) return PyMac_Error(_err);
_res = Py_BuildValue("O&",
TXNFontMenuObj_New, oTXNFontMenuObject);
return _res;
}
static PyObject *Mlte_TXNVersionInformation(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
TXNVersionValue _rv;
TXNFeatureBits oFeatureFlags;
#if !defined(TXNVersionInformation)
PyMac_PRECHECK(TXNVersionInformation);
#endif
if (!PyArg_ParseTuple(_args, ""))
return NULL;
_rv = TXNVersionInformation(&oFeatureFlags);
_res = Py_BuildValue("ll",
_rv,
oFeatureFlags);
return _res;
}
static PyObject *Mlte_TXNInitTextension(PyObject *_self, PyObject *_args) {
PyObject *_res = NULL;
OSStatus _err;
TXNMacOSPreferredFontDescription * iDefaultFonts = NULL;
ItemCount iCountDefaultFonts = 0;
TXNInitOptions iUsageFlags;
PyMac_PRECHECK(TXNInitTextension);
if (!PyArg_ParseTuple(_args, "l", &iUsageFlags))
return NULL;
_err = TXNInitTextension(iDefaultFonts,
iCountDefaultFonts,
iUsageFlags);
if (_err != noErr) return PyMac_Error(_err);
Py_INCREF(Py_None);
_res = Py_None;
return _res;
}
#endif
static PyMethodDef Mlte_methods[] = {
#if !defined(__LP64__)
{
"TXNNewObject", (PyCFunction)Mlte_TXNNewObject, 1,
PyDoc_STR("(FSSpec * iFileSpec, WindowPtr iWindow, Rect iFrame, TXNFrameOptions iFrameOptions, TXNFrameType iFrameType, TXNFileType iFileType, TXNPermanentTextEncodingType iPermanentEncoding) -> (TXNObject oTXNObject, TXNFrameID oTXNFrameID)")
},
{
"TXNTerminateTextension", (PyCFunction)Mlte_TXNTerminateTextension, 1,
PyDoc_STR("() -> None")
},
{
"TXNIsScrapPastable", (PyCFunction)Mlte_TXNIsScrapPastable, 1,
PyDoc_STR("() -> (Boolean _rv)")
},
{
"TXNConvertToPublicScrap", (PyCFunction)Mlte_TXNConvertToPublicScrap, 1,
PyDoc_STR("() -> None")
},
{
"TXNConvertFromPublicScrap", (PyCFunction)Mlte_TXNConvertFromPublicScrap, 1,
PyDoc_STR("() -> None")
},
{
"TXNNewFontMenuObject", (PyCFunction)Mlte_TXNNewFontMenuObject, 1,
PyDoc_STR("(MenuHandle iFontMenuHandle, SInt16 iMenuID, SInt16 iStartHierMenuID) -> (TXNFontMenuObject oTXNFontMenuObject)")
},
{
"TXNVersionInformation", (PyCFunction)Mlte_TXNVersionInformation, 1,
PyDoc_STR("() -> (TXNVersionValue _rv, TXNFeatureBits oFeatureFlags)")
},
{
"TXNInitTextension", (PyCFunction)Mlte_TXNInitTextension, 1,
PyDoc_STR("(TXNInitOptions) -> None")
},
#endif
{NULL, NULL, 0}
};
void init_Mlte(void) {
PyObject *m;
#if !defined(__LP64__)
PyObject *d;
#endif
m = Py_InitModule("_Mlte", Mlte_methods);
#if !defined(__LP64__)
d = PyModule_GetDict(m);
Mlte_Error = PyMac_GetOSErrException();
if (Mlte_Error == NULL ||
PyDict_SetItemString(d, "Error", Mlte_Error) != 0)
return;
TXNObject_Type.ob_type = &PyType_Type;
if (PyType_Ready(&TXNObject_Type) < 0) return;
Py_INCREF(&TXNObject_Type);
PyModule_AddObject(m, "TXNObject", (PyObject *)&TXNObject_Type);
Py_INCREF(&TXNObject_Type);
PyModule_AddObject(m, "TXNObjectType", (PyObject *)&TXNObject_Type);
TXNFontMenuObject_Type.ob_type = &PyType_Type;
if (PyType_Ready(&TXNFontMenuObject_Type) < 0) return;
Py_INCREF(&TXNFontMenuObject_Type);
PyModule_AddObject(m, "TXNFontMenuObject", (PyObject *)&TXNFontMenuObject_Type);
Py_INCREF(&TXNFontMenuObject_Type);
PyModule_AddObject(m, "TXNFontMenuObjectType", (PyObject *)&TXNFontMenuObject_Type);
#endif
}