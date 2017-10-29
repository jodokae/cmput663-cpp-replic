#include "apr.h"
#include "apr_lib.h"
#include "apr_hash.h"
#include "apr_getopt.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_network_io.h"
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#define READ_BUF_SIZE 128*1024
#define WRITE_BUF_SIZE 128*1024
#define LINE_BUF_SIZE 128*1024
static apr_file_t *errfile;
static const char *shortname = "logresolve";
static apr_hash_t *cache;
static int cachehits = 0;
static int cachesize = 0;
static int entries = 0;
static int resolves = 0;
static int withname = 0;
static int doublefailed = 0;
static int noreverse = 0;
#define NL APR_EOL_STR
static void print_statistics (apr_file_t *output) {
apr_file_printf(output, "logresolve Statistics:" NL);
apr_file_printf(output, "Entries: %d" NL, entries);
apr_file_printf(output, " With name : %d" NL, withname);
apr_file_printf(output, " Resolves : %d" NL, resolves);
if (noreverse) {
apr_file_printf(output, " - No reverse : %d" NL,
noreverse);
}
if (doublefailed) {
apr_file_printf(output, " - Double lookup failed : %d" NL,
doublefailed);
}
apr_file_printf(output, "Cache hits : %d" NL, cachehits);
apr_file_printf(output, "Cache size : %d" NL, cachesize);
}
static void usage(void) {
apr_file_printf(errfile,
"%s -- Resolve IP-addresses to hostnames in Apache log files." NL
"Usage: %s [-s STATFILE] [-c]" NL
NL
"Options:" NL
" -s Record statistics to STATFILE when finished." NL
NL
" -c Perform double lookups when resolving IP addresses." NL,
shortname, shortname);
exit(1);
}
#undef NL
int main(int argc, const char * const argv[]) {
apr_file_t * outfile;
apr_file_t * infile;
apr_getopt_t * o;
apr_pool_t * pool;
apr_pool_t *pline;
apr_status_t status;
const char * arg;
char * stats = NULL;
char * inbuffer;
char * outbuffer;
char * line;
int doublelookups = 0;
if (apr_app_initialize(&argc, &argv, NULL) != APR_SUCCESS) {
return 1;
}
atexit(apr_terminate);
if (argc) {
shortname = apr_filepath_name_get(argv[0]);
}
if (apr_pool_create(&pool, NULL) != APR_SUCCESS) {
return 1;
}
apr_file_open_stderr(&errfile, pool);
apr_getopt_init(&o, pool, argc, argv);
while (1) {
char opt;
status = apr_getopt(o, "s:c", &opt, &arg);
if (status == APR_EOF) {
break;
} else if (status != APR_SUCCESS) {
usage();
} else {
switch (opt) {
case 'c':
if (doublelookups) {
usage();
}
doublelookups = 1;
break;
case 's':
if (stats) {
usage();
}
stats = apr_pstrdup(pool, arg);
break;
}
}
}
apr_file_open_stdout(&outfile, pool);
apr_file_open_stdin(&infile, pool);
if ( (outbuffer = apr_palloc(pool, WRITE_BUF_SIZE)) == NULL
|| (inbuffer = apr_palloc(pool, READ_BUF_SIZE)) == NULL
|| (line = apr_palloc(pool, LINE_BUF_SIZE)) == NULL) {
return 1;
}
apr_file_buffer_set(infile, inbuffer, READ_BUF_SIZE);
apr_file_buffer_set(outfile, outbuffer, WRITE_BUF_SIZE);
cache = apr_hash_make(pool);
if (apr_pool_create(&pline, pool) != APR_SUCCESS) {
return 1;
}
while (apr_file_gets(line, LINE_BUF_SIZE, infile) == APR_SUCCESS) {
char *hostname;
char *space;
apr_sockaddr_t *ip;
apr_sockaddr_t *ipdouble;
char dummy[] = " " APR_EOL_STR;
if (line[0] == '\0') {
continue;
}
entries++;
if (!apr_isxdigit(line[0]) && line[0] != ':') {
withname++;
apr_file_puts(line, outfile);
continue;
}
if ((space = strchr(line, ' ')) != NULL) {
*space = '\0';
} else {
space = dummy;
}
hostname = (char *) apr_hash_get(cache, line, APR_HASH_KEY_STRING);
if (hostname) {
apr_file_printf(outfile, "%s %s", hostname, space + 1);
cachehits++;
continue;
}
status = apr_sockaddr_info_get(&ip, line, APR_UNSPEC, 0, 0, pline);
if (status != APR_SUCCESS) {
withname++;
*space = ' ';
apr_file_puts(line, outfile);
continue;
}
resolves++;
cachesize++;
status = apr_getnameinfo(&hostname, ip, 0) != APR_SUCCESS;
if (status || hostname == NULL) {
*space = ' ';
apr_file_puts(line, outfile);
noreverse++;
*space = '\0';
apr_hash_set(cache, line, APR_HASH_KEY_STRING,
apr_pstrdup(apr_hash_pool_get(cache), line));
continue;
}
if (doublelookups) {
status = apr_sockaddr_info_get(&ipdouble, hostname, ip->family, 0,
0, pline);
if (status == APR_SUCCESS ||
memcmp(ipdouble->ipaddr_ptr, ip->ipaddr_ptr, ip->ipaddr_len)) {
*space = ' ';
apr_file_puts(line, outfile);
doublefailed++;
*space = '\0';
apr_hash_set(cache, line, APR_HASH_KEY_STRING,
apr_pstrdup(apr_hash_pool_get(cache), line));
continue;
}
}
apr_file_printf(outfile, "%s %s", hostname, space + 1);
apr_hash_set(cache, line, APR_HASH_KEY_STRING,
apr_pstrdup(apr_hash_pool_get(cache), hostname));
apr_pool_clear(pline);
}
apr_file_flush(outfile);
if (stats) {
apr_file_t *statsfile;
if (apr_file_open(&statsfile, stats,
APR_FOPEN_WRITE | APR_FOPEN_CREATE | APR_FOPEN_TRUNCATE,
APR_OS_DEFAULT, pool) != APR_SUCCESS) {
apr_file_printf(errfile, "%s: Could not open %s for writing.",
shortname, stats);
return 1;
}
print_statistics(statsfile);
apr_file_close(statsfile);
}
return 0;
}
