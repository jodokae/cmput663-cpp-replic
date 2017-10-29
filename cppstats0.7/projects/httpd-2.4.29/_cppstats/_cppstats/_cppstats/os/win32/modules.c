#include "httpd.h"
#include "http_config.h"
extern module core_module;
extern module win32_module;
extern module mpm_winnt_module;
extern module http_module;
extern module so_module;
AP_DECLARE_DATA module *ap_prelinked_modules[] = {
&core_module,
&win32_module,
&mpm_winnt_module,
&http_module,
&so_module,
NULL
};
ap_module_symbol_t ap_prelinked_module_symbols[] = {
{"core_module", &core_module},
{"win32_module", &win32_module},
{"mpm_winnt_module", &mpm_winnt_module},
{"http_module", &http_module},
{"so_module", &so_module},
{NULL, NULL}
};
AP_DECLARE_DATA module *ap_preloaded_modules[] = {
&core_module,
&win32_module,
&mpm_winnt_module,
&http_module,
&so_module,
NULL
};
