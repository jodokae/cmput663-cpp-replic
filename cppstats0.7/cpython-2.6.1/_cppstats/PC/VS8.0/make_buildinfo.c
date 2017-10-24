#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#define CMD_SIZE 500
int make_buildinfo2() {
struct _stat st;
HKEY hTortoise;
char command[CMD_SIZE+1];
DWORD type, size;
if (_stat(".svn", &st) < 0)
return 0;
if (_stat("no_subwcrev", &st) == 0)
return 0;
if (RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\TortoiseSVN", &hTortoise) != ERROR_SUCCESS &&
RegOpenKey(HKEY_CURRENT_USER, "Software\\TortoiseSVN", &hTortoise) != ERROR_SUCCESS)
return 0;
command[0] = '"';
size = sizeof(command) - 1;
if (RegQueryValueEx(hTortoise, "Directory", 0, &type, command+1, &size) != ERROR_SUCCESS ||
type != REG_SZ)
return 0;
strcat_s(command, CMD_SIZE, "bin\\subwcrev.exe");
if (_stat(command+1, &st) < 0)
return 0;
strcat_s(command, CMD_SIZE, "\" ..\\.. ..\\..\\Modules\\getbuildinfo.c getbuildinfo2.c");
puts(command);
fflush(stdout);
if (system(command) < 0)
return 0;
return 1;
}
int main(int argc, char*argv[]) {
char command[500] = "cl.exe -c -D_WIN32 -DUSE_DL_EXPORT -D_WINDOWS -DWIN32 -D_WINDLL ";
int do_unlink, result;
if (argc != 2) {
fprintf(stderr, "make_buildinfo $(ConfigurationName)\n");
return EXIT_FAILURE;
}
if (strcmp(argv[1], "Release") == 0) {
strcat_s(command, CMD_SIZE, "-MD ");
} else if (strcmp(argv[1], "Debug") == 0) {
strcat_s(command, CMD_SIZE, "-D_DEBUG -MDd ");
} else if (strcmp(argv[1], "ReleaseItanium") == 0) {
strcat_s(command, CMD_SIZE, "-MD /USECL:MS_ITANIUM ");
} else if (strcmp(argv[1], "ReleaseAMD64") == 0) {
strcat_s(command, CMD_SIZE, "-MD ");
strcat_s(command, CMD_SIZE, "-MD /USECL:MS_OPTERON ");
} else {
fprintf(stderr, "unsupported configuration %s\n", argv[1]);
return EXIT_FAILURE;
}
if ((do_unlink = make_buildinfo2()))
strcat_s(command, CMD_SIZE, "getbuildinfo2.c -DSUBWCREV ");
else
strcat_s(command, CMD_SIZE, "..\\..\\Modules\\getbuildinfo.c");
strcat_s(command, CMD_SIZE, " -Fogetbuildinfo.o -I..\\..\\Include -I..\\..\\PC");
puts(command);
fflush(stdout);
result = system(command);
if (do_unlink)
_unlink("getbuildinfo2.c");
if (result < 0)
return EXIT_FAILURE;
return 0;
}
