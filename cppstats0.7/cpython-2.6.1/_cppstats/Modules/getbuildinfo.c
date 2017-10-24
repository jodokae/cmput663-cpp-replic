#include "Python.h"
#if !defined(DONT_HAVE_STDIO_H)
#include <stdio.h>
#endif
#if !defined(DATE)
#if defined(__DATE__)
#define DATE __DATE__
#else
#define DATE "xx/xx/xx"
#endif
#endif
#if !defined(TIME)
#if defined(__TIME__)
#define TIME __TIME__
#else
#define TIME "xx:xx:xx"
#endif
#endif
#if !defined(SVNVERSION)
#define SVNVERSION "$WCRANGE$$WCMODS?M:$"
#endif
const char *
Py_GetBuildInfo(void) {
static char buildinfo[50];
const char *revision = Py_SubversionRevision();
const char *sep = *revision ? ":" : "";
const char *branch = Py_SubversionShortBranch();
PyOS_snprintf(buildinfo, sizeof(buildinfo),
"%s%s%s, %.20s, %.9s", branch, sep, revision,
DATE, TIME);
return buildinfo;
}
const char *
_Py_svnversion(void) {
static const char svnversion[] = SVNVERSION;
if (svnversion[0] != '$')
return svnversion;
return "exported";
}