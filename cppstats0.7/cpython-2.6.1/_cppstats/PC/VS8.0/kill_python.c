#include <windows.h>
#include <wchar.h>
#include <tlhelp32.h>
#include <stdio.h>
#pragma comment(lib, "psapi")
#if defined(_DEBUG)
#define PYTHON_EXE (L"python_d.exe")
#define PYTHON_EXE_LEN (12)
#define KILL_PYTHON_EXE (L"kill_python_d.exe")
#define KILL_PYTHON_EXE_LEN (17)
#else
#define PYTHON_EXE (L"python.exe")
#define PYTHON_EXE_LEN (10)
#define KILL_PYTHON_EXE (L"kill_python.exe")
#define KILL_PYTHON_EXE_LEN (15)
#endif
int
main(int argc, char **argv) {
HANDLE hp, hsp, hsm;
DWORD dac, our_pid;
size_t len;
wchar_t path[MAX_PATH+1];
MODULEENTRY32W me;
PROCESSENTRY32W pe;
me.dwSize = sizeof(MODULEENTRY32W);
pe.dwSize = sizeof(PROCESSENTRY32W);
memset(path, 0, MAX_PATH+1);
our_pid = GetCurrentProcessId();
hsm = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, our_pid);
if (hsm == INVALID_HANDLE_VALUE) {
printf("CreateToolhelp32Snapshot[1] failed: %d\n", GetLastError());
return 1;
}
if (!Module32FirstW(hsm, &me)) {
printf("Module32FirstW[1] failed: %d\n", GetLastError());
CloseHandle(hsm);
return 1;
}
do {
if (_wcsnicmp(me.szModule, KILL_PYTHON_EXE, KILL_PYTHON_EXE_LEN))
continue;
len = wcsnlen_s(me.szExePath, MAX_PATH) - KILL_PYTHON_EXE_LEN;
wcsncpy_s(path, MAX_PATH+1, me.szExePath, len);
break;
} while (Module32NextW(hsm, &me));
CloseHandle(hsm);
if (path == NULL) {
printf("failed to discern directory of running process\n");
return 1;
}
hsp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
if (hsp == INVALID_HANDLE_VALUE) {
printf("CreateToolhelp32Snapshot[2] failed: %d\n", GetLastError());
return 1;
}
if (!Process32FirstW(hsp, &pe)) {
printf("Process32FirstW failed: %d\n", GetLastError());
CloseHandle(hsp);
return 1;
}
dac = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE;
do {
if (_wcsnicmp(pe.szExeFile, PYTHON_EXE, PYTHON_EXE_LEN))
continue;
hsm = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe.th32ProcessID);
if (hsm == INVALID_HANDLE_VALUE)
continue;
if (!Module32FirstW(hsm, &me)) {
printf("Module32FirstW[2] failed: %d\n", GetLastError());
CloseHandle(hsp);
CloseHandle(hsm);
return 1;
}
do {
if (_wcsnicmp(me.szModule, PYTHON_EXE, PYTHON_EXE_LEN))
continue;
if (_wcsnicmp(path, me.szExePath, len))
break;
hp = OpenProcess(dac, FALSE, pe.th32ProcessID);
if (!hp) {
printf("OpenProcess failed: %d\n", GetLastError());
CloseHandle(hsp);
CloseHandle(hsm);
return 1;
}
if (!TerminateProcess(hp, 1)) {
printf("TerminateProcess failed: %d\n", GetLastError());
CloseHandle(hsp);
CloseHandle(hsm);
CloseHandle(hp);
return 1;
}
CloseHandle(hp);
break;
} while (Module32NextW(hsm, &me));
CloseHandle(hsm);
} while (Process32NextW(hsp, &pe));
CloseHandle(hsp);
return 0;
}
