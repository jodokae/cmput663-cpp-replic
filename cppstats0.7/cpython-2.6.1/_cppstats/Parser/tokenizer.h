#if !defined(Py_TOKENIZER_H)
#define Py_TOKENIZER_H
#if defined(__cplusplus)
extern "C" {
#endif
#include "object.h"
#include "token.h"
#define MAXINDENT 100
struct tok_state {
char *buf;
char *cur;
char *inp;
char *end;
char *start;
int done;
FILE *fp;
int tabsize;
int indent;
int indstack[MAXINDENT];
int atbol;
int pendin;
char *prompt, *nextprompt;
int lineno;
int level;
const char *filename;
int altwarning;
int alterror;
int alttabsize;
int altindstack[MAXINDENT];
int decoding_state;
int decoding_erred;
int read_coding_spec;
char *encoding;
int cont_line;
const char* line_start;
#if !defined(PGEN)
PyObject *decoding_readline;
PyObject *decoding_buffer;
#endif
const char* enc;
const char* str;
};
extern struct tok_state *PyTokenizer_FromString(const char *);
extern struct tok_state *PyTokenizer_FromFile(FILE *, char *, char *);
extern void PyTokenizer_Free(struct tok_state *);
extern int PyTokenizer_Get(struct tok_state *, char **, char **);
#if defined(PGEN) || defined(Py_USING_UNICODE)
extern char * PyTokenizer_RestoreEncoding(struct tok_state* tok,
int len, int *offset);
#endif
#if defined(__cplusplus)
}
#endif
#endif
