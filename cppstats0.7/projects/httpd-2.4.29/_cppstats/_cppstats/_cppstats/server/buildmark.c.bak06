#include "ap_config.h"
#include "httpd.h"
#if defined(__DATE__) && defined(__TIME__)
static const char server_built[] = __DATE__ " " __TIME__;
#else
static const char server_built[] = "unknown";
#endif
AP_DECLARE(const char *) ap_get_server_built() {
return server_built;
}
