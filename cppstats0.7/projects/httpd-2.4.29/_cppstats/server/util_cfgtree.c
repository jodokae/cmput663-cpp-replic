#include "util_cfgtree.h"
#include <stdlib.h>
ap_directive_t *ap_add_node(ap_directive_t **parent, ap_directive_t *current,
ap_directive_t *toadd, int child) {
if (current == NULL) {
if (*parent != NULL) {
(*parent)->first_child = toadd;
toadd->parent = *parent;
}
if (child) {
*parent = toadd;
return NULL;
}
return toadd;
}
current->next = toadd;
toadd->parent = *parent;
if (child) {
*parent = toadd;
return NULL;
}
return toadd;
}