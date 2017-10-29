#if !defined(HEARTBEAT_H)
#define HEARTBEAT_H
#include "apr.h"
#include "apr_time.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define MAXIPSIZE 46
typedef struct hm_slot_server_t {
char ip[MAXIPSIZE];
int busy;
int ready;
apr_time_t seen;
int id;
} hm_slot_server_t;
#define DEFAULT_HEARTBEAT_STORAGE "hb.dat"
#if defined(__cplusplus)
}
#endif
#endif