#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#include <process.h>
#include "Python.h"
typedef struct {
int argc;
char **argv;
HWND Frame;
int running;
} arglist;
int PythonRC;
extern DL_EXPORT(int) Py_Main(int, char **);
void PythonThread(void *);
int
main(int argc, char **argv) {
ULONG FrameFlags = FCF_TITLEBAR |
FCF_SYSMENU |
FCF_SIZEBORDER |
FCF_HIDEBUTTON |
FCF_SHELLPOSITION |
FCF_TASKLIST;
HAB hab;
HMQ hmq;
HWND Client;
QMSG qmsg;
arglist args;
int python_tid;
hab = WinInitialize(0);
hmq = WinCreateMsgQueue(hab, 0);
args.Frame = WinCreateStdWindow(HWND_DESKTOP,
0,
&FrameFlags,
NULL,
"PythonPM",
0L,
0,
0,
&Client);
args.argc = argc;
args.argv = argv;
args.running = 0;
if (-1 == (python_tid = _beginthread(PythonThread, NULL, 1024 * 1024, &args))) {
WinAlarm(HWND_DESKTOP, WA_ERROR);
PythonRC = 1;
} else {
while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
WinDispatchMsg(hab, &qmsg);
if (args.running > 0)
DosKillThread(python_tid);
}
WinDestroyWindow(args.Frame);
WinDestroyMsgQueue(hmq);
WinTerminate(hab);
return PythonRC;
}
void PythonThread(void *argl) {
HAB hab;
arglist *args;
hab = WinInitialize(0);
args = (arglist *)argl;
args->running = 1;
PythonRC = Py_Main(args->argc, args->argv);
DosEnterCritSec();
args->running = 0;
WinPostMsg(args->Frame, WM_QUIT, NULL, NULL);
WinTerminate(hab);
_endthread();
}