#include "windows.h"
static char *possibleModules[] = {
"pywintypes",
"pythoncom",
"win32ui",
NULL,
};
BOOL CallModuleDllMain(char *modName, DWORD dwReason);
void PyWinFreeze_ExeInit(void) {
char **modName;
for (modName = possibleModules; *modName; *modName++) {
CallModuleDllMain(*modName, DLL_PROCESS_ATTACH);
}
}
void PyWinFreeze_ExeTerm(void) {
char **modName;
for (modName = possibleModules+(sizeof(possibleModules) / sizeof(char *))-2;
modName >= possibleModules;
*modName--) {
CallModuleDllMain(*modName, DLL_PROCESS_DETACH);
}
}
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
BOOL ret = TRUE;
switch (dwReason) {
case DLL_PROCESS_ATTACH: {
char **modName;
for (modName = possibleModules; *modName; *modName++) {
BOOL ok = CallModuleDllMain(*modName, dwReason);
if (!ok)
ret = FALSE;
}
break;
}
case DLL_PROCESS_DETACH: {
char **modName;
for (modName = possibleModules+(sizeof(possibleModules) / sizeof(char *))-2;
modName >= possibleModules;
*modName--)
CallModuleDllMain(*modName, DLL_PROCESS_DETACH);
break;
}
}
return ret;
}
BOOL CallModuleDllMain(char *modName, DWORD dwReason) {
BOOL (WINAPI * pfndllmain)(HINSTANCE, DWORD, LPVOID);
char funcName[255];
HMODULE hmod = GetModuleHandle(NULL);
strcpy(funcName, "_DllMain");
strcat(funcName, modName);
strcat(funcName, "@12");
pfndllmain = (BOOL (WINAPI *)(HINSTANCE, DWORD, LPVOID))GetProcAddress(hmod, funcName);
if (pfndllmain==NULL) {
return TRUE;
}
return (*pfndllmain)(hmod, dwReason, NULL);
}
