#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSSESMGR
#define INCL_WINPROGRAMLIST
#define INCL_WINFRAMEMGR
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
typedef struct _track_rec {
char *name;
HMODULE handle;
void *id;
struct _track_rec *next;
} tDLLchain, *DLLchain;
static DLLchain dlload = NULL;
static char dlerr [256];
static void *last_id;
static DLLchain find_id(void *id) {
DLLchain tmp;
for (tmp = dlload; tmp; tmp = tmp->next)
if (id == tmp->id)
return tmp;
return NULL;
}
void *dlopen(char *filename, int flags) {
HMODULE hm;
DLLchain tmp;
char err[256];
char *errtxt;
int rc = 0, set_chain = 0;
for (tmp = dlload; tmp; tmp = tmp->next)
if (strnicmp(tmp->name, filename, 999) == 0)
break;
if (!tmp) {
tmp = (DLLchain) malloc(sizeof(tDLLchain));
if (!tmp)
goto nomem;
tmp->name = strdup(filename);
tmp->next = dlload;
set_chain = 1;
}
switch (rc = DosLoadModule((PSZ)&err, sizeof(err), filename, &hm)) {
case NO_ERROR:
tmp->handle = hm;
if (set_chain) {
do
last_id++;
while ((last_id == 0) || (find_id(last_id)));
tmp->id = last_id;
dlload = tmp;
}
return tmp->id;
case ERROR_FILE_NOT_FOUND:
case ERROR_PATH_NOT_FOUND:
errtxt = "module `%s' not found";
break;
case ERROR_TOO_MANY_OPEN_FILES:
case ERROR_NOT_ENOUGH_MEMORY:
case ERROR_SHARING_BUFFER_EXCEEDED:
nomem:
errtxt = "out of system resources";
break;
case ERROR_ACCESS_DENIED:
errtxt = "access denied";
break;
case ERROR_BAD_FORMAT:
case ERROR_INVALID_SEGMENT_NUMBER:
case ERROR_INVALID_ORDINAL:
case ERROR_INVALID_MODULETYPE:
case ERROR_INVALID_EXE_SIGNATURE:
case ERROR_EXE_MARKED_INVALID:
case ERROR_ITERATED_DATA_EXCEEDS_64K:
case ERROR_INVALID_MINALLOCSIZE:
case ERROR_INVALID_SEGDPL:
case ERROR_AUTODATASEG_EXCEEDS_64K:
case ERROR_RELOCSRC_CHAIN_EXCEEDS_SEGLIMIT:
errtxt = "invalid module format";
break;
case ERROR_INVALID_NAME:
errtxt = "filename doesn't match module name";
break;
case ERROR_SHARING_VIOLATION:
case ERROR_LOCK_VIOLATION:
errtxt = "sharing violation";
break;
case ERROR_INIT_ROUTINE_FAILED:
errtxt = "module initialization failed";
break;
default:
errtxt = "cause `%s', error code = %d";
break;
}
snprintf(dlerr, sizeof(dlerr), errtxt, &err, rc);
if (tmp) {
if (tmp->name)
free(tmp->name);
free(tmp);
}
return 0;
}
void *dlsym(void *handle, char *symbol) {
int rc = 0;
PFN addr;
char *errtxt;
int symord = 0;
DLLchain tmp = find_id(handle);
if (!tmp)
goto inv_handle;
if (*symbol == '#')
symord = atoi(symbol + 1);
switch (rc = DosQueryProcAddr(tmp->handle, symord, symbol, &addr)) {
case NO_ERROR:
return (void *)addr;
case ERROR_INVALID_HANDLE:
inv_handle:
errtxt = "invalid module handle";
break;
case ERROR_PROC_NOT_FOUND:
case ERROR_INVALID_NAME:
errtxt = "no symbol `%s' in module";
break;
default:
errtxt = "symbol `%s', error code = %d";
break;
}
snprintf(dlerr, sizeof(dlerr), errtxt, symbol, rc);
return NULL;
}
int dlclose(void *handle) {
int rc;
DLLchain tmp = find_id(handle);
if (!tmp)
goto inv_handle;
switch (rc = DosFreeModule(tmp->handle)) {
case NO_ERROR:
free(tmp->name);
dlload = tmp->next;
free(tmp);
return 0;
case ERROR_INVALID_HANDLE:
inv_handle:
strcpy(dlerr, "invalid module handle");
return -1;
case ERROR_INVALID_ACCESS:
strcpy(dlerr, "access denied");
return -1;
default:
return -1;
}
}
char *dlerror() {
return dlerr;
}