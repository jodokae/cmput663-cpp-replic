#if !defined(STRINGLIB_UNICODEDEFS_H)
#define STRINGLIB_UNICODEDEFS_H
#define STRINGLIB_IS_UNICODE 1
#define STRINGLIB_OBJECT PyUnicodeObject
#define STRINGLIB_CHAR Py_UNICODE
#define STRINGLIB_TYPE_NAME "unicode"
#define STRINGLIB_PARSE_CODE "U"
#define STRINGLIB_EMPTY unicode_empty
#define STRINGLIB_ISDECIMAL Py_UNICODE_ISDECIMAL
#define STRINGLIB_TODECIMAL Py_UNICODE_TODECIMAL
#define STRINGLIB_TOUPPER Py_UNICODE_TOUPPER
#define STRINGLIB_TOLOWER Py_UNICODE_TOLOWER
#define STRINGLIB_FILL Py_UNICODE_FILL
#define STRINGLIB_STR PyUnicode_AS_UNICODE
#define STRINGLIB_LEN PyUnicode_GET_SIZE
#define STRINGLIB_NEW PyUnicode_FromUnicode
#define STRINGLIB_RESIZE PyUnicode_Resize
#define STRINGLIB_CHECK PyUnicode_Check
#define STRINGLIB_GROUPING _PyUnicode_InsertThousandsGrouping
#if PY_VERSION_HEX < 0x03000000
#define STRINGLIB_TOSTR PyObject_Unicode
#else
#define STRINGLIB_TOSTR PyObject_Str
#endif
#define STRINGLIB_WANT_CONTAINS_OBJ 1
#define STRINGLIB_CMP(str, other, len) (((str)[0] != (other)[0]) ? 1 : memcmp((void*) (str), (void*) (other), (len) * sizeof(Py_UNICODE)))
#endif
