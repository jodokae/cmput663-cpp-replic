#include <string.h>
#include <tcl.h>
#include <tk.h>
int
Tcl_AppInit(Tcl_Interp *interp) {
Tk_Window main_window;
const char * _tkinter_skip_tk_init;
#if defined(TK_AQUA)
#if !defined(MAX_PATH_LEN)
#define MAX_PATH_LEN 1024
#endif
char tclLibPath[MAX_PATH_LEN], tkLibPath[MAX_PATH_LEN];
Tcl_Obj* pathPtr;
Tk_MacOSXOpenBundleResources (interp, "com.tcltk.tcllibrary",
tclLibPath, MAX_PATH_LEN, 0);
if (tclLibPath[0] != '\0') {
Tcl_SetVar(interp, "tcl_library", tclLibPath, TCL_GLOBAL_ONLY);
Tcl_SetVar(interp, "tclDefaultLibrary", tclLibPath, TCL_GLOBAL_ONLY);
Tcl_SetVar(interp, "tcl_pkgPath", tclLibPath, TCL_GLOBAL_ONLY);
}
if (tclLibPath[0] != '\0') {
Tcl_SetVar(interp, "tcl_library", tclLibPath, TCL_GLOBAL_ONLY);
Tcl_SetVar(interp, "tclDefaultLibrary", tclLibPath, TCL_GLOBAL_ONLY);
Tcl_SetVar(interp, "tcl_pkgPath", tclLibPath, TCL_GLOBAL_ONLY);
}
#endif
if (Tcl_Init (interp) == TCL_ERROR)
return TCL_ERROR;
#if defined(TK_AQUA)
Tk_MacOSXOpenBundleResources (interp, "com.tcltk.tklibrary",
tkLibPath, MAX_PATH_LEN, 1);
if (tclLibPath[0] != '\0') {
pathPtr = Tcl_NewStringObj(tclLibPath, -1);
} else {
Tcl_Obj *pathPtr = TclGetLibraryPath();
}
if (tkLibPath[0] != '\0') {
Tcl_Obj *objPtr;
Tcl_SetVar(interp, "tk_library", tkLibPath, TCL_GLOBAL_ONLY);
objPtr = Tcl_NewStringObj(tkLibPath, -1);
Tcl_ListObjAppendElement(NULL, pathPtr, objPtr);
}
TclSetLibraryPath(pathPtr);
#endif
#if defined(WITH_XXX)
#endif
_tkinter_skip_tk_init = Tcl_GetVar(interp, "_tkinter_skip_tk_init", TCL_GLOBAL_ONLY);
if (_tkinter_skip_tk_init != NULL && strcmp(_tkinter_skip_tk_init, "1") == 0) {
return TCL_OK;
}
if (Tk_Init(interp) == TCL_ERROR)
return TCL_ERROR;
main_window = Tk_MainWindow(interp);
#if defined(TK_AQUA)
TkMacOSXInitAppleEvents(interp);
TkMacOSXInitMenus(interp);
#endif
#if defined(WITH_MOREBUTTONS)
{
extern Tcl_CmdProc studButtonCmd;
extern Tcl_CmdProc triButtonCmd;
Tcl_CreateCommand(interp, "studbutton", studButtonCmd,
(ClientData) main_window, NULL);
Tcl_CreateCommand(interp, "tributton", triButtonCmd,
(ClientData) main_window, NULL);
}
#endif
#if defined(WITH_PIL)
{
extern void TkImaging_Init(Tcl_Interp *);
TkImaging_Init(interp);
}
#endif
#if defined(WITH_PIL_OLD)
{
extern void TkImaging_Init(void);
}
#endif
#if defined(WITH_TIX)
{
extern int Tix_Init(Tcl_Interp *interp);
extern int Tix_SafeInit(Tcl_Interp *interp);
Tcl_StaticPackage(NULL, "Tix", Tix_Init, Tix_SafeInit);
}
#endif
#if defined(WITH_BLT)
{
extern int Blt_Init(Tcl_Interp *);
extern int Blt_SafeInit(Tcl_Interp *);
Tcl_StaticPackage(NULL, "Blt", Blt_Init, Blt_SafeInit);
}
#endif
#if defined(WITH_TOGL)
{
extern int Togl_Init(Tcl_Interp *);
Tcl_StaticPackage(NULL, "Togl", Togl_Init, NULL);
}
#endif
#if defined(WITH_XXX)
#endif
return TCL_OK;
}
