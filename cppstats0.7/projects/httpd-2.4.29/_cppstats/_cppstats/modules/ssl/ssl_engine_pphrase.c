#include "ssl_private.h"
typedef struct {
server_rec *s;
apr_pool_t *p;
apr_array_header_t *aPassPhrase;
int nPassPhraseCur;
char *cpPassPhraseCur;
int nPassPhraseDialog;
int nPassPhraseDialogCur;
BOOL bPassPhraseDialogOnce;
const char *key_id;
const char *pkey_file;
} pphrase_cb_arg_t;
#if defined(HAVE_ECC)
static const char *key_types[] = {"RSA", "DSA", "ECC"};
#else
static const char *key_types[] = {"RSA", "DSA"};
#endif
static apr_status_t exists_and_readable(const char *fname, apr_pool_t *pool,
apr_time_t *mtime) {
apr_status_t stat;
apr_finfo_t sbuf;
apr_file_t *fd;
if ((stat = apr_stat(&sbuf, fname, APR_FINFO_MIN, pool)) != APR_SUCCESS)
return stat;
if (sbuf.filetype != APR_REG)
return APR_EGENERAL;
if ((stat = apr_file_open(&fd, fname, APR_READ, 0, pool)) != APR_SUCCESS)
return stat;
if (mtime) {
*mtime = sbuf.mtime;
}
apr_file_close(fd);
return APR_SUCCESS;
}
static const char *asn1_table_vhost_key(SSLModConfigRec *mc, apr_pool_t *p,
const char *id, int i) {
char *key = apr_psprintf(p, "%s:%d", id, i);
void *keyptr = apr_hash_get(mc->tVHostKeys, key,
APR_HASH_KEY_STRING);
if (!keyptr) {
keyptr = apr_pstrdup(mc->pPool, key);
apr_hash_set(mc->tVHostKeys, keyptr,
APR_HASH_KEY_STRING, keyptr);
}
return (char *)keyptr;
}
#define BUILTIN_DIALOG_BACKOFF 2
#define BUILTIN_DIALOG_RETRIES 5
static apr_file_t *writetty = NULL;
static apr_file_t *readtty = NULL;
int ssl_pphrase_Handle_CB(char *, int, int, void *);
static char *pphrase_array_get(apr_array_header_t *arr, int idx) {
if ((idx < 0) || (idx >= arr->nelts)) {
return NULL;
}
return ((char **)arr->elts)[idx];
}
apr_status_t ssl_load_encrypted_pkey(server_rec *s, apr_pool_t *p, int idx,
const char *pkey_file,
apr_array_header_t **pphrases) {
SSLModConfigRec *mc = myModConfig(s);
SSLSrvConfigRec *sc = mySrvConfig(s);
const char *key_id = asn1_table_vhost_key(mc, p, sc->vhost_id, idx);
EVP_PKEY *pPrivateKey = NULL;
ssl_asn1_t *asn1;
unsigned char *ucp;
long int length;
BOOL bReadable;
int nPassPhrase = (*pphrases)->nelts;
int nPassPhraseRetry = 0;
apr_time_t pkey_mtime = 0;
apr_status_t rv;
pphrase_cb_arg_t ppcb_arg;
if (!pkey_file) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02573)
"Init: No private key specified for %s", key_id);
return ssl_die(s);
} else if ((rv = exists_and_readable(pkey_file, p, &pkey_mtime))
!= APR_SUCCESS ) {
ap_log_error(APLOG_MARK, APLOG_EMERG, rv, s, APLOGNO(02574)
"Init: Can't open server private key file %s", pkey_file);
return ssl_die(s);
}
ppcb_arg.s = s;
ppcb_arg.p = p;
ppcb_arg.aPassPhrase = *pphrases;
ppcb_arg.nPassPhraseCur = 0;
ppcb_arg.cpPassPhraseCur = NULL;
ppcb_arg.nPassPhraseDialog = 0;
ppcb_arg.nPassPhraseDialogCur = 0;
ppcb_arg.bPassPhraseDialogOnce = TRUE;
ppcb_arg.key_id = key_id;
ppcb_arg.pkey_file = pkey_file;
if (pkey_mtime) {
ssl_asn1_t *asn1 = ssl_asn1_table_get(mc->tPrivateKey, key_id);
if (asn1 && (asn1->source_mtime == pkey_mtime)) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(02575)
"Reusing existing private key from %s on restart",
ppcb_arg.pkey_file);
return APR_SUCCESS;
}
}
ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, APLOGNO(02576)
"Attempting to load encrypted (?) private key %s", key_id);
for (;;) {
ppcb_arg.cpPassPhraseCur = NULL;
ERR_clear_error();
bReadable = ((pPrivateKey = modssl_read_privatekey(ppcb_arg.pkey_file,
NULL, ssl_pphrase_Handle_CB, &ppcb_arg)) != NULL ?
TRUE : FALSE);
if (bReadable)
break;
if (ppcb_arg.nPassPhraseCur < nPassPhrase) {
ppcb_arg.nPassPhraseCur++;
continue;
}
#if !defined(WIN32)
if ((sc->server->pphrase_dialog_type == SSL_PPTYPE_BUILTIN
|| sc->server->pphrase_dialog_type == SSL_PPTYPE_PIPE)
#else
if (sc->server->pphrase_dialog_type == SSL_PPTYPE_PIPE
#endif
&& ppcb_arg.cpPassPhraseCur != NULL
&& nPassPhraseRetry < BUILTIN_DIALOG_RETRIES ) {
apr_file_printf(writetty, "Apache:mod_ssl:Error: Pass phrase incorrect "
"(%d more retr%s permitted).\n",
(BUILTIN_DIALOG_RETRIES-nPassPhraseRetry),
(BUILTIN_DIALOG_RETRIES-nPassPhraseRetry) == 1 ? "y" : "ies");
nPassPhraseRetry++;
if (nPassPhraseRetry > BUILTIN_DIALOG_BACKOFF)
apr_sleep((nPassPhraseRetry-BUILTIN_DIALOG_BACKOFF)
* 5 * APR_USEC_PER_SEC);
continue;
}
#if defined(WIN32)
if (sc->server->pphrase_dialog_type == SSL_PPTYPE_BUILTIN) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02577)
"Init: SSLPassPhraseDialog builtin is not "
"supported on Win32 (key file "
"%s)", ppcb_arg.pkey_file);
return ssl_die(s);
}
#endif
if (ppcb_arg.cpPassPhraseCur == NULL) {
if (ppcb_arg.nPassPhraseDialogCur && pkey_mtime &&
!isatty(fileno(stdout))) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0,
s, APLOGNO(02578)
"Init: Unable to read pass phrase "
"[Hint: key introduced or changed "
"before restart?]");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, s);
} else {
ap_log_error(APLOG_MARK, APLOG_ERR, 0,
s, APLOGNO(02579) "Init: Private key not found");
ssl_log_ssl_error(SSLLOG_MARK, APLOG_ERR, s);
}
if (writetty) {
apr_file_printf(writetty, "Apache:mod_ssl:Error: Private key not found.\n");
apr_file_printf(writetty, "**Stopped\n");
}
} else {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02580)
"Init: Pass phrase incorrect for key %s",
key_id);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
if (writetty) {
apr_file_printf(writetty, "Apache:mod_ssl:Error: Pass phrase incorrect.\n");
apr_file_printf(writetty, "**Stopped\n");
}
}
return ssl_die(s);
}
if (pPrivateKey == NULL) {
ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(02581)
"Init: Unable to read server private key from file %s",
ppcb_arg.pkey_file);
ssl_log_ssl_error(SSLLOG_MARK, APLOG_EMERG, s);
return ssl_die(s);
}
if (ppcb_arg.nPassPhraseDialogCur == 0) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(02582)
"unencrypted %s private key - pass phrase not "
"required", key_id);
} else {
if (ppcb_arg.cpPassPhraseCur != NULL) {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0,
s, APLOGNO(02583)
"encrypted %s private key - pass phrase "
"requested", key_id);
} else {
ap_log_error(APLOG_MARK, APLOG_DEBUG, 0,
s, APLOGNO(02584)
"encrypted %s private key - pass phrase"
" reused", key_id);
}
}
if (ppcb_arg.cpPassPhraseCur != NULL) {
*(const char **)apr_array_push(ppcb_arg.aPassPhrase) =
ppcb_arg.cpPassPhraseCur;
nPassPhrase++;
}
length = i2d_PrivateKey(pPrivateKey, NULL);
ucp = ssl_asn1_table_set(mc->tPrivateKey, key_id, length);
(void)i2d_PrivateKey(pPrivateKey, &ucp);
if (ppcb_arg.nPassPhraseDialogCur != 0) {
asn1 = ssl_asn1_table_get(mc->tPrivateKey, key_id);
asn1->source_mtime = pkey_mtime;
}
EVP_PKEY_free(pPrivateKey);
if ((ppcb_arg.nPassPhraseDialog > 0) &&
(ppcb_arg.cpPassPhraseCur != NULL)) {
if (writetty) {
apr_file_printf(writetty, "\n"
"OK: Pass Phrase Dialog successful.\n");
}
}
if (readtty) {
apr_file_close(readtty);
apr_file_close(writetty);
readtty = writetty = NULL;
}
return APR_SUCCESS;
}
static apr_status_t ssl_pipe_child_create(apr_pool_t *p, const char *progname) {
apr_status_t rc;
apr_procattr_t *procattr;
apr_proc_t *procnew;
if (((rc = apr_procattr_create(&procattr, p)) == APR_SUCCESS) &&
((rc = apr_procattr_io_set(procattr,
APR_FULL_BLOCK,
APR_FULL_BLOCK,
APR_NO_PIPE)) == APR_SUCCESS)) {
char **args;
apr_tokenize_to_argv(progname, &args, p);
procnew = (apr_proc_t *)apr_pcalloc(p, sizeof(*procnew));
rc = apr_proc_create(procnew, args[0], (const char * const *)args,
NULL, procattr, p);
if (rc == APR_SUCCESS) {
writetty = procnew->in;
readtty = procnew->out;
}
}
return rc;
}
static int pipe_get_passwd_cb(char *buf, int length, char *prompt, int verify) {
apr_status_t rc;
char *p;
apr_file_puts(prompt, writetty);
buf[0]='\0';
rc = apr_file_gets(buf, length, readtty);
apr_file_puts(APR_EOL_STR, writetty);
if (rc != APR_SUCCESS || apr_file_eof(readtty)) {
memset(buf, 0, length);
return 1;
}
if ((p = strchr(buf, '\n')) != NULL) {
*p = '\0';
}
#if defined(WIN32)
if ((p = strchr(buf, '\r')) != NULL) {
*p = '\0';
}
#endif
return 0;
}
int ssl_pphrase_Handle_CB(char *buf, int bufsize, int verify, void *srv) {
pphrase_cb_arg_t *ppcb_arg = (pphrase_cb_arg_t *)srv;
SSLSrvConfigRec *sc = mySrvConfig(ppcb_arg->s);
char *cpp;
int len = -1;
ppcb_arg->nPassPhraseDialog++;
ppcb_arg->nPassPhraseDialogCur++;
if ((cpp = pphrase_array_get(ppcb_arg->aPassPhrase,
ppcb_arg->nPassPhraseCur)) != NULL) {
apr_cpystrn(buf, cpp, bufsize);
len = strlen(buf);
return len;
}
if (sc->server->pphrase_dialog_type == SSL_PPTYPE_BUILTIN
|| sc->server->pphrase_dialog_type == SSL_PPTYPE_PIPE) {
char *prompt;
int i;
if (sc->server->pphrase_dialog_type == SSL_PPTYPE_PIPE) {
if (!readtty) {
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ppcb_arg->s,
APLOGNO(01965)
"Init: Creating pass phrase dialog pipe child "
"'%s'", sc->server->pphrase_dialog_path);
if (ssl_pipe_child_create(ppcb_arg->p,
sc->server->pphrase_dialog_path)
!= APR_SUCCESS) {
ap_log_error(APLOG_MARK, APLOG_ERR, 0, ppcb_arg->s,
APLOGNO(01966)
"Init: Failed to create pass phrase pipe '%s'",
sc->server->pphrase_dialog_path);
PEMerr(PEM_F_PEM_DEF_CALLBACK,
PEM_R_PROBLEMS_GETTING_PASSWORD);
memset(buf, 0, (unsigned int)bufsize);
return (-1);
}
}
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ppcb_arg->s, APLOGNO(01967)
"Init: Requesting pass phrase via piped dialog");
} else {
#if defined(WIN32)
PEMerr(PEM_F_PEM_DEF_CALLBACK, PEM_R_PROBLEMS_GETTING_PASSWORD);
memset(buf, 0, (unsigned int)bufsize);
return (-1);
#else
apr_file_open_stdout(&writetty, ppcb_arg->p);
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ppcb_arg->s, APLOGNO(01968)
"Init: Requesting pass phrase via builtin terminal "
"dialog");
#endif
}
if (ppcb_arg->nPassPhraseDialog == 1) {
apr_file_printf(writetty, "%s mod_ssl (Pass Phrase Dialog)\n",
AP_SERVER_BASEVERSION);
apr_file_printf(writetty, "Some of your private key files are encrypted for security reasons.\n");
apr_file_printf(writetty, "In order to read them you have to provide the pass phrases.\n");
}
if (ppcb_arg->bPassPhraseDialogOnce) {
ppcb_arg->bPassPhraseDialogOnce = FALSE;
apr_file_printf(writetty, "\n");
apr_file_printf(writetty, "Private key %s (%s)\n",
ppcb_arg->key_id, ppcb_arg->pkey_file);
}
prompt = "Enter pass phrase:";
for (;;) {
apr_file_puts(prompt, writetty);
if (sc->server->pphrase_dialog_type == SSL_PPTYPE_PIPE) {
i = pipe_get_passwd_cb(buf, bufsize, "", FALSE);
} else {
i = EVP_read_pw_string(buf, bufsize, "", FALSE);
}
if (i != 0) {
PEMerr(PEM_F_PEM_DEF_CALLBACK,PEM_R_PROBLEMS_GETTING_PASSWORD);
memset(buf, 0, (unsigned int)bufsize);
return (-1);
}
len = strlen(buf);
if (len < 1)
apr_file_printf(writetty, "Apache:mod_ssl:Error: Pass phrase empty (needs to be at least 1 character).\n");
else
break;
}
} else if (sc->server->pphrase_dialog_type == SSL_PPTYPE_FILTER) {
const char *cmd = sc->server->pphrase_dialog_path;
const char **argv = apr_palloc(ppcb_arg->p, sizeof(char *) * 4);
const char *idx = ap_strrchr_c(ppcb_arg->key_id, ':') + 1;
char *result;
int i;
ap_log_error(APLOG_MARK, APLOG_INFO, 0, ppcb_arg->s, APLOGNO(01969)
"Init: Requesting pass phrase from dialog filter "
"program (%s)", cmd);
argv[0] = cmd;
argv[1] = apr_pstrndup(ppcb_arg->p, ppcb_arg->key_id,
idx-1 - ppcb_arg->key_id);
if ((i = atoi(idx)) < CERTKEYS_IDX_MAX+1) {
argv[2] = key_types[i];
} else {
argv[2] = apr_pstrdup(ppcb_arg->p, idx);
}
argv[3] = NULL;
result = ssl_util_readfilter(ppcb_arg->s, ppcb_arg->p, cmd, argv);
apr_cpystrn(buf, result, bufsize);
len = strlen(buf);
}
ppcb_arg->cpPassPhraseCur = apr_pstrdup(ppcb_arg->p, buf);
return (len);
}