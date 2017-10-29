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
char line[512];
char tok[512];
p = apr_pool_alloc_init();
printf("Enter field value to find items within:\n");
if (!gets(line))
exit(0);
printf("Enter search item:\n");
while (gets(tok)) {
printf(" [%s] == %s\n", tok, ap_find_list_item(p, line, tok)
? "Yes" : "No");
printf("Enter search item:\n");
}
exit(0);
}
