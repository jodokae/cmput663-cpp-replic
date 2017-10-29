#if !defined(__AP_EXPR_PRIVATE_H__)
#define __AP_EXPR_PRIVATE_H__
#include "httpd.h"
#include "apr_strings.h"
#include "apr_tables.h"
#include "ap_expr.h"
#if !defined(YY_NULL)
#define YY_NULL 0
#endif
#if !defined(MIN)
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#if !APR_HAVE_UNISTD_H
#define YY_NO_UNISTD_H
#endif
#if defined(_MSC_VER)
#define YYMALLOC malloc
#define YYFREE free
#endif
#if !defined(YYDEBUG)
#define YYDEBUG 0
#endif
typedef enum {
op_NOP,
op_True, op_False,
op_Not, op_Or, op_And,
op_Comp,
op_EQ, op_NE, op_LT, op_LE, op_GT, op_GE, op_IN,
op_REG, op_NRE,
op_STR_EQ, op_STR_NE, op_STR_LT, op_STR_LE, op_STR_GT, op_STR_GE,
op_Concat,
op_Digit, op_String, op_Regex, op_RegexBackref,
op_Var,
op_ListElement,
op_UnaryOpCall, op_UnaryOpInfo,
op_BinaryOpCall, op_BinaryOpInfo, op_BinaryOpArgs,
op_StringFuncCall, op_StringFuncInfo,
op_ListFuncCall, op_ListFuncInfo
} ap_expr_node_op_e;
struct ap_expr_node {
ap_expr_node_op_e node_op;
const void *node_arg1;
const void *node_arg2;
};
typedef struct {
const char *inputbuf;
int inputlen;
const char *inputptr;
void *scanner;
char *scan_ptr;
char scan_buf[MAX_STRING_LEN];
char scan_del;
int at_start;
apr_pool_t *pool;
apr_pool_t *ptemp;
ap_expr_t *expr;
const char *error;
const char *error2;
unsigned flags;
ap_expr_lookup_fn_t *lookup_fn;
} ap_expr_parse_ctx_t;
int ap_expr_yyparse(ap_expr_parse_ctx_t *context);
void ap_expr_yyerror(ap_expr_parse_ctx_t *context, const char *err);
int ap_expr_yylex_init(void **scanner);
int ap_expr_yylex_destroy(void *scanner);
void ap_expr_yyset_extra(ap_expr_parse_ctx_t *context, void *scanner);
ap_expr_t *ap_expr_make(ap_expr_node_op_e op, const void *arg1,
const void *arg2, ap_expr_parse_ctx_t *ctx);
ap_expr_t *ap_expr_str_func_make(const char *name, const ap_expr_t *arg,
ap_expr_parse_ctx_t *ctx);
ap_expr_t *ap_expr_list_func_make(const char *name, const ap_expr_t *arg,
ap_expr_parse_ctx_t *ctx);
ap_expr_t *ap_expr_var_make(const char *name, ap_expr_parse_ctx_t *ctx);
ap_expr_t *ap_expr_unary_op_make(const char *name, const ap_expr_t *arg,
ap_expr_parse_ctx_t *ctx);
ap_expr_t *ap_expr_binary_op_make(const char *name, const ap_expr_t *arg1,
const ap_expr_t *arg2,
ap_expr_parse_ctx_t *ctx);
#endif
