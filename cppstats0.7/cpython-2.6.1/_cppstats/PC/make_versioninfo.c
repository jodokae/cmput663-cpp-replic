#include <stdio.h>
#include "patchlevel.h"
int main(int argc, char **argv) {
printf("/* This file created by make_versioninfo.exe */\n");
printf("#define FIELD3 %d\n",
PY_MICRO_VERSION*1000 + PY_RELEASE_LEVEL*10 + PY_RELEASE_SERIAL);
printf("#define MS_DLL_ID \"%d.%d\"\n",
PY_MAJOR_VERSION, PY_MINOR_VERSION);
printf("#ifndef _DEBUG\n");
printf("#define PYTHON_DLL_NAME \"python%d%d.dll\"\n",
PY_MAJOR_VERSION, PY_MINOR_VERSION);
printf("#else\n");
printf("#define PYTHON_DLL_NAME \"python%d%d_d.dll\"\n",
PY_MAJOR_VERSION, PY_MINOR_VERSION);
printf("#endif\n");
return 0;
}
