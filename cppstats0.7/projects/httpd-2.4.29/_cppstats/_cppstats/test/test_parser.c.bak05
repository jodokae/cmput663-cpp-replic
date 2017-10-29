#include <stdio.h>
#include <stdlib.h>
#include "httpd.h"
#include "apr_general.h"
uid_t ap_user_id;
gid_t ap_group_id;
void *ap_dummy_mutex = &ap_dummy_mutex;
char *ap_server_argv0;
AP_DECLARE(void) ap_block_alarms(void) {
;
}
AP_DECLARE(void) ap_unblock_alarms(void) {
;
}
AP_DECLARE(void) ap_log_error(const char *file, int line, int level,
const request_rec *r, const char *fmt, ...) {
;
}
int main (void) {
apr_pool_t *p;
const char *field;
char *newstr;
char instr[512];
p = apr_pool_alloc_init();
while (gets(instr)) {
printf(" [%s] ==\n", instr);
field = instr;
while ((newstr = ap_get_list_item(p, &field)) != NULL)
printf(" <%s> ..\n", newstr);
}
exit(0);
}
