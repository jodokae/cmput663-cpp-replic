#if !defined(AP_REGEX_H)
#define AP_REGEX_H
#include "apr.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define AP_REG_ICASE 0x01
#define AP_REG_NEWLINE 0x02
#define AP_REG_NOTBOL 0x04
#define AP_REG_NOTEOL 0x08
#define AP_REG_EXTENDED (0)
#define AP_REG_NOSUB (0)
#define AP_REG_MULTI 0x10
#define AP_REG_NOMEM 0x20
#define AP_REG_DOTALL 0x40
#define AP_REG_MATCH "MATCH_"
enum {
AP_REG_ASSERT = 1,
AP_REG_ESPACE,
AP_REG_INVARG,
AP_REG_NOMATCH
};
typedef struct {
void *re_pcre;
int re_nsub;
apr_size_t re_erroffset;
} ap_regex_t;
typedef struct {
int rm_so;
int rm_eo;
} ap_regmatch_t;
AP_DECLARE(int) ap_regcomp(ap_regex_t *preg, const char *regex, int cflags);
AP_DECLARE(int) ap_regexec(const ap_regex_t *preg, const char *string,
apr_size_t nmatch, ap_regmatch_t *pmatch, int eflags);
AP_DECLARE(int) ap_regexec_len(const ap_regex_t *preg, const char *buff,
apr_size_t len, apr_size_t nmatch,
ap_regmatch_t *pmatch, int eflags);
AP_DECLARE(apr_size_t) ap_regerror(int errcode, const ap_regex_t *preg,
char *errbuf, apr_size_t errbuf_size);
AP_DECLARE(int) ap_regname(const ap_regex_t *preg,
apr_array_header_t *names, const char *prefix,
int upper);
AP_DECLARE(void) ap_regfree(ap_regex_t *preg);
typedef struct {
ap_regex_t rx;
apr_uint32_t flags;
const char *subs;
const char *match;
apr_size_t nmatch;
ap_regmatch_t *pmatch;
} ap_rxplus_t;
AP_DECLARE(ap_rxplus_t*) ap_rxplus_compile(apr_pool_t *pool, const char *pattern);
AP_DECLARE(int) ap_rxplus_exec(apr_pool_t *pool, ap_rxplus_t *rx,
const char *pattern, char **newpattern);
#if defined(DOXYGEN)
AP_DECLARE(int) ap_rxplus_nmatch(ap_rxplus_t *rx);
#else
#define ap_rxplus_nmatch(rx) (((rx)->match != NULL) ? (rx)->nmatch : 0)
#endif
AP_DECLARE(void) ap_rxplus_match(ap_rxplus_t *rx, int n, int *len,
const char **match);
AP_DECLARE(char*) ap_rxplus_pmatch(apr_pool_t *pool, ap_rxplus_t *rx, int n);
#if defined(__cplusplus)
}
#endif
#endif