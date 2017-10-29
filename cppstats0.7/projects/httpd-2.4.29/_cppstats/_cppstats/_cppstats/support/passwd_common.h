#if !defined(_PASSWD_COMMON_H)
#define _PASSWD_COMMON_H
#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_errno.h"
#include "apr_file_io.h"
#include "apr_general.h"
#include "apr_version.h"
#if !APR_VERSION_AT_LEAST(2,0,0)
#include "apu_version.h"
#endif
#define MAX_STRING_LEN 256
#define ALG_PLAIN 0
#define ALG_CRYPT 1
#define ALG_APMD5 2
#define ALG_APSHA 3
#define ALG_BCRYPT 4
#define BCRYPT_DEFAULT_COST 5
#define ERR_FILEPERM 1
#define ERR_SYNTAX 2
#define ERR_PWMISMATCH 3
#define ERR_INTERRUPTED 4
#define ERR_OVERFLOW 5
#define ERR_BADUSER 6
#define ERR_INVALID 7
#define ERR_RANDOM 8
#define ERR_GENERAL 9
#define ERR_ALG_NOT_SUPP 10
#define NL APR_EOL_STR
#if defined(WIN32) || defined(NETWARE)
#define CRYPT_ALGO_SUPPORTED 0
#define PLAIN_ALGO_SUPPORTED 1
#else
#define CRYPT_ALGO_SUPPORTED 1
#define PLAIN_ALGO_SUPPORTED 0
#endif
#if APR_VERSION_AT_LEAST(2,0,0) || (APU_MAJOR_VERSION == 1 && APU_MINOR_VERSION >= 5)
#define BCRYPT_ALGO_SUPPORTED 1
#else
#define BCRYPT_ALGO_SUPPORTED 0
#endif
extern apr_file_t *errfile;
struct passwd_ctx {
apr_pool_t *pool;
const char *errstr;
char *out;
apr_size_t out_len;
char *passwd;
int alg;
int cost;
enum {
PW_PROMPT = 0,
PW_ARG,
PW_STDIN,
PW_PROMPT_VERIFY,
} passwd_src;
};
int abort_on_oom(int rc);
void putline(apr_file_t *f, const char *l);
int parse_common_options(struct passwd_ctx *ctx, char opt, const char *opt_arg);
int get_password(struct passwd_ctx *ctx);
int mkhash(struct passwd_ctx *ctx);
#endif