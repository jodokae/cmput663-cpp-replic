#if !defined(STRINGLIB_LOCALEUTIL_H)
#define STRINGLIB_LOCALEUTIL_H
#include <locale.h>
int
_Py_InsertThousandsGrouping(STRINGLIB_CHAR *buffer,
Py_ssize_t n_buffer,
Py_ssize_t n_digits,
Py_ssize_t buf_size,
Py_ssize_t *count,
int append_zero_char) {
struct lconv *locale_data = localeconv();
const char *grouping = locale_data->grouping;
const char *thousands_sep = locale_data->thousands_sep;
Py_ssize_t thousands_sep_len = strlen(thousands_sep);
STRINGLIB_CHAR *pend = NULL;
STRINGLIB_CHAR *pmax = NULL;
char current_grouping;
Py_ssize_t remaining = n_digits;
if (count)
*count = 0;
else {
pend = buffer + n_buffer;
pmax = buffer + buf_size;
}
current_grouping = *grouping++;
if (current_grouping == 0)
return 1;
while (remaining > current_grouping) {
remaining -= current_grouping;
if (count) {
*count += thousands_sep_len;
} else {
STRINGLIB_CHAR *plast = buffer + remaining;
if (pmax - pend < thousands_sep_len)
return 0;
memmove(plast + thousands_sep_len,
plast,
(pend - plast) * sizeof(STRINGLIB_CHAR));
#if STRINGLIB_IS_UNICODE
{
Py_ssize_t i;
for (i = 0; i < thousands_sep_len; ++i)
plast[i] = thousands_sep[i];
}
#else
memcpy(plast, thousands_sep, thousands_sep_len);
#endif
}
pend += thousands_sep_len;
if (*grouping != 0) {
current_grouping = *grouping++;
if (current_grouping == CHAR_MAX)
break;
}
}
if (append_zero_char) {
if (pend - (buffer + remaining) < 1)
return 0;
*pend = 0;
}
return 1;
}
#endif
