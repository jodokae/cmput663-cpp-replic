#if !defined(AP_CONFTREE_H)
#define AP_CONFTREE_H
#include "ap_config.h"
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct ap_directive_t ap_directive_t;
struct ap_directive_t {
const char *directive;
const char *args;
struct ap_directive_t *next;
struct ap_directive_t *first_child;
struct ap_directive_t *parent;
void *data;
const char *filename;
int line_num;
struct ap_directive_t *last;
};
AP_DECLARE_DATA extern ap_directive_t *ap_conftree;
ap_directive_t *ap_add_node(ap_directive_t **parent, ap_directive_t *current,
ap_directive_t *toadd, int child);
#if defined(__cplusplus)
}
#endif
#endif