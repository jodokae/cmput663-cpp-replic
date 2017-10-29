#if !defined(_REGEXP_H)
#define _REGEXP_H
#include "libsed.h"
#if defined(__cplusplus)
extern "C" {
#endif
#define CBRA 2
#define CCHR 4
#define CDOT 8
#define CCL 12
#define CXCL 16
#define CDOL 20
#define CCEOF 22
#define CKET 24
#define CBACK 36
#define NCCL 40
#define STAR 01
#define RNGE 03
#define NBRA 9
#define PLACE(c) ep[c >> 3] |= bittab[c & 07]
#define ISTHERE(c) (ep[c >> 3] & bittab[c & 07])
typedef struct _step_vars_storage {
char *loc1, *loc2, *locs;
char *braslist[NBRA];
char *braelist[NBRA];
int low;
int size;
} step_vars_storage;
typedef struct _sed_comp_args {
int circf;
int nbra;
} sed_comp_args;
extern char *sed_compile(sed_commands_t *commands, sed_comp_args *compargs,
char *ep, char *endbuf, int seof);
extern void command_errf(sed_commands_t *commands, const char *fmt, ...)
__attribute__((format(printf,2,3)));
#define SEDERR_CGMES "command garbled: %s"
#define SEDERR_SMMES "Space missing before filename: %s"
#define SEDERR_TMMES "too much command text: %s"
#define SEDERR_LTLMES "label too long: %s"
#define SEDERR_ULMES "undefined label: %s"
#define SEDERR_DLMES "duplicate labels: %s"
#define SEDERR_TMLMES "too many labels: %s"
#define SEDERR_AD0MES "no addresses allowed: %s"
#define SEDERR_AD1MES "only one address allowed: %s"
#define SEDERR_TOOBIG "suffix too large: %s"
#define SEDERR_OOMMES "out of memory"
#define SEDERR_COPFMES "cannot open pattern file: %s"
#define SEDERR_COIFMES "cannot open input file: %s"
#define SEDERR_TMOMES "too many {'s"
#define SEDERR_TMCMES "too many }'s"
#define SEDERR_NRMES "first RE may not be null"
#define SEDERR_UCMES "unrecognized command: %s"
#define SEDERR_TMWFMES "too many files in w commands"
#define SEDERR_COMES "cannot open %s"
#define SEDERR_CCMES "cannot create %s"
#define SEDERR_TMLNMES "too many line numbers"
#define SEDERR_TMAMES "too many appends after line %lld"
#define SEDERR_TMRMES "too many reads after line %lld"
#define SEDERR_DOORNG "``\\digit'' out of range: %s"
#define SEDERR_EDMOSUB "ending delimiter missing on substitution: %s"
#define SEDERR_EDMOSTR "ending delimiter missing on string: %s"
#define SEDERR_FNTL "file name too long: %s"
#define SEDERR_CLTL "command line too long"
#define SEDERR_TSNTSS "transform strings not the same size: %s"
#define SEDERR_OLTL "output line too long."
#define SEDERR_HSOVERFLOW "hold space overflowed."
#define SEDERR_INTERNAL "internal sed error"
#if defined(__cplusplus)
}
#endif
#endif