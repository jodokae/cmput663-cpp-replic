#if !defined(STRINGLIB_COUNT_H)
#define STRINGLIB_COUNT_H
#if !defined(STRINGLIB_FASTSEARCH_H)
#error must include "stringlib/fastsearch.h" before including this module
#endif
Py_LOCAL_INLINE(Py_ssize_t)
stringlib_count(const STRINGLIB_CHAR* str, Py_ssize_t str_len,
const STRINGLIB_CHAR* sub, Py_ssize_t sub_len) {
Py_ssize_t count;
if (str_len < 0)
return 0;
if (sub_len == 0)
return str_len + 1;
count = fastsearch(str, str_len, sub, sub_len, FAST_COUNT);
if (count < 0)
count = 0;
return count;
}
#endif