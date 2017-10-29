#include "ssl_private.h"
static int app_data2_idx = -1;
void modssl_init_app_data2_idx(void) {
int i;
if (app_data2_idx > -1) {
return;
}
for (i = 0; i <= 1; i++) {
app_data2_idx =
SSL_get_ex_new_index(0,
"Second Application Data for SSL",
NULL, NULL, NULL);
}
}
void *modssl_get_app_data2(SSL *ssl) {
return (void *)SSL_get_ex_data(ssl, app_data2_idx);
}
void modssl_set_app_data2(SSL *ssl, void *arg) {
SSL_set_ex_data(ssl, app_data2_idx, (char *)arg);
return;
}
EVP_PKEY *modssl_read_privatekey(const char* filename, EVP_PKEY **key, pem_password_cb *cb, void *s) {
EVP_PKEY *rc;
BIO *bioS;
BIO *bioF;
if ((bioS=BIO_new_file(filename, "r")) == NULL)
return NULL;
rc = PEM_read_bio_PrivateKey(bioS, key, cb, s);
BIO_free(bioS);
if (rc == NULL) {
if ((bioS = BIO_new_file(filename, "r")) == NULL)
return NULL;
if ((bioF = BIO_new(BIO_f_base64())) == NULL) {
BIO_free(bioS);
return NULL;
}
bioS = BIO_push(bioF, bioS);
rc = d2i_PrivateKey_bio(bioS, NULL);
BIO_free_all(bioS);
if (rc == NULL) {
if ((bioS = BIO_new_file(filename, "r")) == NULL)
return NULL;
rc = d2i_PrivateKey_bio(bioS, NULL);
BIO_free(bioS);
}
}
if (rc != NULL && key != NULL) {
if (*key != NULL)
EVP_PKEY_free(*key);
*key = rc;
}
return rc;
}
int modssl_smart_shutdown(SSL *ssl) {
int i;
int rc;
int flush;
rc = 0;
flush = !(SSL_get_shutdown(ssl) & SSL_SENT_SHUTDOWN);
for (i = 0; i < 4 ; i++) {
rc = SSL_shutdown(ssl);
if (rc >= 0 && flush && (SSL_get_shutdown(ssl) & SSL_SENT_SHUTDOWN)) {
if (BIO_flush(SSL_get_wbio(ssl)) <= 0) {
rc = -1;
break;
}
flush = 0;
}
if (rc != 0)
break;
}
return rc;
}
BOOL modssl_X509_getBC(X509 *cert, int *ca, int *pathlen) {
BASIC_CONSTRAINTS *bc;
BIGNUM *bn = NULL;
char *cp;
bc = X509_get_ext_d2i(cert, NID_basic_constraints, NULL, NULL);
if (bc == NULL)
return FALSE;
*ca = bc->ca;
*pathlen = -1 ;
if (bc->pathlen != NULL) {
if ((bn = ASN1_INTEGER_to_BN(bc->pathlen, NULL)) == NULL) {
BASIC_CONSTRAINTS_free(bc);
return FALSE;
}
if ((cp = BN_bn2dec(bn)) == NULL) {
BN_free(bn);
BASIC_CONSTRAINTS_free(bc);
return FALSE;
}
*pathlen = atoi(cp);
OPENSSL_free(cp);
BN_free(bn);
}
BASIC_CONSTRAINTS_free(bc);
return TRUE;
}
static char *asn1_string_to_utf8(apr_pool_t *p, ASN1_STRING *asn1str) {
char *result = NULL;
BIO *bio;
int len;
if ((bio = BIO_new(BIO_s_mem())) == NULL)
return NULL;
ASN1_STRING_print_ex(bio, asn1str, ASN1_STRFLGS_ESC_CTRL|
ASN1_STRFLGS_UTF8_CONVERT);
len = BIO_pending(bio);
if (len > 0) {
result = apr_palloc(p, len+1);
len = BIO_read(bio, result, len);
result[len] = NUL;
}
BIO_free(bio);
return result;
}
char *modssl_X509_NAME_ENTRY_to_string(apr_pool_t *p, X509_NAME_ENTRY *xsne) {
char *result = asn1_string_to_utf8(p, X509_NAME_ENTRY_get_data(xsne));
ap_xlate_proto_from_ascii(result, len);
return result;
}
char *modssl_X509_NAME_to_string(apr_pool_t *p, X509_NAME *dn, int maxlen) {
char *result = NULL;
BIO *bio;
int len;
if ((bio = BIO_new(BIO_s_mem())) == NULL)
return NULL;
X509_NAME_print_ex(bio, dn, 0, XN_FLAG_RFC2253);
len = BIO_pending(bio);
if (len > 0) {
result = apr_palloc(p, (maxlen > 0) ? maxlen+1 : len+1);
if (maxlen > 0 && maxlen < len) {
len = BIO_read(bio, result, maxlen);
if (maxlen > 2) {
apr_snprintf(result + maxlen - 3, 4, "...");
}
} else {
len = BIO_read(bio, result, len);
}
result[len] = NUL;
}
BIO_free(bio);
return result;
}
static void parse_otherName_value(apr_pool_t *p, ASN1_TYPE *value,
const char *onf, apr_array_header_t **entries) {
const char *str;
int nid = onf ? OBJ_txt2nid(onf) : NID_undef;
if (!value || (nid == NID_undef) || !*entries)
return;
if ((nid == NID_ms_upn) && (value->type == V_ASN1_UTF8STRING) &&
(str = asn1_string_to_utf8(p, value->value.utf8string))) {
APR_ARRAY_PUSH(*entries, const char *) = str;
} else if (strEQ(onf, "id-on-dnsSRV") &&
(value->type == V_ASN1_IA5STRING) &&
(str = asn1_string_to_utf8(p, value->value.ia5string))) {
APR_ARRAY_PUSH(*entries, const char *) = str;
}
}
BOOL modssl_X509_getSAN(apr_pool_t *p, X509 *x509, int type, const char *onf,
int idx, apr_array_header_t **entries) {
STACK_OF(GENERAL_NAME) *names;
int nid = onf ? OBJ_txt2nid(onf) : NID_undef;
if (!x509 || (type < GEN_OTHERNAME) ||
((type == GEN_OTHERNAME) && (nid == NID_undef)) ||
(type > GEN_RID) || (idx < -1) ||
!(*entries = apr_array_make(p, 0, sizeof(char *)))) {
*entries = NULL;
return FALSE;
}
if ((names = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL))) {
int i, n = 0;
GENERAL_NAME *name;
const char *utf8str;
for (i = 0; i < sk_GENERAL_NAME_num(names); i++) {
name = sk_GENERAL_NAME_value(names, i);
if (name->type != type)
continue;
switch (type) {
case GEN_EMAIL:
case GEN_DNS:
if (((idx == -1) || (n == idx)) &&
(utf8str = asn1_string_to_utf8(p, name->d.ia5))) {
APR_ARRAY_PUSH(*entries, const char *) = utf8str;
}
n++;
break;
case GEN_OTHERNAME:
if (OBJ_obj2nid(name->d.otherName->type_id) == nid) {
if (((idx == -1) || (n == idx))) {
parse_otherName_value(p, name->d.otherName->value,
onf, entries);
}
n++;
}
break;
default:
break;
}
if ((idx != -1) && (n > idx))
break;
}
sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
}
return apr_is_empty_array(*entries) ? FALSE : TRUE;
}
static BOOL getIDs(apr_pool_t *p, X509 *x509, apr_array_header_t **ids) {
X509_NAME *subj;
int i = -1;
if (!x509 ||
(modssl_X509_getSAN(p, x509, GEN_DNS, NULL, -1, ids) == FALSE && !*ids)) {
*ids = NULL;
return FALSE;
}
subj = X509_get_subject_name(x509);
while ((i = X509_NAME_get_index_by_NID(subj, NID_commonName, i)) != -1) {
APR_ARRAY_PUSH(*ids, const char *) =
modssl_X509_NAME_ENTRY_to_string(p, X509_NAME_get_entry(subj, i));
}
return apr_is_empty_array(*ids) ? FALSE : TRUE;
}
BOOL modssl_X509_match_name(apr_pool_t *p, X509 *x509, const char *name,
BOOL allow_wildcard, server_rec *s) {
BOOL matched = FALSE;
apr_array_header_t *ids;
if (getIDs(p, x509, &ids)) {
const char *cp;
int i;
char **id = (char **)ids->elts;
BOOL is_wildcard;
for (i = 0; i < ids->nelts; i++) {
if (!id[i])
continue;
is_wildcard = (*id[i] == '*' && *(id[i]+1) == '.') ? TRUE : FALSE;
if ((allow_wildcard == TRUE && is_wildcard == TRUE &&
(cp = ap_strchr_c(name, '.')) && !strcasecmp(id[i]+1, cp)) ||
!strcasecmp(id[i], name)) {
matched = TRUE;
}
if (s) {
ap_log_error(APLOG_MARK, APLOG_TRACE3, 0, s,
"[%s] modssl_X509_match_name: expecting name '%s', "
"%smatched by ID '%s'",
(mySrvConfig(s))->vhost_id, name,
matched == TRUE ? "" : "NOT ", id[i]);
}
if (matched == TRUE) {
break;
}
}
}
if (s) {
ssl_log_xerror(SSLLOG_MARK, APLOG_DEBUG, 0, p, s, x509,
APLOGNO(02412) "[%s] Cert %s for name '%s'",
(mySrvConfig(s))->vhost_id,
matched == TRUE ? "matches" : "does not match",
name);
}
return matched;
}
DH *ssl_dh_GetParamFromFile(const char *file) {
DH *dh = NULL;
BIO *bio;
if ((bio = BIO_new_file(file, "r")) == NULL)
return NULL;
dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
BIO_free(bio);
return (dh);
}
#if defined(HAVE_ECC)
EC_GROUP *ssl_ec_GetParamFromFile(const char *file) {
EC_GROUP *group = NULL;
BIO *bio;
if ((bio = BIO_new_file(file, "r")) == NULL)
return NULL;
group = PEM_read_bio_ECPKParameters(bio, NULL, NULL, NULL);
BIO_free(bio);
return (group);
}
#endif
char *modssl_SSL_SESSION_id2sz(IDCONST unsigned char *id, int idlen,
char *str, int strsize) {
if (idlen > SSL_MAX_SSL_SESSION_ID_LENGTH)
idlen = SSL_MAX_SSL_SESSION_ID_LENGTH;
if (idlen > (strsize-1) / 2)
idlen = (strsize-1) / 2;
ap_bin2hex(id, idlen, str);
return str;
}