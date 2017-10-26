#include <assert.h>
#include <string.h>
#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <stdlib.h>
#include <apr.h>
#include "key-gen.h"
apr_size_t
svn_fs_base__getsize(const char *data, apr_size_t len,
const char **endptr, apr_size_t max) {
apr_size_t max_prefix = max / 10;
apr_size_t max_digit = max % 10;
apr_size_t i;
apr_size_t value = 0;
for (i = 0; i < len && '0' <= data[i] && data[i] <= '9'; i++) {
apr_size_t digit = data[i] - '0';
if (value > max_prefix
|| (value == max_prefix && digit > max_digit)) {
*endptr = 0;
return 0;
}
value = (value * 10) + digit;
}
if (i == 0) {
*endptr = 0;
return 0;
} else {
*endptr = data + i;
return value;
}
}
int
svn_fs_base__putsize(char *data, apr_size_t len, apr_size_t value) {
apr_size_t i = 0;
do {
if (i >= len)
return 0;
data[i] = (value % 10) + '0';
value /= 10;
i++;
} while (value > 0);
{
int left, right;
for (left = 0, right = i-1; left < right; left++, right--) {
char t = data[left];
data[left] = data[right];
data[right] = t;
}
}
return i;
}
void
svn_fs_base__next_key(const char *this, apr_size_t *len, char *next) {
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
svn_fs_base__key_compare(const char *a, const char *b) {
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
svn_boolean_t
svn_fs_base__same_keys(const char *a, const char *b) {
if (! (a || b))
return TRUE;
if (a && (! b))
return FALSE;
if ((! a) && b)
return FALSE;
return (strcmp(a, b) == 0) ? TRUE : FALSE;
}
