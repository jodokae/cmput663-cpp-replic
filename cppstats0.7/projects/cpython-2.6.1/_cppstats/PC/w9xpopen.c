#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
const char *usage =
"This program is used by Python's os.popen function\n"
"to work around a limitation in Windows 95/98. It is\n"
"not designed to be used as a stand-alone program.";
int main(int argc, char *argv[]) {
BOOL bRet;
STARTUPINFO si;
PROCESS_INFORMATION pi;
DWORD exit_code=0;
size_t cmdlen = 0;
int i;
char *cmdline, *cmdlinefill;
if (argc < 2) {
if (GetFileType(GetStdHandle(STD_INPUT_HANDLE))==FILE_TYPE_CHAR)
MessageBox(NULL, usage, argv[0], MB_OK);
else {
fprintf(stdout, "Internal popen error - no args specified\n%s\n", usage);
}
return 1;
}
for (i=1; i<argc; i++)
cmdlen += strlen(argv[i])*2 + 3;
cmdline = cmdlinefill = (char *)malloc(cmdlen+1);
if (cmdline == NULL)
return -1;
for (i=1; i<argc; i++) {
const char *arglook;
int bQuote = strchr(argv[i], ' ') != NULL;
if (bQuote)
*cmdlinefill++ = '"';
for (arglook=argv[i]; *arglook; arglook++) {
if (*arglook=='"')
*cmdlinefill++ = '\\';
*cmdlinefill++ = *arglook;
}
if (bQuote)
*cmdlinefill++ = '"';
*cmdlinefill++ = ' ';
}
*cmdlinefill = '\0';
ZeroMemory(&si, sizeof si);
si.cb = sizeof si;
si.dwFlags = STARTF_USESTDHANDLES;
si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
bRet = CreateProcess(
NULL, cmdline,
NULL, NULL,
TRUE, 0,
NULL, NULL,
&si, &pi
);
free(cmdline);
if (bRet) {
if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_FAILED) {
GetExitCodeProcess(pi.hProcess, &exit_code);
}
CloseHandle(pi.hProcess);
CloseHandle(pi.hThread);
return exit_code;
}
return 1;
}