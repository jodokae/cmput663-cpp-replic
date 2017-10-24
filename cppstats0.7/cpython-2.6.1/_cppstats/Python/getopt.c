#include <stdio.h>
#include <string.h>
#if defined(__cplusplus)
extern "C" {
#endif
int _PyOS_opterr = 1;
int _PyOS_optind = 1;
char *_PyOS_optarg = NULL;
int _PyOS_GetOpt(int argc, char **argv, char *optstring) {
static char *opt_ptr = "";
char *ptr;
int option;
if (*opt_ptr == '\0') {
if (_PyOS_optind >= argc)
return -1;
#if defined(MS_WINDOWS)
else if (strcmp(argv[_PyOS_optind], "/?") == 0) {
++_PyOS_optind;
return 'h';
}
#endif
else if (argv[_PyOS_optind][0] != '-' ||
argv[_PyOS_optind][1] == '\0' )
return -1;
else if (strcmp(argv[_PyOS_optind], "--") == 0) {
++_PyOS_optind;
return -1;
}
else if (strcmp(argv[_PyOS_optind], "--help") == 0) {
++_PyOS_optind;
return 'h';
}
else if (strcmp(argv[_PyOS_optind], "--version") == 0) {
++_PyOS_optind;
return 'V';
}
opt_ptr = &argv[_PyOS_optind++][1];
}
if ( (option = *opt_ptr++) == '\0')
return -1;
if (option == 'J') {
fprintf(stderr, "-J is reserved for Jython\n");
return '_';
}
if (option == 'X') {
fprintf(stderr,
"-X is reserved for implementation-specific arguments\n");
return '_';
}
if ((ptr = strchr(optstring, option)) == NULL) {
if (_PyOS_opterr)
fprintf(stderr, "Unknown option: -%c\n", option);
return '_';
}
if (*(ptr + 1) == ':') {
if (*opt_ptr != '\0') {
_PyOS_optarg = opt_ptr;
opt_ptr = "";
}
else {
if (_PyOS_optind >= argc) {
if (_PyOS_opterr)
fprintf(stderr,
"Argument expected for the -%c option\n", option);
return '_';
}
_PyOS_optarg = argv[_PyOS_optind++];
}
}
return option;
}
#if defined(__cplusplus)
}
#endif
