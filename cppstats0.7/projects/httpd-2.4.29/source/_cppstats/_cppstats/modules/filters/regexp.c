#include "apr.h"
#include "apr_lib.h"
#if APR_HAVE_LIMITS_H
#include <limits.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "libsed.h"
#include "regexp.h"
#include "sed.h"
#define GETC() ((unsigned char)*sp++)
#define PEEKC() ((unsigned char)*sp)
#define UNGETC(c) (--sp)
#define SEDCOMPILE_ERROR(c) { regerrno = c; goto out; }
#define ecmp(s1, s2, n) (strncmp(s1, s2, n) == 0)
#define uletter(c) (isalpha(c) || c == '_')
static unsigned char bittab[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
static int regerr(sed_commands_t *commands, int err);
static void comperr(sed_commands_t *commands, char *msg);
static void getrnge(char *str, step_vars_storage *vars);
static int _advance(char *, char *, step_vars_storage *);
extern int sed_step(char *p1, char *p2, int circf, step_vars_storage *vars);
static void comperr(sed_commands_t *commands, char *msg) {
command_errf(commands, msg, commands->linebuf);
}
static int regerr(sed_commands_t *commands, int err) {
switch(err) {
case 0:
break;
case 11:
comperr(commands, "Range endpoint too large: %s");
break;
case 16:
comperr(commands, "Bad number: %s");
break;
case 25:
comperr(commands, "``\\digit'' out of range: %s");
break;
case 36:
comperr(commands, "Illegal or missing delimiter: %s");
break;
case 41:
comperr(commands, "No remembered search string: %s");
break;
case 42:
comperr(commands, "\\( \\) imbalance: %s");
break;
case 43:
comperr(commands, "Too many \\(: %s");
break;
case 44:
comperr(commands, "More than 2 numbers given in \\{ \\}: %s");
break;
case 45:
comperr(commands, "} expected after \\: %s");
break;
case 46:
comperr(commands, "First number exceeds second in \\{ \\}: %s");
break;
case 49:
comperr(commands, "[ ] imbalance: %s");
break;
case 50:
comperr(commands, SEDERR_TMMES);
break;
default:
comperr(commands, "Unknown regexp error code %s\n");
break;
}
return (0);
}
char *sed_compile(sed_commands_t *commands, sed_comp_args *compargs,
char *ep, char *endbuf, int seof) {
int c;
int eof = seof;
char *lastep;
int cclcnt;
char bracket[NBRA], *bracketp;
int closed;
int neg;
int lc;
int i, cflg;
int iflag;
char *sp = commands->cp;
int regerrno = 0;
lastep = 0;
if ((c = GETC()) == eof || c == '\n') {
if (c == '\n') {
UNGETC(c);
}
commands->cp = sp;
goto out;
}
bracketp = bracket;
compargs->circf = closed = compargs->nbra = 0;
if (c == '^')
compargs->circf++;
else
UNGETC(c);
while (1) {
if (ep >= endbuf)
SEDCOMPILE_ERROR(50);
c = GETC();
if (c != '*' && ((c != '\\') || (PEEKC() != '{')))
lastep = ep;
if (c == eof) {
*ep++ = CCEOF;
if (bracketp != bracket)
SEDCOMPILE_ERROR(42);
commands->cp = sp;
goto out;
}
switch (c) {
case '.':
*ep++ = CDOT;
continue;
case '\n':
SEDCOMPILE_ERROR(36);
commands->cp = sp;
goto out;
case '*':
if (lastep == 0 || *lastep == CBRA || *lastep == CKET)
goto defchar;
*lastep |= STAR;
continue;
case '$':
if (PEEKC() != eof && PEEKC() != '\n')
goto defchar;
*ep++ = CDOL;
continue;
case '[':
if (&ep[17] >= endbuf)
SEDCOMPILE_ERROR(50);
*ep++ = CCL;
lc = 0;
for (i = 0; i < 16; i++)
ep[i] = 0;
neg = 0;
if ((c = GETC()) == '^') {
neg = 1;
c = GETC();
}
iflag = 1;
do {
c &= 0377;
if (c == '\0' || c == '\n')
SEDCOMPILE_ERROR(49);
if ((c & 0200) && iflag) {
iflag = 0;
if (&ep[32] >= endbuf)
SEDCOMPILE_ERROR(50);
ep[-1] = CXCL;
for (i = 16; i < 32; i++)
ep[i] = 0;
}
if (c == '-' && lc != 0) {
if ((c = GETC()) == ']') {
PLACE('-');
break;
}
if ((c & 0200) && iflag) {
iflag = 0;
if (&ep[32] >= endbuf)
SEDCOMPILE_ERROR(50);
ep[-1] = CXCL;
for (i = 16; i < 32; i++)
ep[i] = 0;
}
while (lc < c) {
PLACE(lc);
lc++;
}
}
lc = c;
PLACE(c);
} while ((c = GETC()) != ']');
if (iflag)
iflag = 16;
else
iflag = 32;
if (neg) {
if (iflag == 32) {
for (cclcnt = 0; cclcnt < iflag;
cclcnt++)
ep[cclcnt] ^= 0377;
ep[0] &= 0376;
} else {
ep[-1] = NCCL;
ep[0] |= 01;
}
}
ep += iflag;
continue;
case '\\':
switch (c = GETC()) {
case '(':
if (compargs->nbra >= NBRA)
SEDCOMPILE_ERROR(43);
*bracketp++ = compargs->nbra;
*ep++ = CBRA;
*ep++ = compargs->nbra++;
continue;
case ')':
if (bracketp <= bracket)
SEDCOMPILE_ERROR(42);
*ep++ = CKET;
*ep++ = *--bracketp;
closed++;
continue;
case '{':
if (lastep == (char *) 0)
goto defchar;
*lastep |= RNGE;
cflg = 0;
nlim:
c = GETC();
i = 0;
do {
if ('0' <= c && c <= '9')
i = 10 * i + c - '0';
else
SEDCOMPILE_ERROR(16);
} while (((c = GETC()) != '\\') && (c != ','));
if (i >= 255)
SEDCOMPILE_ERROR(11);
*ep++ = i;
if (c == ',') {
if (cflg++)
SEDCOMPILE_ERROR(44);
if ((c = GETC()) == '\\')
*ep++ = (char) 255;
else {
UNGETC(c);
goto nlim;
}
}
if (GETC() != '}')
SEDCOMPILE_ERROR(45);
if (!cflg)
*ep++ = i;
else if ((ep[-1] & 0377) < (ep[-2] & 0377))
SEDCOMPILE_ERROR(46);
continue;
case '\n':
SEDCOMPILE_ERROR(36);
case 'n':
c = '\n';
goto defchar;
default:
if (c >= '1' && c <= '9') {
if ((c -= '1') >= closed)
SEDCOMPILE_ERROR(25);
*ep++ = CBACK;
*ep++ = c;
continue;
}
}
defchar:
default:
lastep = ep;
*ep++ = CCHR;
*ep++ = c;
}
}
out:
if (regerrno) {
regerr(commands, regerrno);
return (char*) NULL;
}
return ep;
}
int sed_step(char *p1, char *p2, int circf, step_vars_storage *vars) {
int c;
if (circf) {
vars->loc1 = p1;
return (_advance(p1, p2, vars));
}
if (*p2 == CCHR) {
c = p2[1];
do {
if (*p1 != c)
continue;
if (_advance(p1, p2, vars)) {
vars->loc1 = p1;
return (1);
}
} while (*p1++);
return (0);
}
do {
if (_advance(p1, p2, vars)) {
vars->loc1 = p1;
return (1);
}
} while (*p1++);
return (0);
}
static int _advance(char *lp, char *ep, step_vars_storage *vars) {
char *curlp;
int c;
char *bbeg;
char neg;
int ct;
int epint;
while (1) {
neg = 0;
switch (*ep++) {
case CCHR:
if (*ep++ == *lp++)
continue;
return (0);
case CDOT:
if (*lp++)
continue;
return (0);
case CDOL:
if (*lp == 0)
continue;
return (0);
case CCEOF:
vars->loc2 = lp;
return (1);
case CXCL:
c = (unsigned char)*lp++;
if (ISTHERE(c)) {
ep += 32;
continue;
}
return (0);
case NCCL:
neg = 1;
case CCL:
c = *lp++;
if (((c & 0200) == 0 && ISTHERE(c)) ^ neg) {
ep += 16;
continue;
}
return (0);
case CBRA:
epint = (int) *ep;
vars->braslist[epint] = lp;
ep++;
continue;
case CKET:
epint = (int) *ep;
vars->braelist[epint] = lp;
ep++;
continue;
case CCHR | RNGE:
c = *ep++;
getrnge(ep, vars);
while (vars->low--)
if (*lp++ != c)
return (0);
curlp = lp;
while (vars->size--)
if (*lp++ != c)
break;
if (vars->size < 0)
lp++;
ep += 2;
goto star;
case CDOT | RNGE:
getrnge(ep, vars);
while (vars->low--)
if (*lp++ == '\0')
return (0);
curlp = lp;
while (vars->size--)
if (*lp++ == '\0')
break;
if (vars->size < 0)
lp++;
ep += 2;
goto star;
case CXCL | RNGE:
getrnge(ep + 32, vars);
while (vars->low--) {
c = (unsigned char)*lp++;
if (!ISTHERE(c))
return (0);
}
curlp = lp;
while (vars->size--) {
c = (unsigned char)*lp++;
if (!ISTHERE(c))
break;
}
if (vars->size < 0)
lp++;
ep += 34;
goto star;
case NCCL | RNGE:
neg = 1;
case CCL | RNGE:
getrnge(ep + 16, vars);
while (vars->low--) {
c = *lp++;
if (((c & 0200) || !ISTHERE(c)) ^ neg)
return (0);
}
curlp = lp;
while (vars->size--) {
c = *lp++;
if (((c & 0200) || !ISTHERE(c)) ^ neg)
break;
}
if (vars->size < 0)
lp++;
ep += 18;
goto star;
case CBACK:
epint = (int) *ep;
bbeg = vars->braslist[epint];
ct = vars->braelist[epint] - bbeg;
ep++;
if (ecmp(bbeg, lp, ct)) {
lp += ct;
continue;
}
return (0);
case CBACK | STAR:
epint = (int) *ep;
bbeg = vars->braslist[epint];
ct = vars->braelist[epint] - bbeg;
ep++;
curlp = lp;
while (ecmp(bbeg, lp, ct))
lp += ct;
while (lp >= curlp) {
if (_advance(lp, ep, vars))
return (1);
lp -= ct;
}
return (0);
case CDOT | STAR:
curlp = lp;
while (*lp++);
goto star;
case CCHR | STAR:
curlp = lp;
while (*lp++ == *ep);
ep++;
goto star;
case CXCL | STAR:
curlp = lp;
do {
c = (unsigned char)*lp++;
} while (ISTHERE(c));
ep += 32;
goto star;
case NCCL | STAR:
neg = 1;
case CCL | STAR:
curlp = lp;
do {
c = *lp++;
} while (((c & 0200) == 0 && ISTHERE(c)) ^ neg);
ep += 16;
goto star;
star:
do {
if (--lp == vars->locs)
break;
if (_advance(lp, ep, vars))
return (1);
} while (lp > curlp);
return (0);
}
}
}
static void getrnge(char *str, step_vars_storage *vars) {
vars->low = *str++ & 0377;
vars->size = ((*str & 0377) == 255)? 20000: (*str &0377) - vars->low;
}