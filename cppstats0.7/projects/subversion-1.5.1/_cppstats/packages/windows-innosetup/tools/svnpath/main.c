#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <io.h>
#include <sys\stat.h>
#define BUFSIZE 4000
static char g_AuExBatFile[17] = "C:\\Autoexec.bat";
static char g_AuExSvnFile[17] = "C:\\Autoexec.svn";
char g_cSvnLineRem1[80];
char g_cSvnLineRem2[80];
char g_cSvnLinePath[256];
int svn_add9x (char cPath[255]);
int svn_addnt (char cPath[BUFSIZE]);
void svn_error_msg(char cMsg[255]);
int svn_os_is_nt();
int svn_print_help();
int svn_read_regval (HKEY hKey, char cValue[10], char cKey[BUFSIZE],
char *pcPathCur[BUFSIZE], DWORD *lpType);
int svn_remove9x (char cPath[255]);
int svn_removent (char cPath[255]);
int svn_run_cmd (char cAction[10], char cPath[255]);
int svn_set_auexlines (char cPath[255]);
int svn_svnpath_exists (char cPath[255]);
int
main (int argc, char *argv[]) {
int counter=0, iCmdArgError=1, iRetVal=1;
char cMsg[150];
switch (argc) {
case 1:
lstrcpy ( cMsg, "Missing arguments.");
svn_error_msg(cMsg);
iRetVal = 65;
iCmdArgError=0;
break;
case 2:
if (! strcmp(argv[1], "--help") || ! strcmp(argv[1], "-h")) {
iRetVal=svn_print_help();
iCmdArgError=0;
}
break;
case 3:
if (! strcmp(argv[1], "add") || ! strcmp(argv[1], "remove")) {
iRetVal=svn_run_cmd(argv[1], argv[2]);
iCmdArgError=0;
}
break;
default:
iRetVal = 1;
}
if (iCmdArgError) {
lstrcpy ( cMsg, "Argument Error: Wrong arguments\n\n");
lstrcat ( cMsg, "This program received the following arguments:");
for (counter=1; counter<argc; counter++) {
lstrcat ( cMsg, "\n '");
lstrcat ( cMsg, argv[counter]);
lstrcat ( cMsg, "'");
}
if ((!strcmp(argv[1], "add") || !strcmp(argv[1], "remove")) && (argc > 3)) {
iRetVal=svn_run_cmd(argv[1], argv[2]);
iCmdArgError=0;
} else {
svn_error_msg(cMsg);
iRetVal = 1;
}
}
return (iRetVal);
}
int
svn_add9x (char cPath[255]) {
char cSvnCnt[1024];
int iAutoBatRo=0;
FILE *FH_AUBAT;
svn_set_auexlines(cPath);
lstrcpy (cSvnCnt, g_cSvnLineRem1);
lstrcat (cSvnCnt, g_cSvnLineRem2);
lstrcat (cSvnCnt, g_cSvnLinePath);
if( _access(g_AuExBatFile, 0 ) != -1) {
if((_access(g_AuExBatFile, 2)) == -1) {
_chmod(g_AuExBatFile, _S_IWRITE);
iAutoBatRo=1;
}
CopyFileA(g_AuExBatFile, g_AuExSvnFile, FALSE);
}
FH_AUBAT = fopen(g_AuExBatFile, "a+t");
fputs(cSvnCnt, FH_AUBAT);
fclose(FH_AUBAT);
if (iAutoBatRo) {
_chmod(g_AuExBatFile, _S_IREAD);
}
return 0;
}
int
svn_addnt (char cPathSvn[255]) {
long lRet;
char cPathTmp[BUFSIZE];
HKEY hKey;
char cKey[BUFSIZE], cPathNew[BUFSIZE], cPathCur[BUFSIZE];
DWORD dwBufLen, lpType;
char *pcPathCur[BUFSIZE];
dwBufLen=BUFSIZE;
*pcPathCur=cPathCur;
lstrcpy (cPathTmp, cPathSvn);
if (svn_svnpath_exists(cPathTmp)) {
exit (1);
}
lstrcpy(cKey, "SYSTEM\\CurrentControlSet\\");
lstrcat(cKey, "Control\\Session Manager\\Environment");
svn_read_regval (HKEY_LOCAL_MACHINE, "Path", cKey, &*pcPathCur, &lpType);
lRet = RegCreateKeyEx(
HKEY_LOCAL_MACHINE, cKey, 0, NULL,
REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
&hKey, &dwBufLen);
lstrcpy(cPathNew, cPathCur);
lstrcat(cPathNew, ";");
lstrcat(cPathNew, cPathSvn);
lRet = RegSetValueExA(hKey, "Path", 0, lpType,
(BYTE*)cPathNew, strlen(cPathNew)+1);
RegCloseKey(hKey);
if (lRet != 0) {
strcpy (cPathCur, "");
lRet = svn_read_regval(HKEY_CURRENT_USER, "Path",
"Environment", &*pcPathCur, &lpType);
cPathNew[0] = 0;
if (strlen(cPathCur)) {
lstrcpy(cPathNew, cPathCur);
lstrcat(cPathNew, ";");
} else
lpType = REG_EXPAND_SZ;
lstrcat(cPathNew, cPathSvn);
lRet = RegCreateKeyEx(
HKEY_CURRENT_USER, "Environment", 0, NULL,
REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
&hKey, &dwBufLen);
lRet = RegSetValueExA(hKey, "Path", 0, lpType,
(LPBYTE)cPathNew, strlen(cPathNew)+1);
RegCloseKey(hKey);
}
if (lRet != 0) {
return (1);
} else {
long lRet;
SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
(LPARAM) "Environment", SMTO_ABORTIFHUNG,
5000, &lRet);
return (0);
}
}
void
svn_error_msg(char cMsg[150]) {
long lRet;
long lMsgBoxFlag=MB_YESNO+MB_ICONWARNING+MB_SETFOREGROUND+MB_TOPMOST;
lstrcat(cMsg, "\n\nDo you want to read the help for svnpath?");
lRet=MessageBox(0, cMsg, "svnpath - Error" , lMsgBoxFlag);
if (lRet==IDYES) {
svn_print_help();
}
}
int
svn_os_is_nt() {
int iRetVal=0;
OSVERSIONINFO osvi;
BOOL bOsVersionInfoEx;
ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) ) {
osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) ) {
exit (1);
}
}
if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
iRetVal=1;
}
return (iRetVal);
}
int
svn_print_help() {
char cMsgBoxCaption[80];
char cMsgBoxMsg[1024];
long lMsgBoxFlag=MB_OK+MB_ICONINFORMATION+MB_SETFOREGROUND;
lstrcpy(cMsgBoxCaption, "Help for svnpath");
lstrcpy(cMsgBoxMsg, "svnpath - Add/remove a path on the system's PATH environment variable\n\n");
lstrcat(cMsgBoxMsg, "usage:\tsvnpath add|remove \"Path\"\n");
lstrcat(cMsgBoxMsg, "\tsvnpath -h|--help\n\n");
lstrcat(cMsgBoxMsg, "Example:\tsvnpath add \"C:\\Path\\to\\svn.exe\"\n\n");
lstrcat(cMsgBoxMsg, "Command explanations:\n");
lstrcat(cMsgBoxMsg, " add <path>\n");
lstrcat(cMsgBoxMsg, " Adding the path to the system's PATH environment variable\n");
lstrcat(cMsgBoxMsg, " remove <path>,\n");
lstrcat(cMsgBoxMsg, " Removing the path from the system's PATH environment ");
lstrcat(cMsgBoxMsg, "variable\n\n");
lstrcat(cMsgBoxMsg, " * On the Windows 9x variations, the Autoexec.bat file are ");
lstrcat(cMsgBoxMsg, "edited\n");
lstrcat(cMsgBoxMsg, " * On the Windows NT variations, the registry are edited. The ");
lstrcat(cMsgBoxMsg, "program tries\n");
lstrcat(cMsgBoxMsg, " to edit the Environment in HKLM first. If that fails, then ");
lstrcat(cMsgBoxMsg, "the Environment\n in HKCU are used.\n\n");
lstrcat(cMsgBoxMsg, " -h, --help: Print help (this page)\n\n");
lstrcat(cMsgBoxMsg, "Notes:\n");
lstrcat(cMsgBoxMsg, " * For playing safe: -Make sure that the given path allways is ");
lstrcat(cMsgBoxMsg, "quoted between\n");
lstrcat(cMsgBoxMsg, " two \"'s wherewer the path contains spaces or not\n");
MessageBox(0,cMsgBoxMsg, cMsgBoxCaption , lMsgBoxFlag);
return 0;
}
int
svn_read_regval (HKEY hKey, char cValue[10], char cKey[BUFSIZE],
char *pcPathCur[BUFSIZE], DWORD *lpType) {
long lRet;
DWORD dwBufLen;
dwBufLen=BUFSIZE;
lRet = RegOpenKeyExA(hKey, cKey,
0, KEY_READ, &hKey );
lRet = RegQueryValueExA(hKey, cValue, NULL, &*lpType,
(LPBYTE) &**pcPathCur, &dwBufLen);
RegCloseKey(hKey);
if (lRet != 0) {
return (1);
} else {
return (0);
}
}
int
svn_remove9x (char cPath[255]) {
char cPathTmp[255];
FILE *FH_AUBAT, *FH_AUSVN;
char cLineBuffer[255];
char cSvnLineBuffer[255];
int iCounter=0;
int iAutoBatRo=0;
lstrcpy (cPathTmp, cPath);
if (! svn_svnpath_exists(cPathTmp)) {
exit(1);
}
if(_access(g_AuExBatFile, 0) != -1) {
if((_access(g_AuExBatFile, 2 )) == -1) {
_chmod(g_AuExBatFile, _S_IWRITE);
iAutoBatRo=1;
}
CopyFileA(g_AuExBatFile, g_AuExSvnFile, FALSE);
}
FH_AUSVN=fopen(g_AuExSvnFile, "rt");
FH_AUBAT=fopen(g_AuExBatFile, "wt");
svn_set_auexlines(cPath);
lstrcpy (cSvnLineBuffer, g_cSvnLineRem1);
while(fgets(cLineBuffer, 255, FH_AUSVN) != NULL) {
if (strstr (cLineBuffer, cSvnLineBuffer) == NULL) {
fputs(cLineBuffer, FH_AUBAT);
} else {
iCounter++;
switch (iCounter) {
case 1:
lstrcpy (cSvnLineBuffer, g_cSvnLineRem2);
break;
case 2:
lstrcpy (cSvnLineBuffer, g_cSvnLinePath);
break;
}
}
}
fclose(FH_AUSVN);
fclose(FH_AUBAT);
if (iAutoBatRo) {
_chmod(g_AuExBatFile, _S_IREAD);
}
return 0;
}
int
svn_removent (char cPathSvn[255]) {
long lRet;
char cPathTmp[BUFSIZE];
HKEY hKey;
char cKey[BUFSIZE], cPathNew[BUFSIZE], cPathCur[BUFSIZE];
DWORD dwBufLen, lpType;
char *pcPathCur[BUFSIZE];
char * pcSubPath;
*pcPathCur=cPathCur;
dwBufLen=BUFSIZE;
lstrcpy (cPathTmp, cPathSvn);
if (! svn_svnpath_exists(cPathTmp)) {
exit (1);
}
lstrcpy(cKey, "SYSTEM\\CurrentControlSet\\");
lstrcat(cKey, "Control\\Session Manager\\Environment");
lRet = svn_read_regval(HKEY_LOCAL_MACHINE, "Path",
cKey, &*pcPathCur, &lpType);
lRet = RegCreateKeyEx(
HKEY_LOCAL_MACHINE, cKey, 0, NULL,
REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
&hKey, &dwBufLen);
pcSubPath = strtok (cPathCur,";");
strcpy(cPathNew, "");
while (pcSubPath != NULL) {
if (strcmp(pcSubPath, cPathSvn)) {
if (strlen(cPathNew)==0) {
lstrcpy(cPathNew, pcSubPath);
} else {
lstrcat(cPathNew, ";");
lstrcat(cPathNew, pcSubPath);
}
}
pcSubPath = strtok (NULL, ";");
}
lRet = RegSetValueExA(hKey, "Path", 0, lpType,
(BYTE*)cPathNew, strlen(cPathNew)+1);
RegCloseKey(hKey);
if (lRet != 0) {
strcpy(cPathCur, "");
lRet = svn_read_regval(HKEY_CURRENT_USER, "Path", "Environment",
&*pcPathCur, &lpType);
pcSubPath = strtok (cPathCur,";");
strcpy(cPathNew, "");
while (pcSubPath != NULL) {
if (strcmp(pcSubPath, cPathSvn)) {
if (strlen(cPathNew)==0) {
lstrcpy(cPathNew, pcSubPath);
} else {
lstrcat(cPathNew, ";");
lstrcat(cPathNew, pcSubPath);
}
}
pcSubPath = strtok (NULL, ";");
}
lRet = RegCreateKeyEx(
HKEY_CURRENT_USER, "Environment", 0, NULL,
REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
&hKey, &dwBufLen);
lRet = RegSetValueExA(hKey, "Path", 0, lpType,
(LPBYTE)cPathNew, strlen(cPathNew)+1);
if (lRet != 0) {
return (1);
}
RegCloseKey(hKey);
}
if (lRet != 0) {
return (lRet);
} else {
SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
(LPARAM) "Environment", SMTO_ABORTIFHUNG,
5000, &lRet);
}
return (0);
}
int
svn_run_cmd (char cAction[10], char cPath[255]) {
int iRetVal=1;
if (svn_os_is_nt()) {
if (! strcmp(cAction, "add")) {
iRetVal=svn_addnt(cPath);
} else if (! strcmp(cAction, "remove")) {
iRetVal=svn_removent(cPath);
}
} else {
if (! strcmp(cAction, "add")) {
iRetVal=svn_add9x(cPath);
} else if (! strcmp(cAction, "remove")) {
iRetVal=svn_remove9x(cPath);
}
}
return (iRetVal);
}
int
svn_set_auexlines (char cPath[255]) {
lstrcpy (g_cSvnLineRem1, "REM *** For Subversion: ");
lstrcat (g_cSvnLineRem1, "Don't touch this and the two next lines ***\n");
lstrcpy (g_cSvnLineRem2, "REM *** They will be removed when Subversion is ");
lstrcat (g_cSvnLineRem2, "uninstalled ***\n");
lstrcat (g_cSvnLinePath, "PATH=%PATH%;\"");
lstrcat (g_cSvnLinePath, cPath);
lstrcat (g_cSvnLinePath, "\"\n");
return 0;
}
int
svn_svnpath_exists (char cPath[255]) {
char cSysPath[1024];
DWORD dwLenPath;
int iRetVal=0;
char * pcSubPath;
dwLenPath = GetEnvironmentVariable("PATH", cSysPath, 1024);
if (dwLenPath) {
pcSubPath = strtok (cSysPath,";");
while (pcSubPath != NULL) {
if (! strcmp(strupr(pcSubPath), strupr(cPath)) &&
strlen(pcSubPath) == strlen(cPath)) {
iRetVal = 1;
break;
}
pcSubPath = strtok (NULL, ";");
}
} else {
exit (1);
}
return iRetVal;
}