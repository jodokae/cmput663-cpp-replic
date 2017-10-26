#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <apr.h>
#include <apr_network_io.h>
#include "private/svn_fs_private.h"
#include "key-gen.h"
#if MAX_KEY_SIZE > SVN_FS__TXN_MAX_LEN
#error The MAX_KEY_SIZE used for BDB txn names is greater than SVN_FS__TXN_MAX_LEN.
#endif
void
svn_fs_fs__add_keys(const char *key1, const char *key2, char *result) {
int i1 = strlen(key1) - 1;
int i2 = strlen(key2) - 1;
int i3 = 0;
int val;
int carry = 0;
char buf[MAX_KEY_SIZE + 2];
while ((i1 >= 0) || (i2 >= 0) || (carry > 0)) {
val = carry;
if (i1>=0)
val += (key1[i1] <= '9') ? (key1[i1] - '0') : (key1[i1] - 'a' + 10);
if (i2>=0)
val += (key2[i2] <= '9') ? (key2[i2] - '0') : (key2[i2] - 'a' + 10);
carry = val / 36;
val = val % 36;
buf[i3++] = (val <= 9) ? (val + '0') : (val - 10 + 'a');
if (i1>=0)
i1--;
if (i2>=0)
i2--;
}
for (i1 = 0; i1 < i3; i1++)
result[i1] = buf[i3 - i1 - 1];
result[i1] = '\0';
}
void
svn_fs_fs__next_key(const char *this, apr_size_t *len, char *next) {
apr_size_t olen = *len;
int i = olen - 1;
char c;
svn_boolean_t carry = TRUE;
if ((*len > 1) && (this[0] == '0')) {
*len = 0;
return;
}
for (i = (olen - 1); i >= 0; i--) {
c = this[i];
if (! (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'z')))) {
*len = 0;
return;
}
if (carry) {
if (c == 'z')
next[i] = '0';
else {
carry = FALSE;
if (c == '9')
next[i] = 'a';
else
next[i] = c + 1;
}
} else
next[i] = c;
}
*len = olen + (carry ? 1 : 0);
assert(*len < MAX_KEY_SIZE);
next[*len] = '\0';
if (carry) {
memmove(next+1, next, olen);
next[0] = '1';
}
}
int
svn_fs_fs__key_compare(const char *a, const char *b) {
int a_len = strlen(a);
int b_len = strlen(b);
int cmp;
if (a_len > b_len)
return 1;
if (b_len > a_len)
return -1;
cmp = strcmp(a, b);
return (cmp ? (cmp / abs(cmp)) : 0);
}
