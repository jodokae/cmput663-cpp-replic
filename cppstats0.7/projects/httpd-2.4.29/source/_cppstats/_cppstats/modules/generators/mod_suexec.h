#include "unixd.h"
typedef struct {
ap_unix_identity_t ugid;
int active;
} suexec_config_t;