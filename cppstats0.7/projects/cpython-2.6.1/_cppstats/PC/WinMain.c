#include "Python.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI WinMain(
HINSTANCE hInstance,
HINSTANCE hPrevInstance,
LPSTR lpCmdLine,
int nCmdShow
) {
return Py_Main(__argc, __argv);
}
