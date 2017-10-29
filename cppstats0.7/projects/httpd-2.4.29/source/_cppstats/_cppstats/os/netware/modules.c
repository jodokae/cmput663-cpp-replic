#include "httpd.h"
#include "http_config.h"
extern module core_module;
extern module mpm_netware_module;
extern module http_module;
extern module so_module;
extern module mime_module;
extern module authn_core_module;
extern module authz_core_module;
extern module authz_host_module;
extern module negotiation_module;
extern module include_module;
extern module dir_module;
extern module alias_module;
extern module env_module;
extern module log_config_module;
extern module setenvif_module;
extern module watchdog_module;
#if defined(USE_WINSOCK)
extern module nwssl_module;
#endif
extern module netware_module;
module *ap_prelinked_modules[] = {
&core_module,
&mpm_netware_module,
&http_module,
&so_module,
&mime_module,
&authn_core_module,
&authz_core_module,
&authz_host_module,
&negotiation_module,
&include_module,
&dir_module,
&alias_module,
&env_module,
&log_config_module,
&setenvif_module,
&watchdog_module,
#if defined(USE_WINSOCK)
&nwssl_module,
#endif
&netware_module,
NULL
};
ap_module_symbol_t ap_prelinked_module_symbols[] = {
{"core_module", &core_module},
{"mpm_netware_module", &mpm_netware_module},
{"http_module", &http_module},
{"so_module", &so_module},
{"mime_module", &mime_module},
{"authn_core_module", &authn_core_module},
{"authz_core_module", &authz_core_module},
{"authz_host_module", &authz_host_module},
{"negotiation_module", &negotiation_module},
{"include_module", &include_module},
{"dir_module", &dir_module},
{"alias_module", &alias_module},
{"env_module", &env_module},
{"log_config_module", &log_config_module},
{"setenvif_module", &setenvif_module},
{"watchdog module", &watchdog_module},
#if defined(USE_WINSOCK)
{"nwssl_module", &nwssl_module},
#endif
{"netware_module", &netware_module},
{NULL, NULL}
};
module *ap_preloaded_modules[] = {
&core_module,
&mpm_netware_module,
&http_module,
&so_module,
&mime_module,
&authn_core_module,
&authz_core_module,
&authz_host_module,
&negotiation_module,
&include_module,
&dir_module,
&alias_module,
&env_module,
&log_config_module,
&setenvif_module,
&watchdog_module,
#if defined(USE_WINSOCK)
&nwssl_module,
#endif
&netware_module,
NULL
};
