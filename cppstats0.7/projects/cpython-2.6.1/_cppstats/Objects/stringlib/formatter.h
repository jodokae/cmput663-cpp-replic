#define ALLOW_PARENS_FOR_SIGN 0
static int
get_integer(STRINGLIB_CHAR **ptr, STRINGLIB_CHAR *end,
Py_ssize_t *result) {
Py_ssize_t accumulator, digitval, oldaccumulator;
int numdigits;
accumulator = numdigits = 0;
for (;; (*ptr)++, numdigits++) {
if (*ptr >= end)
break;
digitval = STRINGLIB_TODECIMAL(**ptr);
if (digitval < 0)
break;
oldaccumulator = accumulator;
accumulator *= 10;
if ((accumulator+10)/10 != oldaccumulator+1) {
PyErr_Format(PyExc_ValueError,
"Too many decimal digits in format string");
return -1;
}
accumulator += digitval;
}
*result = accumulator;
return numdigits;
}
Py_LOCAL_INLINE(int)
is_alignment_token(STRINGLIB_CHAR c) {
switch (c) {
case '<':
case '>':
case '=':
case '^':
return 1;
default:
return 0;
}
}
Py_LOCAL_INLINE(int)
is_sign_element(STRINGLIB_CHAR c) {
switch (c) {
case ' ':
case '+':
case '-':
#if ALLOW_PARENS_FOR_SIGN
case '(':
#endif
return 1;
default:
return 0;
}
}
typedef struct {
STRINGLIB_CHAR fill_char;
STRINGLIB_CHAR align;
int alternate;
STRINGLIB_CHAR sign;
Py_ssize_t width;
Py_ssize_t precision;
STRINGLIB_CHAR type;
} InternalFormatSpec;
static int
parse_internal_render_format_spec(STRINGLIB_CHAR *format_spec,
Py_ssize_t format_spec_len,
InternalFormatSpec *format,
char default_type) {
STRINGLIB_CHAR *ptr = format_spec;
STRINGLIB_CHAR *end = format_spec + format_spec_len;
Py_ssize_t specified_width;
format->fill_char = '\0';
format->align = '\0';
format->alternate = 0;
format->sign = '\0';
format->width = -1;
format->precision = -1;
format->type = default_type;
if (end-ptr >= 2 && is_alignment_token(ptr[1])) {
format->align = ptr[1];
format->fill_char = ptr[0];
ptr += 2;
} else if (end-ptr >= 1 && is_alignment_token(ptr[0])) {
format->align = ptr[0];
++ptr;
}
if (end-ptr >= 1 && is_sign_element(ptr[0])) {
format->sign = ptr[0];
++ptr;
#if ALLOW_PARENS_FOR_SIGN
if (end-ptr >= 1 && ptr[0] == ')') {
++ptr;
}
#endif
}
if (end-ptr >= 1 && ptr[0] == '#') {
format->alternate = 1;
++ptr;
}
if (format->fill_char == '\0' && end-ptr >= 1 && ptr[0] == '0') {
format->fill_char = '0';
if (format->align == '\0') {
format->align = '=';
}
++ptr;
}
specified_width = get_integer(&ptr, end, &format->width);
if (specified_width == 0) {
format->width = -1;
}
if (end-ptr && ptr[0] == '.') {
++ptr;
specified_width = get_integer(&ptr, end, &format->precision);
if (specified_width == 0) {
PyErr_Format(PyExc_ValueError,
"Format specifier missing precision");
return 0;
}
}
if (end-ptr > 1) {
PyErr_Format(PyExc_ValueError, "Invalid conversion specification");
return 0;
}
if (end-ptr == 1) {
format->type = ptr[0];
++ptr;
}
return 1;
}
#if defined FORMAT_FLOAT || defined FORMAT_LONG
typedef struct {
Py_ssize_t n_lpadding;
Py_ssize_t n_prefix;
Py_ssize_t n_spadding;
Py_ssize_t n_rpadding;
char lsign;
Py_ssize_t n_lsign;
char rsign;
Py_ssize_t n_rsign;
Py_ssize_t n_total;
} NumberFieldWidths;
static void
calc_number_widths(NumberFieldWidths *spec, STRINGLIB_CHAR actual_sign,
Py_ssize_t n_prefix, Py_ssize_t n_digits,
const InternalFormatSpec *format) {
spec->n_lpadding = 0;
spec->n_prefix = 0;
spec->n_spadding = 0;
spec->n_rpadding = 0;
spec->lsign = '\0';
spec->n_lsign = 0;
spec->rsign = '\0';
spec->n_rsign = 0;
if (format->sign == '+') {
spec->n_lsign = 1;
spec->lsign = (actual_sign == '-' ? '-' : '+');
}
#if ALLOW_PARENS_FOR_SIGN
else if (format->sign == '(') {
if (actual_sign == '-') {
spec->n_lsign = 1;
spec->lsign = '(';
spec->n_rsign = 1;
spec->rsign = ')';
}
}
#endif
else if (format->sign == ' ') {
spec->n_lsign = 1;
spec->lsign = (actual_sign == '-' ? '-' : ' ');
} else {
if (actual_sign == '-') {
spec->n_lsign = 1;
spec->lsign = '-';
}
}
spec->n_prefix = n_prefix;
if (format->width == -1) {
} else {
if (spec->n_lsign + n_digits + spec->n_rsign +
spec->n_prefix >= format->width) {
} else {
Py_ssize_t padding = format->width -
(spec->n_lsign + spec->n_prefix +
n_digits + spec->n_rsign);
if (format->align == '<')
spec->n_rpadding = padding;
else if (format->align == '>')
spec->n_lpadding = padding;
else if (format->align == '^') {
spec->n_lpadding = padding / 2;
spec->n_rpadding = padding - spec->n_lpadding;
} else if (format->align == '=')
spec->n_spadding = padding;
else
spec->n_lpadding = padding;
}
}
spec->n_total = spec->n_lpadding + spec->n_lsign + spec->n_prefix +
spec->n_spadding + n_digits + spec->n_rsign + spec->n_rpadding;
}
static STRINGLIB_CHAR *
fill_non_digits(STRINGLIB_CHAR *p_buf, const NumberFieldWidths *spec,
STRINGLIB_CHAR *prefix, Py_ssize_t n_digits,
STRINGLIB_CHAR fill_char) {
STRINGLIB_CHAR *p_digits;
if (spec->n_lpadding) {
STRINGLIB_FILL(p_buf, fill_char, spec->n_lpadding);
p_buf += spec->n_lpadding;
}
if (spec->n_lsign == 1) {
*p_buf++ = spec->lsign;
}
if (spec->n_prefix) {
memmove(p_buf,
prefix,
spec->n_prefix * sizeof(STRINGLIB_CHAR));
p_buf += spec->n_prefix;
}
if (spec->n_spadding) {
STRINGLIB_FILL(p_buf, fill_char, spec->n_spadding);
p_buf += spec->n_spadding;
}
p_digits = p_buf;
p_buf += n_digits;
if (spec->n_rsign == 1) {
*p_buf++ = spec->rsign;
}
if (spec->n_rpadding) {
STRINGLIB_FILL(p_buf, fill_char, spec->n_rpadding);
p_buf += spec->n_rpadding;
}
return p_digits;
}
#endif
static PyObject *
format_string_internal(PyObject *value, const InternalFormatSpec *format) {
Py_ssize_t width;
Py_ssize_t lpad;
STRINGLIB_CHAR *dst;
STRINGLIB_CHAR *src = STRINGLIB_STR(value);
Py_ssize_t len = STRINGLIB_LEN(value);
PyObject *result = NULL;
if (format->sign != '\0') {
PyErr_SetString(PyExc_ValueError,
"Sign not allowed in string format specifier");
goto done;
}
if (format->alternate) {
PyErr_SetString(PyExc_ValueError,
"Alternate form (#) not allowed in string format "
"specifier");
goto done;
}
if (format->align == '=') {
PyErr_SetString(PyExc_ValueError,
"'=' alignment not allowed "
"in string format specifier");
goto done;
}
if (format->precision >= 0 && len >= format->precision) {
len = format->precision;
}
if (format->width >= 0) {
width = format->width;
if (len > width) {
width = len;
}
} else {
width = len;
}
result = STRINGLIB_NEW(NULL, width);
if (result == NULL)
goto done;
dst = STRINGLIB_STR(result);
if (format->align == '>')
lpad = width - len;
else if (format->align == '^')
lpad = (width - len) / 2;
else
lpad = 0;
memcpy(dst + lpad, src, len * sizeof(STRINGLIB_CHAR));
if (width > len) {
STRINGLIB_CHAR fill_char = format->fill_char;
if (fill_char == '\0') {
fill_char = ' ';
}
if (lpad)
STRINGLIB_FILL(dst, fill_char, lpad);
if (width - len - lpad)
STRINGLIB_FILL(dst + len + lpad, fill_char, width - len - lpad);
}
done:
return result;
}
#if defined FORMAT_LONG || defined FORMAT_INT
typedef PyObject*
(*IntOrLongToString)(PyObject *value, int base);
static PyObject *
format_int_or_long_internal(PyObject *value, const InternalFormatSpec *format,
IntOrLongToString tostring) {
PyObject *result = NULL;
PyObject *tmp = NULL;
STRINGLIB_CHAR *pnumeric_chars;
STRINGLIB_CHAR numeric_char;
STRINGLIB_CHAR sign = '\0';
STRINGLIB_CHAR *p;
Py_ssize_t n_digits;
Py_ssize_t n_leading_chars;
Py_ssize_t n_grouping_chars = 0;
Py_ssize_t n_prefix = 0;
STRINGLIB_CHAR *prefix = NULL;
NumberFieldWidths spec;
long x;
if (format->precision != -1) {
PyErr_SetString(PyExc_ValueError,
"Precision not allowed in integer format specifier");
goto done;
}
if (format->type == 'c') {
if (format->sign != '\0') {
PyErr_SetString(PyExc_ValueError,
"Sign not allowed with integer"
" format specifier 'c'");
goto done;
}
x = PyLong_AsLong(value);
if (x == -1 && PyErr_Occurred())
goto done;
#if defined(Py_UNICODE_WIDE)
if (x < 0 || x > 0x10ffff) {
PyErr_SetString(PyExc_OverflowError,
"%c arg not in range(0x110000) "
"(wide Python build)");
goto done;
}
#else
if (x < 0 || x > 0xffff) {
PyErr_SetString(PyExc_OverflowError,
"%c arg not in range(0x10000) "
"(narrow Python build)");
goto done;
}
#endif
numeric_char = (STRINGLIB_CHAR)x;
pnumeric_chars = &numeric_char;
n_digits = 1;
} else {
int base;
int leading_chars_to_skip = 0;
switch (format->type) {
case 'b':
base = 2;
leading_chars_to_skip = 2;
break;
case 'o':
base = 8;
leading_chars_to_skip = 2;
break;
case 'x':
case 'X':
base = 16;
leading_chars_to_skip = 2;
break;
default:
case 'd':
case 'n':
base = 10;
break;
}
if (format->alternate)
n_prefix = leading_chars_to_skip;
tmp = tostring(value, base);
if (tmp == NULL)
goto done;
pnumeric_chars = STRINGLIB_STR(tmp);
n_digits = STRINGLIB_LEN(tmp);
prefix = pnumeric_chars;
sign = pnumeric_chars[0];
if (sign == '-') {
++prefix;
++leading_chars_to_skip;
}
n_digits -= leading_chars_to_skip;
pnumeric_chars += leading_chars_to_skip;
}
if (format->type == 'n')
STRINGLIB_GROUPING(NULL, n_digits, n_digits,
0, &n_grouping_chars, 0);
calc_number_widths(&spec, sign, n_prefix, n_digits + n_grouping_chars,
format);
result = STRINGLIB_NEW(NULL, spec.n_total);
if (!result)
goto done;
p = STRINGLIB_STR(result);
n_leading_chars = spec.n_lpadding + spec.n_lsign +
spec.n_prefix + spec.n_spadding;
memmove(p + n_leading_chars,
pnumeric_chars,
n_digits * sizeof(STRINGLIB_CHAR));
if (format->type == 'X') {
Py_ssize_t t;
for (t = 0; t < n_digits; ++t)
p[t + n_leading_chars] = STRINGLIB_TOUPPER(p[t + n_leading_chars]);
}
if (n_grouping_chars) {
STRINGLIB_CHAR *pstart = p + n_leading_chars;
#if !defined(NDEBUG)
int r =
#endif
STRINGLIB_GROUPING(pstart, n_digits, n_digits,
spec.n_total+n_grouping_chars-n_leading_chars,
NULL, 0);
assert(r);
}
fill_non_digits(p, &spec, prefix, n_digits + n_grouping_chars,
format->fill_char == '\0' ? ' ' : format->fill_char);
if (format->type == 'X') {
Py_ssize_t t;
for (t = 0; t < n_prefix; ++t)
p[t + spec.n_lpadding + spec.n_lsign] =
STRINGLIB_TOUPPER(p[t + spec.n_lpadding + spec.n_lsign]);
}
done:
Py_XDECREF(tmp);
return result;
}
#endif
#if defined(FORMAT_FLOAT)
#if STRINGLIB_IS_UNICODE
static Py_ssize_t
strtounicode(Py_UNICODE *buffer, const char *charbuffer) {
register Py_ssize_t i;
Py_ssize_t len = strlen(charbuffer);
for (i = len - 1; i >= 0; --i)
buffer[i] = (Py_UNICODE) charbuffer[i];
return len;
}
#endif
#define FLOAT_FORMATBUFLEN 120
static PyObject *
format_float_internal(PyObject *value,
const InternalFormatSpec *format) {
char fmt[20];
char charbuf[FLOAT_FORMATBUFLEN];
Py_ssize_t n_digits;
double x;
Py_ssize_t precision = format->precision;
PyObject *result = NULL;
STRINGLIB_CHAR sign;
char* trailing = "";
STRINGLIB_CHAR *p;
NumberFieldWidths spec;
STRINGLIB_CHAR type = format->type;
#if STRINGLIB_IS_UNICODE
Py_UNICODE unicodebuf[FLOAT_FORMATBUFLEN];
#endif
if (format->alternate) {
PyErr_SetString(PyExc_ValueError,
"Alternate form (#) not allowed in float format "
"specifier");
goto done;
}
if (type == 'F')
type = 'f';
x = PyFloat_AsDouble(value);
if (x == -1.0 && PyErr_Occurred())
goto done;
if (type == '%') {
type = 'f';
x *= 100;
trailing = "%";
}
if (precision < 0)
precision = 6;
if (type == 'f' && (fabs(x) / 1e25) >= 1e25)
type = 'g';
PyOS_snprintf(fmt, sizeof(fmt), "%%.%" PY_FORMAT_SIZE_T "d%c", precision,
(char)type);
PyOS_ascii_formatd(charbuf, sizeof(charbuf), fmt, x);
strcat(charbuf, trailing);
#if STRINGLIB_IS_UNICODE
n_digits = strtounicode(unicodebuf, charbuf);
p = unicodebuf;
#else
n_digits = strlen(charbuf);
p = charbuf;
#endif
sign = p[0];
if (sign == '-') {
++p;
--n_digits;
}
calc_number_widths(&spec, sign, 0, n_digits, format);
result = STRINGLIB_NEW(NULL, spec.n_total);
if (result == NULL)
goto done;
fill_non_digits(STRINGLIB_STR(result), &spec, NULL, n_digits,
format->fill_char == '\0' ? ' ' : format->fill_char);
memmove(STRINGLIB_STR(result) +
(spec.n_lpadding + spec.n_lsign + spec.n_spadding),
p,
n_digits * sizeof(STRINGLIB_CHAR));
done:
return result;
}
#endif
PyObject *
FORMAT_STRING(PyObject *obj,
STRINGLIB_CHAR *format_spec,
Py_ssize_t format_spec_len) {
InternalFormatSpec format;
PyObject *result = NULL;
if (format_spec_len == 0) {
result = STRINGLIB_TOSTR(obj);
goto done;
}
if (!parse_internal_render_format_spec(format_spec, format_spec_len,
&format, 's'))
goto done;
switch (format.type) {
case 's':
result = format_string_internal(obj, &format);
break;
default:
#if STRINGLIB_IS_UNICODE
if (format.type > 32 && format.type <128)
#endif
PyErr_Format(PyExc_ValueError, "Unknown conversion type %c",
(char)format.type);
#if STRINGLIB_IS_UNICODE
else
PyErr_Format(PyExc_ValueError, "Unknown conversion type '\\x%x'",
(unsigned int)format.type);
#endif
goto done;
}
done:
return result;
}
#if defined FORMAT_LONG || defined FORMAT_INT
static PyObject*
format_int_or_long(PyObject* obj,
STRINGLIB_CHAR *format_spec,
Py_ssize_t format_spec_len,
IntOrLongToString tostring) {
PyObject *result = NULL;
PyObject *tmp = NULL;
InternalFormatSpec format;
if (format_spec_len == 0) {
result = STRINGLIB_TOSTR(obj);
goto done;
}
if (!parse_internal_render_format_spec(format_spec,
format_spec_len,
&format, 'd'))
goto done;
switch (format.type) {
case 'b':
case 'c':
case 'd':
case 'o':
case 'x':
case 'X':
case 'n':
result = format_int_or_long_internal(obj, &format, tostring);
break;
case 'e':
case 'E':
case 'f':
case 'F':
case 'g':
case 'G':
case '%':
tmp = PyNumber_Float(obj);
if (tmp == NULL)
goto done;
result = format_float_internal(obj, &format);
break;
default:
PyErr_Format(PyExc_ValueError, "Unknown conversion type %c",
format.type);
goto done;
}
done:
Py_XDECREF(tmp);
return result;
}
#endif
#if defined(FORMAT_LONG)
#if PY_VERSION_HEX >= 0x03000000
#define long_format _PyLong_Format
#else
static PyObject*
long_format(PyObject* value, int base) {
assert(PyLong_Check(value));
return _PyLong_Format(value, base, 0, 1);
}
#endif
PyObject *
FORMAT_LONG(PyObject *obj,
STRINGLIB_CHAR *format_spec,
Py_ssize_t format_spec_len) {
return format_int_or_long(obj, format_spec, format_spec_len,
long_format);
}
#endif
#if defined(FORMAT_INT)
static PyObject*
int_format(PyObject* value, int base) {
assert(PyInt_Check(value));
return _PyInt_Format((PyIntObject*)value, base, 1);
}
PyObject *
FORMAT_INT(PyObject *obj,
STRINGLIB_CHAR *format_spec,
Py_ssize_t format_spec_len) {
return format_int_or_long(obj, format_spec, format_spec_len,
int_format);
}
#endif
#if defined(FORMAT_FLOAT)
PyObject *
FORMAT_FLOAT(PyObject *obj,
STRINGLIB_CHAR *format_spec,
Py_ssize_t format_spec_len) {
PyObject *result = NULL;
InternalFormatSpec format;
if (format_spec_len == 0) {
result = STRINGLIB_TOSTR(obj);
goto done;
}
if (!parse_internal_render_format_spec(format_spec,
format_spec_len,
&format, '\0'))
goto done;
switch (format.type) {
case '\0':
format.type = 'Z';
case 'e':
case 'E':
case 'f':
case 'F':
case 'g':
case 'G':
case 'n':
case '%':
result = format_float_internal(obj, &format);
break;
default:
PyErr_Format(PyExc_ValueError, "Unknown conversion type %c",
format.type);
goto done;
}
done:
return result;
}
#endif