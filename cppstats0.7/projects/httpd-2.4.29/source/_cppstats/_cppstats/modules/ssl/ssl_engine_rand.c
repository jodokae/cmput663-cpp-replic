#include "ssl_private.h"
static int ssl_rand_choosenum(int, int);
static int ssl_rand_feedfp(apr_pool_t *, apr_file_t *, int);
int ssl_rand_seed(server_rec *s, apr_pool_t *p, ssl_rsctx_t nCtx, char *prefix) {
SSLModConfigRec *mc;
apr_array_header_t *apRandSeed;
ssl_randseed_t *pRandSeeds;
ssl_randseed_t *pRandSeed;
unsigned char stackdata[256];
int nDone;
apr_file_t *fp;
int i, n, l;
mc = myModConfig(s);
nDone = 0;
apRandSeed = mc->aRandSeed;
pRandSeeds = (ssl_randseed_t *)apRandSeed->elts;
for (i = 0; i < apRandSeed->nelts; i++) {
pRandSeed = &pRandSeeds[i];
if (pRandSeed->nCtx == nCtx) {
if (pRandSeed->nSrc == SSL_RSSRC_FILE) {
if (apr_file_open(&fp, pRandSeed->cpPath,
APR_READ, APR_OS_DEFAULT, p) != APR_SUCCESS)
continue;
nDone += ssl_rand_feedfp(p, fp, pRandSeed->nBytes);
apr_file_close(fp);
} else if (pRandSeed->nSrc == SSL_RSSRC_EXEC) {
const char *cmd = pRandSeed->cpPath;
const char **argv = apr_palloc(p, sizeof(char *) * 3);
argv[0] = cmd;
argv[1] = apr_itoa(p, pRandSeed->nBytes);
argv[2] = NULL;
if ((fp = ssl_util_ppopen(s, p, cmd, argv)) == NULL)
continue;
nDone += ssl_rand_feedfp(p, fp, pRandSeed->nBytes);
ssl_util_ppclose(s, p, fp);
}
#if defined(HAVE_RAND_EGD)
else if (pRandSeed->nSrc == SSL_RSSRC_EGD) {
if ((n = RAND_egd(pRandSeed->cpPath)) == -1)
continue;
nDone += n;
}
#endif
else if (pRandSeed->nSrc == SSL_RSSRC_BUILTIN) {
struct {
time_t t;
pid_t pid;
} my_seed;
my_seed.t = time(NULL);
my_seed.pid = mc->pid;
l = sizeof(my_seed);
RAND_seed((unsigned char *)&my_seed, l);
nDone += l;
n = ssl_rand_choosenum(0, sizeof(stackdata)-128-1);
RAND_seed(stackdata+n, 128);
nDone += 128;
}
}
}
ap_log_error(APLOG_MARK, APLOG_TRACE2, 0, s,
"%sSeeding PRNG with %d bytes of entropy", prefix, nDone);
if (RAND_status() == 0)
ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(01990)
"%sPRNG still contains insufficient entropy!", prefix);
return nDone;
}
#define BUFSIZE 8192
static int ssl_rand_feedfp(apr_pool_t *p, apr_file_t *fp, int nReq) {
apr_size_t nDone;
unsigned char caBuf[BUFSIZE];
apr_size_t nBuf;
apr_size_t nRead;
apr_size_t nTodo;
nDone = 0;
nRead = BUFSIZE;
nTodo = nReq;
while (1) {
if (nReq > 0)
nRead = (nTodo < BUFSIZE ? nTodo : BUFSIZE);
nBuf = nRead;
if (apr_file_read(fp, caBuf, &nBuf) != APR_SUCCESS)
break;
RAND_seed(caBuf, nBuf);
nDone += nBuf;
if (nReq > 0) {
nTodo -= nBuf;
if (nTodo <= 0)
break;
}
}
return nDone;
}
static int ssl_rand_choosenum(int l, int h) {
int i;
char buf[50];
apr_snprintf(buf, sizeof(buf), "%.0f",
(((double)(rand()%RAND_MAX)/RAND_MAX)*(h-l)));
i = atoi(buf)+1;
if (i < l) i = l;
if (i > h) i = h;
return i;
}
