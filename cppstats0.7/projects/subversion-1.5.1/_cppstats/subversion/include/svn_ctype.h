#if !defined(SVN_CTYPE_H)
#define SVN_CTYPE_H
#include <apr.h>
#if defined(__cplusplus)
extern "C" {
#endif
extern const apr_uint32_t *const svn_ctype_table;
#define svn_ctype_test(c, flags) (0 != (svn_ctype_table[(unsigned char)(c)] & (flags)))
#define SVN_CTYPE_CNTRL 0x0001
#define SVN_CTYPE_SPACE 0x0002
#define SVN_CTYPE_DIGIT 0x0004
#define SVN_CTYPE_UPPER 0x0008
#define SVN_CTYPE_LOWER 0x0010
#define SVN_CTYPE_PUNCT 0x0020
#define SVN_CTYPE_XALPHA 0x0040
#define SVN_CTYPE_ASCII 0x0080
#define SVN_CTYPE_ALPHA (SVN_CTYPE_LOWER | SVN_CTYPE_UPPER)
#define SVN_CTYPE_ALNUM (SVN_CTYPE_ALPHA | SVN_CTYPE_DIGIT)
#define SVN_CTYPE_XDIGIT (SVN_CTYPE_DIGIT | SVN_CTYPE_XALPHA)
#define SVN_CTYPE_GRAPH (SVN_CTYPE_PUNCT | SVN_CTYPE_ALNUM)
#define SVN_CTYPE_PRINT (SVN_CTYPE_GRAPH | SVN_CTYPE_SPACE)
#define svn_ctype_iscntrl(c) svn_ctype_test((c), SVN_CTYPE_CNTRL)
#define svn_ctype_isspace(c) svn_ctype_test((c), SVN_CTYPE_SPACE)
#define svn_ctype_isdigit(c) svn_ctype_test((c), SVN_CTYPE_DIGIT)
#define svn_ctype_isupper(c) svn_ctype_test((c), SVN_CTYPE_UPPER)
#define svn_ctype_islower(c) svn_ctype_test((c), SVN_CTYPE_LOWER)
#define svn_ctype_ispunct(c) svn_ctype_test((c), SVN_CTYPE_PUNCT)
#define svn_ctype_isascii(c) svn_ctype_test((c), SVN_CTYPE_ASCII)
#define svn_ctype_isalpha(c) svn_ctype_test((c), SVN_CTYPE_ALPHA)
#define svn_ctype_isalnum(c) svn_ctype_test((c), SVN_CTYPE_ALNUM)
#define svn_ctype_isxdigit(c) svn_ctype_test((c), SVN_CTYPE_XDIGIT)
#define svn_ctype_isgraph(c) svn_ctype_test((c), SVN_CTYPE_GRAPH)
#define svn_ctype_isprint(c) svn_ctype_test((c), SVN_CTYPE_PRINT)
#define SVN_CTYPE_UTF8LEAD 0x0100
#define SVN_CTYPE_UTF8CONT 0x0200
#define SVN_CTYPE_UTF8MBC (SVN_CTYPE_UTF8LEAD | SVN_CTYPE_UTF8CONT)
#define SVN_CTYPE_UTF8 (SVN_CTYPE_ASCII | SVN_CTYPE_UTF8MBC)
#define svn_ctype_isutf8lead(c) svn_ctype_test((c), SVN_CTYPE_UTF8LEAD)
#define svn_ctype_isutf8cont(c) svn_ctype_test((c), SVN_CTYLE_UTF8CONT)
#define svn_ctype_isutf8mbc(c) svn_ctype_test((c), SVN_CTYPE_UTF8MBC)
#define svn_ctype_isutf8(c) svn_ctype_test((c), SVN_CTYPE_UTF8)
#define SVN_CTYPE_ASCII_MINUS 45
#define SVN_CTYPE_ASCII_DOT 46
#define SVN_CTYPE_ASCII_COLON 58
#define SVN_CTYPE_ASCII_UNDERSCORE 95
#define SVN_CTYPE_ASCII_TAB 9
#define SVN_CTYPE_ASCII_LINEFEED 10
#define SVN_CTYPE_ASCII_CARRIAGERETURN 13
#define SVN_CTYPE_ASCII_DELETE 127
int svn_ctype_casecmp(int a, int b);
#if defined(__cplusplus)
}
#endif
#endif
