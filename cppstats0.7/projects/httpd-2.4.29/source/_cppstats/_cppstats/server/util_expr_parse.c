#define YYBISON 1
#define YYBISON_VERSION "2.5"
#define YYSKELETON_NAME "yacc.c"
#define YYPURE 1
#define YYPUSH 0
#define YYPULL 1
#define YYLSP_NEEDED 0
#define yyparse ap_expr_yyparse
#define yylex ap_expr_yylex
#define yyerror ap_expr_yyerror
#define yylval ap_expr_yylval
#define yychar ap_expr_yychar
#define yydebug ap_expr_yydebug
#define yynerrs ap_expr_yynerrs
#line 31 "util_expr_parse.y"
#include "util_expr_private.h"
#line 84 "util_expr_parse.c"
#if !defined(YYDEBUG)
#define YYDEBUG 0
#endif
#if defined(YYERROR_VERBOSE)
#undef YYERROR_VERBOSE
#define YYERROR_VERBOSE 1
#else
#define YYERROR_VERBOSE 1
#endif
#if !defined(YYTOKEN_TABLE)
#define YYTOKEN_TABLE 0
#endif
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
#line 166 "util_expr_parse.c"
} YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define yystype YYSTYPE
#define YYSTYPE_IS_DECLARED 1
#endif
#line 102 "util_expr_parse.y"
#include "util_expr_private.h"
#define yyscanner ctx->scanner
int ap_expr_yylex(YYSTYPE *lvalp, void *scanner);
#line 186 "util_expr_parse.c"
#if defined(short)
#undef short
#endif
#if defined(YYTYPE_UINT8)
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif
#if defined(YYTYPE_INT8)
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif
#if defined(YYTYPE_UINT16)
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif
#if defined(YYTYPE_INT16)
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif
#if !defined(YYSIZE_T)
#if defined(__SIZE_TYPE__)
#define YYSIZE_T __SIZE_TYPE__
#elif defined size_t
#define YYSIZE_T size_t
#elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
#include <stddef.h>
#define YYSIZE_T size_t
#else
#define YYSIZE_T unsigned int
#endif
#endif
#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)
#if !defined(YY_)
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h>
#define YY_(msgid) dgettext ("bison-runtime", msgid)
#endif
#endif
#if !defined(YY_)
#define YY_(msgid) msgid
#endif
#endif
#if ! defined lint || defined __GNUC__
#define YYUSE(e) ((void) (e))
#else
#define YYUSE(e)
#endif
#if !defined(lint)
#define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
int yyi;
#endif
{
return yyi;
}
#endif
#if ! defined yyoverflow || YYERROR_VERBOSE
#if defined(YYSTACK_USE_ALLOCA)
#if YYSTACK_USE_ALLOCA
#if defined(__GNUC__)
#define YYSTACK_ALLOC __builtin_alloca
#elif defined __BUILTIN_VA_ARG_INCR
#include <alloca.h>
#elif defined _AIX
#define YYSTACK_ALLOC __alloca
#elif defined _MSC_VER
#include <malloc.h>
#define alloca _alloca
#else
#define YYSTACK_ALLOC alloca
#if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
#include <stdlib.h>
#if !defined(EXIT_SUCCESS)
#define EXIT_SUCCESS 0
#endif
#endif
#endif
#endif
#endif
#if defined(YYSTACK_ALLOC)
#define YYSTACK_FREE(Ptr) do { ; } while (YYID (0))
#if !defined(YYSTACK_ALLOC_MAXIMUM)
#define YYSTACK_ALLOC_MAXIMUM 4032
#endif
#else
#define YYSTACK_ALLOC YYMALLOC
#define YYSTACK_FREE YYFREE
#if !defined(YYSTACK_ALLOC_MAXIMUM)
#define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#endif
#if (defined __cplusplus && ! defined EXIT_SUCCESS && ! ((defined YYMALLOC || defined malloc) && (defined YYFREE || defined free)))
#include <stdlib.h>
#if !defined(EXIT_SUCCESS)
#define EXIT_SUCCESS 0
#endif
#endif
#if !defined(YYMALLOC)
#define YYMALLOC malloc
#if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T);
#endif
#endif
#if !defined(YYFREE)
#define YYFREE free
#if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
void free (void *);
#endif
#endif
#endif
#endif
#if (! defined yyoverflow && (! defined __cplusplus || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))
union yyalloc {
yytype_int16 yyss_alloc;
YYSTYPE yyvs_alloc;
};
#define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)
#define YYSTACK_BYTES(N) ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) + YYSTACK_GAP_MAXIMUM)
#define YYCOPY_NEEDED 1
#define YYSTACK_RELOCATE(Stack_alloc, Stack) do { YYSIZE_T yynewbytes; YYCOPY (&yyptr->Stack_alloc, Stack, yysize); Stack = &yyptr->Stack_alloc; yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; yyptr += yynewbytes / sizeof (*yyptr); } while (YYID (0))
#endif
#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
#if !defined(YYCOPY)
#if defined __GNUC__ && 1 < __GNUC__
#define YYCOPY(To, From, Count) __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#else
#define YYCOPY(To, From, Count) do { YYSIZE_T yyi; for (yyi = 0; yyi < (Count); yyi++) (To)[yyi] = (From)[yyi]; } while (YYID (0))
#endif
#endif
#endif
#define YYFINAL 28
#define YYLAST 128
#define YYNTOKENS 45
#define YYNNTS 14
#define YYNRULES 53
#define YYNSTATES 96
#define YYUNDEFTOK 2
#define YYMAXUTOK 293
#define YYTRANSLATE(YYX) ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)
static const yytype_uint8 yytranslate[] = {
0, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
39, 40, 2, 2, 43, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 44, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 41, 2, 42, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
2, 2, 2, 2, 2, 2, 1, 2, 3, 4,
5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
35, 36, 37, 38
};
#if YYDEBUG
static const yytype_uint8 yyprhs[] = {
0, 0, 3, 6, 9, 11, 13, 15, 18, 22,
26, 28, 31, 35, 39, 41, 45, 49, 53, 57,
61, 65, 69, 73, 77, 81, 85, 89, 93, 97,
101, 103, 107, 109, 113, 116, 118, 120, 122, 124,
126, 130, 136, 138, 142, 144, 146, 148, 152, 155,
157, 159, 161, 166
};
static const yytype_int8 yyrhs[] = {
46, 0, -1, 5, 47, -1, 6, 51, -1, 7,
-1, 3, -1, 4, -1, 38, 47, -1, 47, 36,
47, -1, 47, 37, 47, -1, 48, -1, 14, 54,
-1, 54, 15, 54, -1, 39, 47, 40, -1, 7,
-1, 54, 20, 54, -1, 54, 21, 54, -1, 54,
22, 54, -1, 54, 23, 54, -1, 54, 24, 54,
-1, 54, 25, 54, -1, 54, 29, 54, -1, 54,
30, 54, -1, 54, 31, 54, -1, 54, 32, 54,
-1, 54, 33, 54, -1, 54, 34, 54, -1, 54,
28, 49, -1, 54, 26, 55, -1, 54, 27, 55,
-1, 57, -1, 41, 50, 42, -1, 54, -1, 50,
43, 54, -1, 51, 52, -1, 52, -1, 7, -1,
10, -1, 53, -1, 56, -1, 18, 9, 19, -1,
18, 9, 44, 51, 19, -1, 8, -1, 54, 35,
54, -1, 53, -1, 56, -1, 58, -1, 16, 51,
17, -1, 16, 17, -1, 11, -1, 12, -1, 13,
-1, 9, 39, 54, 40, -1, 9, 39, 54, 40,
-1
};
static const yytype_uint8 yyrline[] = {
0, 112, 112, 113, 114, 117, 118, 119, 120, 121,
122, 123, 124, 125, 126, 129, 130, 131, 132, 133,
134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
146, 147, 150, 151, 154, 155, 156, 159, 160, 161,
164, 165, 168, 169, 170, 171, 172, 173, 174, 177,
186, 197, 204, 207
};
#endif
#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
static const char *const yytname[] = {
"$end", "error", "$undefined", "T_TRUE", "T_FALSE", "T_EXPR_BOOL",
"T_EXPR_STRING", "T_ERROR", "T_DIGIT", "T_ID", "T_STRING", "T_REGEX",
"T_REGEX_I", "T_REGEX_BACKREF", "T_OP_UNARY", "T_OP_BINARY",
"T_STR_BEGIN", "T_STR_END", "T_VAR_BEGIN", "T_VAR_END", "T_OP_EQ",
"T_OP_NE", "T_OP_LT", "T_OP_LE", "T_OP_GT", "T_OP_GE", "T_OP_REG",
"T_OP_NRE", "T_OP_IN", "T_OP_STR_EQ", "T_OP_STR_NE", "T_OP_STR_LT",
"T_OP_STR_LE", "T_OP_STR_GT", "T_OP_STR_GE", "T_OP_CONCAT", "T_OP_OR",
"T_OP_AND", "T_OP_NOT", "'('", "')'", "'{'", "'}'", "','", "':'",
"$accept", "root", "expr", "comparison", "wordlist", "words", "string",
"strpart", "var", "word", "regex", "backref", "lstfunccall",
"strfunccall", 0
};
#endif
#if defined(YYPRINT)
static const yytype_uint16 yytoknum[] = {
0, 256, 257, 258, 259, 260, 261, 262, 263, 264,
265, 266, 267, 268, 269, 270, 271, 272, 273, 274,
275, 276, 277, 278, 279, 280, 281, 282, 283, 284,
285, 286, 287, 288, 289, 290, 291, 292, 293, 40,
41, 123, 125, 44, 58
};
#endif
static const yytype_uint8 yyr1[] = {
0, 45, 46, 46, 46, 47, 47, 47, 47, 47,
47, 47, 47, 47, 47, 48, 48, 48, 48, 48,
48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
49, 49, 50, 50, 51, 51, 51, 52, 52, 52,
53, 53, 54, 54, 54, 54, 54, 54, 54, 55,
55, 56, 57, 58
};
static const yytype_uint8 yyr2[] = {
0, 2, 2, 2, 1, 1, 1, 2, 3, 3,
1, 2, 3, 3, 1, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
1, 3, 1, 3, 2, 1, 1, 1, 1, 1,
3, 5, 1, 3, 1, 1, 1, 3, 2, 1,
1, 1, 4, 4
};
static const yytype_uint8 yydefact[] = {
0, 0, 0, 4, 0, 5, 6, 14, 42, 0,
51, 0, 0, 0, 0, 0, 2, 10, 44, 0,
45, 46, 36, 37, 3, 35, 38, 39, 1, 0,
11, 48, 0, 0, 7, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 34, 0, 47, 40, 0,
13, 8, 9, 12, 15, 16, 17, 18, 19, 20,
49, 50, 28, 29, 0, 0, 27, 30, 21, 22,
23, 24, 25, 26, 43, 53, 0, 0, 0, 32,
41, 0, 31, 0, 52, 33
};
static const yytype_int8 yydefgoto[] = {
-1, 4, 16, 17, 76, 88, 24, 25, 18, 19,
72, 20, 77, 21
};
#define YYPACT_NINF -35
static const yytype_int8 yypact[] = {
48, 60, 73, -35, 7, -35, -35, -35, -35, -34,
-35, 43, 8, 11, 60, 60, 86, -35, -35, 80,
-35, -35, -35, -35, 108, -35, -35, -35, -35, 43,
25, -35, 79, -17, -35, -8, 60, 60, 43, 43,
43, 43, 43, 43, 43, 5, 5, 0, 43, 43,
43, 43, 43, 43, 43, -35, -27, -35, -35, 73,
-35, 86, 3, 25, 25, 25, 25, 25, 25, 25,
-35, -35, -35, -35, 23, 43, -35, -35, 25, 25,
25, 25, 25, 25, 25, -35, 106, 43, 85, 25,
-35, -21, -35, 43, -35, 25
};
static const yytype_int8 yypgoto[] = {
-35, -35, 57, -35, -35, -35, -9, -20, -2, -5,
-4, -1, -35, -35
};
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] = {
26, 27, 58, 32, 55, 29, 30, 28, 54, 74,
26, 27, 55, 85, 54, 22, 70, 71, 23, 94,
33, 10, 26, 27, 56, 31, 13, 59, 36, 37,
26, 27, 60, 63, 64, 65, 66, 67, 68, 69,
37, 75, 73, 78, 79, 80, 81, 82, 83, 84,
86, 8, 9, 1, 2, 3, 10, 26, 27, 12,
54, 13, 87, 5, 6, 0, 55, 7, 8, 9,
89, 34, 35, 10, 11, 0, 12, 0, 13, 0,
22, 0, 91, 23, 26, 27, 10, 0, 95, 23,
0, 13, 10, 61, 62, 38, 57, 13, 14, 15,
39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
49, 50, 51, 52, 53, 54, 23, 0, 23, 10,
0, 10, 36, 37, 13, 90, 13, 92, 93
};
#define yypact_value_is_default(yystate) ((yystate) == (-35))
#define yytable_value_is_error(yytable_value) YYID (0)
static const yytype_int8 yycheck[] = {
2, 2, 19, 12, 24, 39, 11, 0, 35, 9,
12, 12, 32, 40, 35, 7, 11, 12, 10, 40,
9, 13, 24, 24, 29, 17, 18, 44, 36, 37,
32, 32, 40, 38, 39, 40, 41, 42, 43, 44,
37, 41, 46, 48, 49, 50, 51, 52, 53, 54,
59, 8, 9, 5, 6, 7, 13, 59, 59, 16,
35, 18, 39, 3, 4, -1, 86, 7, 8, 9,
75, 14, 15, 13, 14, -1, 16, -1, 18, -1,
7, -1, 87, 10, 86, 86, 13, -1, 93, 10,
-1, 18, 13, 36, 37, 15, 17, 18, 38, 39,
20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
30, 31, 32, 33, 34, 35, 10, -1, 10, 13,
-1, 13, 36, 37, 18, 19, 18, 42, 43
};
static const yytype_uint8 yystos[] = {
0, 5, 6, 7, 46, 3, 4, 7, 8, 9,
13, 14, 16, 18, 38, 39, 47, 48, 53, 54,
56, 58, 7, 10, 51, 52, 53, 56, 0, 39,
54, 17, 51, 9, 47, 47, 36, 37, 15, 20,
21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
31, 32, 33, 34, 35, 52, 54, 17, 19, 44,
40, 47, 47, 54, 54, 54, 54, 54, 54, 54,
11, 12, 55, 55, 9, 41, 49, 57, 54, 54,
54, 54, 54, 54, 54, 40, 51, 39, 50, 54,
19, 54, 42, 43, 40, 54
};
#define yyerrok (yyerrstatus = 0)
#define yyclearin (yychar = YYEMPTY)
#define YYEMPTY (-2)
#define YYEOF 0
#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab
#define YYFAIL goto yyerrlab
#if defined YYFAIL
#endif
#define YYRECOVERING() (!!yyerrstatus)
#define YYBACKUP(Token, Value) do if (yychar == YYEMPTY && yylen == 1) { yychar = (Token); yylval = (Value); YYPOPSTACK (1); goto yybackup; } else { yyerror (ctx, YY_("syntax error: cannot back up")); YYERROR; } while (YYID (0))
#define YYTERROR 1
#define YYERRCODE 256
#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#if !defined(YYLLOC_DEFAULT)
#define YYLLOC_DEFAULT(Current, Rhs, N) do if (YYID (N)) { (Current).first_line = YYRHSLOC (Rhs, 1).first_line; (Current).first_column = YYRHSLOC (Rhs, 1).first_column; (Current).last_line = YYRHSLOC (Rhs, N).last_line; (Current).last_column = YYRHSLOC (Rhs, N).last_column; } else { (Current).first_line = (Current).last_line = YYRHSLOC (Rhs, 0).last_line; (Current).first_column = (Current).last_column = YYRHSLOC (Rhs, 0).last_column; } while (YYID (0))
#endif
#if !defined(YY_LOCATION_PRINT)
#define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif
#if defined(YYLEX_PARAM)
#define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
#define YYLEX yylex (&yylval, yyscanner)
#endif
#if YYDEBUG
#if !defined(YYFPRINTF)
#include <stdio.h>
#define YYFPRINTF fprintf
#endif
#define YYDPRINTF(Args) do { if (yydebug) YYFPRINTF Args; } while (YYID (0))
#define YY_SYMBOL_PRINT(Title, Type, Value, Location) do { if (yydebug) { YYFPRINTF (stderr, "%s ", Title); yy_symbol_print (stderr, Type, Value, ctx); YYFPRINTF (stderr, "\n"); } } while (YYID (0))
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, ap_expr_parse_ctx_t *ctx)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, ctx)
FILE *yyoutput;
int yytype;
YYSTYPE const * const yyvaluep;
ap_expr_parse_ctx_t *ctx;
#endif
{
if (!yyvaluep)
return;
YYUSE (ctx);
#if defined(YYPRINT)
if (yytype < YYNTOKENS)
YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
#else
YYUSE (yyoutput);
#endif
switch (yytype) {
default:
break;
}
}
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, ap_expr_parse_ctx_t *ctx)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, ctx)
FILE *yyoutput;
int yytype;
YYSTYPE const * const yyvaluep;
ap_expr_parse_ctx_t *ctx;
#endif
{
if (yytype < YYNTOKENS)
YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
else
YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);
yy_symbol_value_print (yyoutput, yytype, yyvaluep, ctx);
YYFPRINTF (yyoutput, ")");
}
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
yytype_int16 *yybottom;
yytype_int16 *yytop;
#endif
{
YYFPRINTF (stderr, "Stack now");
for (; yybottom <= yytop; yybottom++) {
int yybot = *yybottom;
YYFPRINTF (stderr, " %d", yybot);
}
YYFPRINTF (stderr, "\n");
}
#define YY_STACK_PRINT(Bottom, Top) do { if (yydebug) yy_stack_print ((Bottom), (Top)); } while (YYID (0))
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, ap_expr_parse_ctx_t *ctx)
#else
static void
yy_reduce_print (yyvsp, yyrule, ctx)
YYSTYPE *yyvsp;
int yyrule;
ap_expr_parse_ctx_t *ctx;
#endif
{
int yynrhs = yyr2[yyrule];
int yyi;
unsigned long int yylno = yyrline[yyrule];
YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
yyrule - 1, yylno);
for (yyi = 0; yyi < yynrhs; yyi++) {
YYFPRINTF (stderr, " $%d = ", yyi + 1);
yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
&(yyvsp[(yyi + 1) - (yynrhs)])
, ctx);
YYFPRINTF (stderr, "\n");
}
}
#define YY_REDUCE_PRINT(Rule) do { if (yydebug) yy_reduce_print (yyvsp, Rule, ctx); } while (YYID (0))
int yydebug;
#else
#define YYDPRINTF(Args)
#define YY_SYMBOL_PRINT(Title, Type, Value, Location)
#define YY_STACK_PRINT(Bottom, Top)
#define YY_REDUCE_PRINT(Rule)
#endif
#if !defined(YYINITDEPTH)
#define YYINITDEPTH 200
#endif
#if !defined(YYMAXDEPTH)
#define YYMAXDEPTH 10000
#endif
#if YYERROR_VERBOSE
#if !defined(yystrlen)
#if defined __GLIBC__ && defined _STRING_H
#define yystrlen strlen
#else
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
const char *yystr;
#endif
{
YYSIZE_T yylen;
for (yylen = 0; yystr[yylen]; yylen++)
continue;
return yylen;
}
#endif
#endif
#if !defined(yystpcpy)
#if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#define yystpcpy stpcpy
#else
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
char *yydest;
const char *yysrc;
#endif
{
char *yyd = yydest;
const char *yys = yysrc;
while ((*yyd++ = *yys++) != '\0')
continue;
return yyd - 1;
}
#endif
#endif
#if !defined(yytnamerr)
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr) {
if (*yystr == '"') {
YYSIZE_T yyn = 0;
char const *yyp = yystr;
for (;;)
switch (*++yyp) {
case '\'':
case ',':
goto do_not_strip_quotes;
case '\\':
if (*++yyp != '\\')
goto do_not_strip_quotes;
default:
if (yyres)
yyres[yyn] = *yyp;
yyn++;
break;
case '"':
if (yyres)
yyres[yyn] = '\0';
return yyn;
}
do_not_strip_quotes:
;
}
if (! yyres)
return yystrlen (yystr);
return yystpcpy (yyres, yystr) - yyres;
}
#endif
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
yytype_int16 *yyssp, int yytoken) {
YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
YYSIZE_T yysize = yysize0;
YYSIZE_T yysize1;
enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
const char *yyformat = 0;
char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
int yycount = 0;
if (yytoken != YYEMPTY) {
int yyn = yypact[*yyssp];
yyarg[yycount++] = yytname[yytoken];
if (!yypact_value_is_default (yyn)) {
int yyxbegin = yyn < 0 ? -yyn : 0;
int yychecklim = YYLAST - yyn + 1;
int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
int yyx;
for (yyx = yyxbegin; yyx < yyxend; ++yyx)
if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
&& !yytable_value_is_error (yytable[yyx + yyn])) {
if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM) {
yycount = 1;
yysize = yysize0;
break;
}
yyarg[yycount++] = yytname[yyx];
yysize1 = yysize + yytnamerr (0, yytname[yyx]);
if (! (yysize <= yysize1
&& yysize1 <= YYSTACK_ALLOC_MAXIMUM))
return 2;
yysize = yysize1;
}
}
}
switch (yycount) {
#define YYCASE_(N, S) case N: yyformat = S; break
YYCASE_(0, YY_("syntax error"));
YYCASE_(1, YY_("syntax error, unexpected %s"));
YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
}
yysize1 = yysize + yystrlen (yyformat);
if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
return 2;
yysize = yysize1;
if (*yymsg_alloc < yysize) {
*yymsg_alloc = 2 * yysize;
if (! (yysize <= *yymsg_alloc
&& *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
*yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
return 1;
}
{
char *yyp = *yymsg;
int yyi = 0;
while ((*yyp = *yyformat) != '\0')
if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount) {
yyp += yytnamerr (yyp, yyarg[yyi++]);
yyformat += 2;
} else {
yyp++;
yyformat++;
}
}
return 0;
}
#endif
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, ap_expr_parse_ctx_t *ctx)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, ctx)
const char *yymsg;
int yytype;
YYSTYPE *yyvaluep;
ap_expr_parse_ctx_t *ctx;
#endif
{
YYUSE (yyvaluep);
YYUSE (ctx);
if (!yymsg)
yymsg = "Deleting";
YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);
switch (yytype) {
default:
break;
}
}
#if defined(YYPARSE_PARAM)
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else
#if defined __STDC__ || defined __cplusplus
int yyparse (ap_expr_parse_ctx_t *ctx);
#else
int yyparse ();
#endif
#endif
#if defined(YYPARSE_PARAM)
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
void *YYPARSE_PARAM;
#endif
#else
#if (defined __STDC__ || defined __C99__FUNC__ || defined __cplusplus || defined _MSC_VER)
int
yyparse (ap_expr_parse_ctx_t *ctx)
#else
int
yyparse (ctx)
ap_expr_parse_ctx_t *ctx;
#endif
#endif
{
int yychar;
YYSTYPE yylval;
int yynerrs;
int yystate;
int yyerrstatus;
yytype_int16 yyssa[YYINITDEPTH];
yytype_int16 *yyss;
yytype_int16 *yyssp;
YYSTYPE yyvsa[YYINITDEPTH];
YYSTYPE *yyvs;
YYSTYPE *yyvsp;
YYSIZE_T yystacksize;
int yyn;
int yyresult;
int yytoken;
YYSTYPE yyval;
#if YYERROR_VERBOSE
char yymsgbuf[128];
char *yymsg = yymsgbuf;
YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif
#define YYPOPSTACK(N) (yyvsp -= (N), yyssp -= (N))
int yylen = 0;
yytoken = 0;
yyss = yyssa;
yyvs = yyvsa;
yystacksize = YYINITDEPTH;
YYDPRINTF ((stderr, "Starting parse\n"));
yystate = 0;
yyerrstatus = 0;
yynerrs = 0;
yychar = YYEMPTY;
yyssp = yyss;
yyvsp = yyvs;
goto yysetstate;
yynewstate:
yyssp++;
yysetstate:
*yyssp = yystate;
if (yyss + yystacksize - 1 <= yyssp) {
YYSIZE_T yysize = yyssp - yyss + 1;
#if defined(yyoverflow)
{
YYSTYPE *yyvs1 = yyvs;
yytype_int16 *yyss1 = yyss;
yyoverflow (YY_("memory exhausted"),
&yyss1, yysize * sizeof (*yyssp),
&yyvs1, yysize * sizeof (*yyvsp),
&yystacksize);
yyss = yyss1;
yyvs = yyvs1;
}
#else
#if !defined(YYSTACK_RELOCATE)
goto yyexhaustedlab;
#else
if (YYMAXDEPTH <= yystacksize)
goto yyexhaustedlab;
yystacksize *= 2;
if (YYMAXDEPTH < yystacksize)
yystacksize = YYMAXDEPTH;
{
yytype_int16 *yyss1 = yyss;
union yyalloc *yyptr =
(union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
if (! yyptr)
goto yyexhaustedlab;
YYSTACK_RELOCATE (yyss_alloc, yyss);
YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#undef YYSTACK_RELOCATE
if (yyss1 != yyssa)
YYSTACK_FREE (yyss1);
}
#endif
#endif
yyssp = yyss + yysize - 1;
yyvsp = yyvs + yysize - 1;
YYDPRINTF ((stderr, "Stack size increased to %lu\n",
(unsigned long int) yystacksize));
if (yyss + yystacksize - 1 <= yyssp)
YYABORT;
}
YYDPRINTF ((stderr, "Entering state %d\n", yystate));
if (yystate == YYFINAL)
YYACCEPT;
goto yybackup;
yybackup:
yyn = yypact[yystate];
if (yypact_value_is_default (yyn))
goto yydefault;
if (yychar == YYEMPTY) {
YYDPRINTF ((stderr, "Reading a token: "));
yychar = YYLEX;
}
if (yychar <= YYEOF) {
yychar = yytoken = YYEOF;
YYDPRINTF ((stderr, "Now at end of input.\n"));
} else {
yytoken = YYTRANSLATE (yychar);
YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
}
yyn += yytoken;
if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
goto yydefault;
yyn = yytable[yyn];
if (yyn <= 0) {
if (yytable_value_is_error (yyn))
goto yyerrlab;
yyn = -yyn;
goto yyreduce;
}
if (yyerrstatus)
yyerrstatus--;
YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
yychar = YYEMPTY;
yystate = yyn;
*++yyvsp = yylval;
goto yynewstate;
yydefault:
yyn = yydefact[yystate];
if (yyn == 0)
goto yyerrlab;
goto yyreduce;
yyreduce:
yylen = yyr2[yyn];
yyval = yyvsp[1-yylen];
YY_REDUCE_PRINT (yyn);
switch (yyn) {
case 2:
#line 112 "util_expr_parse.y"
{
ctx->expr = (yyvsp[(2) - (2)].exVal);
}
break;
case 3:
#line 113 "util_expr_parse.y"
{
ctx->expr = (yyvsp[(2) - (2)].exVal);
}
break;
case 4:
#line 114 "util_expr_parse.y"
{
YYABORT;
}
break;
case 5:
#line 117 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_True, NULL, NULL, ctx);
}
break;
case 6:
#line 118 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_False, NULL, NULL, ctx);
}
break;
case 7:
#line 119 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_Not, (yyvsp[(2) - (2)].exVal), NULL, ctx);
}
break;
case 8:
#line 120 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_Or, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 9:
#line 121 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_And, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 10:
#line 122 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_Comp, (yyvsp[(1) - (1)].exVal), NULL, ctx);
}
break;
case 11:
#line 123 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_unary_op_make( (yyvsp[(1) - (2)].cpVal), (yyvsp[(2) - (2)].exVal), ctx);
}
break;
case 12:
#line 124 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_binary_op_make((yyvsp[(2) - (3)].cpVal), (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 13:
#line 125 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(2) - (3)].exVal);
}
break;
case 14:
#line 126 "util_expr_parse.y"
{
YYABORT;
}
break;
case 15:
#line 129 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_EQ, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 16:
#line 130 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_NE, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 17:
#line 131 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_LT, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 18:
#line 132 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_LE, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 19:
#line 133 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_GT, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 20:
#line 134 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_GE, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 21:
#line 135 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_STR_EQ, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 22:
#line 136 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_STR_NE, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 23:
#line 137 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_STR_LT, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 24:
#line 138 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_STR_LE, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 25:
#line 139 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_STR_GT, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 26:
#line 140 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_STR_GE, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 27:
#line 141 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_IN, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 28:
#line 142 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_REG, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 29:
#line 143 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_NRE, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 30:
#line 146 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(1) - (1)].exVal);
}
break;
case 31:
#line 147 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(2) - (3)].exVal);
}
break;
case 32:
#line 150 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_ListElement, (yyvsp[(1) - (1)].exVal), NULL, ctx);
}
break;
case 33:
#line 151 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_ListElement, (yyvsp[(3) - (3)].exVal), (yyvsp[(1) - (3)].exVal), ctx);
}
break;
case 34:
#line 154 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_Concat, (yyvsp[(1) - (2)].exVal), (yyvsp[(2) - (2)].exVal), ctx);
}
break;
case 35:
#line 155 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(1) - (1)].exVal);
}
break;
case 36:
#line 156 "util_expr_parse.y"
{
YYABORT;
}
break;
case 37:
#line 159 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_String, (yyvsp[(1) - (1)].cpVal), NULL, ctx);
}
break;
case 38:
#line 160 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(1) - (1)].exVal);
}
break;
case 39:
#line 161 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(1) - (1)].exVal);
}
break;
case 40:
#line 164 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_var_make((yyvsp[(2) - (3)].cpVal), ctx);
}
break;
case 41:
#line 165 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_str_func_make((yyvsp[(2) - (5)].cpVal), (yyvsp[(4) - (5)].exVal), ctx);
}
break;
case 42:
#line 168 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_Digit, (yyvsp[(1) - (1)].cpVal), NULL, ctx);
}
break;
case 43:
#line 169 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_Concat, (yyvsp[(1) - (3)].exVal), (yyvsp[(3) - (3)].exVal), ctx);
}
break;
case 44:
#line 170 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(1) - (1)].exVal);
}
break;
case 45:
#line 171 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(1) - (1)].exVal);
}
break;
case 46:
#line 172 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(1) - (1)].exVal);
}
break;
case 47:
#line 173 "util_expr_parse.y"
{
(yyval.exVal) = (yyvsp[(2) - (3)].exVal);
}
break;
case 48:
#line 174 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_make(op_String, "", NULL, ctx);
}
break;
case 49:
#line 177 "util_expr_parse.y"
{
ap_regex_t *regex;
if ((regex = ap_pregcomp(ctx->pool, (yyvsp[(1) - (1)].cpVal),
AP_REG_EXTENDED|AP_REG_NOSUB)) == NULL) {
ctx->error = "Failed to compile regular expression";
YYERROR;
}
(yyval.exVal) = ap_expr_make(op_Regex, regex, NULL, ctx);
}
break;
case 50:
#line 186 "util_expr_parse.y"
{
ap_regex_t *regex;
if ((regex = ap_pregcomp(ctx->pool, (yyvsp[(1) - (1)].cpVal),
AP_REG_EXTENDED|AP_REG_NOSUB|AP_REG_ICASE)) == NULL) {
ctx->error = "Failed to compile regular expression";
YYERROR;
}
(yyval.exVal) = ap_expr_make(op_Regex, regex, NULL, ctx);
}
break;
case 51:
#line 197 "util_expr_parse.y"
{
int *n = apr_palloc(ctx->pool, sizeof(int));
*n = (yyvsp[(1) - (1)].num);
(yyval.exVal) = ap_expr_make(op_RegexBackref, n, NULL, ctx);
}
break;
case 52:
#line 204 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_list_func_make((yyvsp[(1) - (4)].cpVal), (yyvsp[(3) - (4)].exVal), ctx);
}
break;
case 53:
#line 207 "util_expr_parse.y"
{
(yyval.exVal) = ap_expr_str_func_make((yyvsp[(1) - (4)].cpVal), (yyvsp[(3) - (4)].exVal), ctx);
}
break;
#line 1891 "util_expr_parse.c"
default:
break;
}
YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);
YYPOPSTACK (yylen);
yylen = 0;
YY_STACK_PRINT (yyss, yyssp);
*++yyvsp = yyval;
yyn = yyr1[yyn];
yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
yystate = yytable[yystate];
else
yystate = yydefgoto[yyn - YYNTOKENS];
goto yynewstate;
yyerrlab:
yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);
if (!yyerrstatus) {
++yynerrs;
#if ! YYERROR_VERBOSE
yyerror (ctx, YY_("syntax error"));
#else
#define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, yyssp, yytoken)
{
char const *yymsgp = YY_("syntax error");
int yysyntax_error_status;
yysyntax_error_status = YYSYNTAX_ERROR;
if (yysyntax_error_status == 0)
yymsgp = yymsg;
else if (yysyntax_error_status == 1) {
if (yymsg != yymsgbuf)
YYSTACK_FREE (yymsg);
yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
if (!yymsg) {
yymsg = yymsgbuf;
yymsg_alloc = sizeof yymsgbuf;
yysyntax_error_status = 2;
} else {
yysyntax_error_status = YYSYNTAX_ERROR;
yymsgp = yymsg;
}
}
yyerror (ctx, yymsgp);
if (yysyntax_error_status == 2)
goto yyexhaustedlab;
}
#undef YYSYNTAX_ERROR
#endif
}
if (yyerrstatus == 3) {
if (yychar <= YYEOF) {
if (yychar == YYEOF)
YYABORT;
} else {
yydestruct ("Error: discarding",
yytoken, &yylval, ctx);
yychar = YYEMPTY;
}
}
goto yyerrlab1;
yyerrorlab:
if ( 0)
goto yyerrorlab;
YYPOPSTACK (yylen);
yylen = 0;
YY_STACK_PRINT (yyss, yyssp);
yystate = *yyssp;
goto yyerrlab1;
yyerrlab1:
yyerrstatus = 3;
for (;;) {
yyn = yypact[yystate];
if (!yypact_value_is_default (yyn)) {
yyn += YYTERROR;
if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR) {
yyn = yytable[yyn];
if (0 < yyn)
break;
}
}
if (yyssp == yyss)
YYABORT;
yydestruct ("Error: popping",
yystos[yystate], yyvsp, ctx);
YYPOPSTACK (1);
yystate = *yyssp;
YY_STACK_PRINT (yyss, yyssp);
}
*++yyvsp = yylval;
YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);
yystate = yyn;
goto yynewstate;
yyacceptlab:
yyresult = 0;
goto yyreturn;
yyabortlab:
yyresult = 1;
goto yyreturn;
#if !defined(yyoverflow) || YYERROR_VERBOSE
yyexhaustedlab:
yyerror (ctx, YY_("memory exhausted"));
yyresult = 2;
#endif
yyreturn:
if (yychar != YYEMPTY) {
yytoken = YYTRANSLATE (yychar);
yydestruct ("Cleanup: discarding lookahead",
yytoken, &yylval, ctx);
}
YYPOPSTACK (yylen);
YY_STACK_PRINT (yyss, yyssp);
while (yyssp != yyss) {
yydestruct ("Cleanup: popping",
yystos[*yyssp], yyvsp, ctx);
YYPOPSTACK (1);
}
#if !defined(yyoverflow)
if (yyss != yyssa)
YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
if (yymsg != yymsgbuf)
YYSTACK_FREE (yymsg);
#endif
return YYID (yyresult);
}
#line 210 "util_expr_parse.y"
void yyerror(ap_expr_parse_ctx_t *ctx, const char *s) {
ctx->error = apr_pstrdup(ctx->ptemp, s);
}