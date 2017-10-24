#include <Python.h>
#include <locale.h>
#define ISSPACE(c) ((c) == ' ' || (c) == '\f' || (c) == '\n' || (c) == '\r' || (c) == '\t' || (c) == '\v')
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
double
PyOS_ascii_strtod(const char *nptr, char **endptr) {
char *fail_pos;
double val = -1.0;
struct lconv *locale_data;
const char *decimal_point;
size_t decimal_point_len;
const char *p, *decimal_point_pos;
const char *end = NULL;
const char *digits_pos = NULL;
int negate = 0;
assert(nptr != NULL);
fail_pos = NULL;
locale_data = localeconv();
decimal_point = locale_data->decimal_point;
decimal_point_len = strlen(decimal_point);
assert(decimal_point_len != 0);
decimal_point_pos = NULL;
p = nptr;
while (ISSPACE(*p))
p++;
if (*p == '-') {
negate = 1;
p++;
} else if (*p == '+') {
p++;
}
if ((!ISDIGIT(*p) &&
*p != '.' && *p != 'i' && *p != 'I' && *p != 'n' && *p != 'N')
||
(*p == '0' && (p[1] == 'x' || p[1] == 'X'))) {
if (endptr)
*endptr = (char*)nptr;
errno = EINVAL;
return val;
}
digits_pos = p;
if (decimal_point[0] != '.' ||
decimal_point[1] != 0) {
while (ISDIGIT(*p))
p++;
if (*p == '.') {
decimal_point_pos = p++;
while (ISDIGIT(*p))
p++;
if (*p == 'e' || *p == 'E')
p++;
if (*p == '+' || *p == '-')
p++;
while (ISDIGIT(*p))
p++;
end = p;
} else if (strncmp(p, decimal_point, decimal_point_len) == 0) {
if (endptr)
*endptr = (char*)nptr;
errno = EINVAL;
return val;
}
}
errno = 0;
if (decimal_point_pos) {
char *copy, *c;
copy = (char *)PyMem_MALLOC(end - digits_pos +
1 + decimal_point_len);
if (copy == NULL) {
if (endptr)
*endptr = (char *)nptr;
errno = ENOMEM;
return val;
}
c = copy;
memcpy(c, digits_pos, decimal_point_pos - digits_pos);
c += decimal_point_pos - digits_pos;
memcpy(c, decimal_point, decimal_point_len);
c += decimal_point_len;
memcpy(c, decimal_point_pos + 1,
end - (decimal_point_pos + 1));
c += end - (decimal_point_pos + 1);
*c = 0;
val = strtod(copy, &fail_pos);
if (fail_pos) {
if (fail_pos > decimal_point_pos)
fail_pos = (char *)digits_pos +
(fail_pos - copy) -
(decimal_point_len - 1);
else
fail_pos = (char *)digits_pos +
(fail_pos - copy);
}
PyMem_FREE(copy);
} else {
val = strtod(digits_pos, &fail_pos);
}
if (fail_pos == digits_pos)
fail_pos = (char *)nptr;
if (negate && fail_pos != nptr)
val = -val;
if (endptr)
*endptr = fail_pos;
return val;
}
Py_LOCAL_INLINE(void)
change_decimal_from_locale_to_dot(char* buffer) {
struct lconv *locale_data = localeconv();
const char *decimal_point = locale_data->decimal_point;
if (decimal_point[0] != '.' || decimal_point[1] != 0) {
size_t decimal_point_len = strlen(decimal_point);
if (*buffer == '+' || *buffer == '-')
buffer++;
while (isdigit(Py_CHARMASK(*buffer)))
buffer++;
if (strncmp(buffer, decimal_point, decimal_point_len) == 0) {
*buffer = '.';
buffer++;
if (decimal_point_len > 1) {
size_t rest_len = strlen(buffer +
(decimal_point_len - 1));
memmove(buffer,
buffer + (decimal_point_len - 1),
rest_len);
buffer[rest_len] = 0;
}
}
}
}
#define MIN_EXPONENT_DIGITS 2
Py_LOCAL_INLINE(void)
ensure_minumim_exponent_length(char* buffer, size_t buf_size) {
char *p = strpbrk(buffer, "eE");
if (p && (*(p + 1) == '-' || *(p + 1) == '+')) {
char *start = p + 2;
int exponent_digit_cnt = 0;
int leading_zero_cnt = 0;
int in_leading_zeros = 1;
int significant_digit_cnt;
p += 2;
while (*p && isdigit(Py_CHARMASK(*p))) {
if (in_leading_zeros && *p == '0')
++leading_zero_cnt;
if (*p != '0')
in_leading_zeros = 0;
++p;
++exponent_digit_cnt;
}
significant_digit_cnt = exponent_digit_cnt - leading_zero_cnt;
if (exponent_digit_cnt == MIN_EXPONENT_DIGITS) {
} else if (exponent_digit_cnt > MIN_EXPONENT_DIGITS) {
int extra_zeros_cnt;
if (significant_digit_cnt < MIN_EXPONENT_DIGITS)
significant_digit_cnt = MIN_EXPONENT_DIGITS;
extra_zeros_cnt = exponent_digit_cnt -
significant_digit_cnt;
assert(extra_zeros_cnt >= 0);
memmove(start,
start + extra_zeros_cnt,
significant_digit_cnt + 1);
} else {
int zeros = MIN_EXPONENT_DIGITS - exponent_digit_cnt;
if (start + zeros + exponent_digit_cnt + 1
< buffer + buf_size) {
memmove(start + zeros, start,
exponent_digit_cnt + 1);
memset(start, '0', zeros);
}
}
}
}
Py_LOCAL_INLINE(void)
ensure_decimal_point(char* buffer, size_t buf_size) {
int insert_count = 0;
char* chars_to_insert;
char *p = buffer;
if (*p == '-' || *p == '+')
++p;
while (*p && isdigit(Py_CHARMASK(*p)))
++p;
if (*p == '.') {
if (isdigit(Py_CHARMASK(*(p+1)))) {
} else {
++p;
chars_to_insert = "0";
insert_count = 1;
}
} else {
chars_to_insert = ".0";
insert_count = 2;
}
if (insert_count) {
size_t buf_len = strlen(buffer);
if (buf_len + insert_count + 1 >= buf_size) {
} else {
memmove(p + insert_count, p,
buffer + strlen(buffer) - p + 1);
memcpy(p, chars_to_insert, insert_count);
}
}
}
Py_LOCAL_INLINE(int)
add_thousands_grouping(char* buffer, size_t buf_size) {
Py_ssize_t len = strlen(buffer);
struct lconv *locale_data = localeconv();
const char *decimal_point = locale_data->decimal_point;
char *p = strstr(buffer, decimal_point);
if (!p) {
p = strpbrk(buffer, "eE");
if (!p)
p = buffer + len;
}
return _PyString_InsertThousandsGrouping(buffer, len, p-buffer,
buf_size, NULL, 1);
}
#define FLOAT_FORMATBUFLEN 120
char *
PyOS_ascii_formatd(char *buffer,
size_t buf_size,
const char *format,
double d) {
char format_char;
size_t format_len = strlen(format);
char tmp_format[FLOAT_FORMATBUFLEN];
format_char = format[format_len - 1];
if (format[0] != '%')
return NULL;
if (strpbrk(format + 1, "'l%"))
return NULL;
if (!(format_char == 'e' || format_char == 'E' ||
format_char == 'f' || format_char == 'F' ||
format_char == 'g' || format_char == 'G' ||
format_char == 'n' || format_char == 'Z'))
return NULL;
if (format_char == 'n' || format_char == 'Z') {
if (format_len + 1 >= sizeof(tmp_format)) {
return NULL;
}
strcpy(tmp_format, format);
tmp_format[format_len - 1] = 'g';
format = tmp_format;
}
PyOS_snprintf(buffer, buf_size, format, d);
if (format_char != 'n')
change_decimal_from_locale_to_dot(buffer);
ensure_minumim_exponent_length(buffer, buf_size);
if (format_char == 'Z')
ensure_decimal_point(buffer, buf_size);
if (format_char == 'n')
if (!add_thousands_grouping(buffer, buf_size))
return NULL;
return buffer;
}
double
PyOS_ascii_atof(const char *nptr) {
return PyOS_ascii_strtod(nptr, NULL);
}
