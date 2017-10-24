#if !defined(Py_PYMACTOOLBOX_H)
#define Py_PYMACTOOLBOX_H
#if defined(__cplusplus)
extern "C" {
#endif
#include <Carbon/Carbon.h>
#if !defined(__LP64__)
#include <QuickTime/QuickTime.h>
#endif
char *PyMac_StrError(int);
extern PyObject *PyMac_OSErrException;
PyObject *PyMac_GetOSErrException(void);
PyObject *PyErr_Mac(PyObject *, int);
PyObject *PyMac_Error(OSErr);
#if !defined(__LP64__)
extern OSErr PyMac_GetFullPathname(FSSpec *, char *, int);
#endif
int PyMac_GetOSType(PyObject *, OSType *);
PyObject *PyMac_BuildOSType(OSType);
PyObject *PyMac_BuildNumVersion(NumVersion);
int PyMac_GetStr255(PyObject *, Str255);
PyObject *PyMac_BuildStr255(Str255);
PyObject *PyMac_BuildOptStr255(Str255);
int PyMac_GetRect(PyObject *, Rect *);
PyObject *PyMac_BuildRect(Rect *);
int PyMac_GetPoint(PyObject *, Point *);
PyObject *PyMac_BuildPoint(Point);
int PyMac_GetEventRecord(PyObject *, EventRecord *);
PyObject *PyMac_BuildEventRecord(EventRecord *);
int PyMac_GetFixed(PyObject *, Fixed *);
PyObject *PyMac_BuildFixed(Fixed);
int PyMac_Getwide(PyObject *, wide *);
PyObject *PyMac_Buildwide(wide *);
#if defined(USE_TOOLBOX_OBJECT_GLUE)
#define PyMac_INIT_TOOLBOX_OBJECT_NEW(object, rtn) { extern PyObject *(*PyMacGluePtr_##rtn)(object); PyMacGluePtr_##rtn = _##rtn; }
#define PyMac_INIT_TOOLBOX_OBJECT_CONVERT(object, rtn) { extern int (*PyMacGluePtr_##rtn)(PyObject *, object *); PyMacGluePtr_##rtn = _##rtn; }
#else
#define PyMac_INIT_TOOLBOX_OBJECT_NEW(object, rtn)
#define PyMac_INIT_TOOLBOX_OBJECT_CONVERT(object, rtn)
#endif
#if !defined(__LP64__)
int PyMac_GetFSSpec(PyObject *, FSSpec *);
PyObject *PyMac_BuildFSSpec(FSSpec *);
#endif
int PyMac_GetFSRef(PyObject *, FSRef *);
PyObject *PyMac_BuildFSRef(FSRef *);
extern PyObject *AEDesc_New(AppleEvent *);
extern PyObject *AEDesc_NewBorrowed(AppleEvent *);
extern int AEDesc_Convert(PyObject *, AppleEvent *);
extern PyObject *CmpObj_New(Component);
extern int CmpObj_Convert(PyObject *, Component *);
extern PyObject *CmpInstObj_New(ComponentInstance);
extern int CmpInstObj_Convert(PyObject *, ComponentInstance *);
#if !defined(__LP64__)
extern PyObject *CtlObj_New(ControlHandle);
extern int CtlObj_Convert(PyObject *, ControlHandle *);
#endif
#if !defined(__LP64__)
extern PyObject *DlgObj_New(DialogPtr);
extern int DlgObj_Convert(PyObject *, DialogPtr *);
extern PyObject *DlgObj_WhichDialog(DialogPtr);
#endif
#if !defined(__LP64__)
extern PyObject *DragObj_New(DragReference);
extern int DragObj_Convert(PyObject *, DragReference *);
#endif
#if !defined(__LP64__)
extern PyObject *ListObj_New(ListHandle);
extern int ListObj_Convert(PyObject *, ListHandle *);
#endif
#if !defined(__LP64__)
extern PyObject *MenuObj_New(MenuHandle);
extern int MenuObj_Convert(PyObject *, MenuHandle *);
#endif
#if !defined(__LP64__)
extern PyObject *GrafObj_New(GrafPtr);
extern int GrafObj_Convert(PyObject *, GrafPtr *);
extern PyObject *BMObj_New(BitMapPtr);
extern int BMObj_Convert(PyObject *, BitMapPtr *);
extern PyObject *QdRGB_New(RGBColor *);
extern int QdRGB_Convert(PyObject *, RGBColor *);
#endif
#if !defined(__LP64__)
extern PyObject *GWorldObj_New(GWorldPtr);
extern int GWorldObj_Convert(PyObject *, GWorldPtr *);
#endif
#if !defined(__LP64__)
extern PyObject *TrackObj_New(Track);
extern int TrackObj_Convert(PyObject *, Track *);
extern PyObject *MovieObj_New(Movie);
extern int MovieObj_Convert(PyObject *, Movie *);
extern PyObject *MovieCtlObj_New(MovieController);
extern int MovieCtlObj_Convert(PyObject *, MovieController *);
extern PyObject *TimeBaseObj_New(TimeBase);
extern int TimeBaseObj_Convert(PyObject *, TimeBase *);
extern PyObject *UserDataObj_New(UserData);
extern int UserDataObj_Convert(PyObject *, UserData *);
extern PyObject *MediaObj_New(Media);
extern int MediaObj_Convert(PyObject *, Media *);
#endif
extern PyObject *ResObj_New(Handle);
extern int ResObj_Convert(PyObject *, Handle *);
extern PyObject *OptResObj_New(Handle);
extern int OptResObj_Convert(PyObject *, Handle *);
#if !defined(__LP64__)
extern PyObject *TEObj_New(TEHandle);
extern int TEObj_Convert(PyObject *, TEHandle *);
#endif
#if !defined(__LP64__)
extern PyObject *WinObj_New(WindowPtr);
extern int WinObj_Convert(PyObject *, WindowPtr *);
extern PyObject *WinObj_WhichWindow(WindowPtr);
#endif
extern PyObject *CFObj_New(CFTypeRef);
extern int CFObj_Convert(PyObject *, CFTypeRef *);
extern PyObject *CFTypeRefObj_New(CFTypeRef);
extern int CFTypeRefObj_Convert(PyObject *, CFTypeRef *);
extern PyObject *CFStringRefObj_New(CFStringRef);
extern int CFStringRefObj_Convert(PyObject *, CFStringRef *);
extern PyObject *CFMutableStringRefObj_New(CFMutableStringRef);
extern int CFMutableStringRefObj_Convert(PyObject *, CFMutableStringRef *);
extern PyObject *CFArrayRefObj_New(CFArrayRef);
extern int CFArrayRefObj_Convert(PyObject *, CFArrayRef *);
extern PyObject *CFMutableArrayRefObj_New(CFMutableArrayRef);
extern int CFMutableArrayRefObj_Convert(PyObject *, CFMutableArrayRef *);
extern PyObject *CFDictionaryRefObj_New(CFDictionaryRef);
extern int CFDictionaryRefObj_Convert(PyObject *, CFDictionaryRef *);
extern PyObject *CFMutableDictionaryRefObj_New(CFMutableDictionaryRef);
extern int CFMutableDictionaryRefObj_Convert(PyObject *, CFMutableDictionaryRef *);
extern PyObject *CFURLRefObj_New(CFURLRef);
extern int CFURLRefObj_Convert(PyObject *, CFURLRef *);
extern int OptionalCFURLRefObj_Convert(PyObject *, CFURLRef *);
#if defined(__cplusplus)
}
#endif
#endif
