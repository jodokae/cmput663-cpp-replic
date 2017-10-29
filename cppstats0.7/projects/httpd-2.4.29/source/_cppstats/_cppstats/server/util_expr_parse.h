#if !defined(YYTOKENTYPE)
#define YYTOKENTYPE
enum yytokentype {
T_TRUE = 258,
T_FALSE = 259,
T_EXPR_BOOL = 260,
T_EXPR_STRING = 261,
T_ERROR = 262,
T_DIGIT = 263,
T_ID = 264,
T_STRING = 265,
T_REGEX = 266,
T_REGEX_I = 267,
T_REGEX_BACKREF = 268,
T_OP_UNARY = 269,
T_OP_BINARY = 270,
T_STR_BEGIN = 271,
T_STR_END = 272,
T_VAR_BEGIN = 273,
T_VAR_END = 274,
T_OP_EQ = 275,
T_OP_NE = 276,
T_OP_LT = 277,
T_OP_LE = 278,
T_OP_GT = 279,
T_OP_GE = 280,
T_OP_REG = 281,
T_OP_NRE = 282,
T_OP_IN = 283,
T_OP_STR_EQ = 284,
T_OP_STR_NE = 285,
T_OP_STR_LT = 286,
T_OP_STR_LE = 287,
T_OP_STR_GT = 288,
T_OP_STR_GE = 289,
T_OP_CONCAT = 290,
T_OP_OR = 291,
T_OP_AND = 292,
T_OP_NOT = 293
};
#endif
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE {
#line 35 "util_expr_parse.y"
char *cpVal;
ap_expr_t *exVal;
int num;
#line 96 "util_expr_parse.h"
} YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define yystype YYSTYPE
#define YYSTYPE_IS_DECLARED 1
#endif