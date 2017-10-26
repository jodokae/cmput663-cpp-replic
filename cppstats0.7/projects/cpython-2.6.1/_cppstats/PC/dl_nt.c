#include "Python.h"
#include "windows.h"
#if defined(Py_ENABLE_SHARED)
char dllVersionBuffer[16] = "";
HMODULE PyWin_DLLhModule = NULL;
const char *PyWin_DLLVersionString = dllVersionBuffer;
BOOL WINAPI DllMain (HANDLE hInst,
ULONG ul_reason_for_call,
LPVOID lpReserved) {
switch (ul_reason_for_call) {
case DLL_PROCESS_ATTACH:
PyWin_DLLhModule = hInst;
LoadString(hInst, 1000, dllVersionBuffer, sizeof(dllVersionBuffer));
break;
case DLL_PROCESS_DETACH:
break;
}
return TRUE;
}
#endif
