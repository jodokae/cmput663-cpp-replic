#include "mod_proxy.h"
#if !defined(_PROXY_FDPASS_H_)
#define _PROXY_FDPASS_H_
#define PROXY_FDPASS_FLUSHER "proxy_fdpass_flusher"
typedef struct proxy_fdpass_flush proxy_fdpass_flush;
struct proxy_fdpass_flush {
const char *name;
int (*flusher)(request_rec *r);
void *context;
};
#endif
