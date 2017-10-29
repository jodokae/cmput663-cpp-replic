#if !defined(PROMPTER_H)
#define PROMPTER_H
#include <jni.h>
#include "svn_auth.h"
#include <string>
class Prompter {
private:
jobject m_prompter;
bool m_version2;
bool m_version3;
std::string m_answer;
bool m_maySave;
Prompter(jobject jprompter, bool v2, bool v3);
bool prompt(const char *realm, const char *pi_username, bool maySave);
bool askYesNo(const char *realm, const char *question, bool yesIsDefault);
const char *askQuestion(const char *realm, const char *question,
bool showAnswer, bool maySave);
int askTrust(const char *question, bool maySave);
jstring password();
jstring username();
static svn_error_t *simple_prompt(svn_auth_cred_simple_t **cred_p,
void *baton, const char *realm,
const char *username,
svn_boolean_t may_save,
apr_pool_t *pool);
static svn_error_t *username_prompt
(svn_auth_cred_username_t **cred_p,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
static svn_error_t *ssl_server_trust_prompt
(svn_auth_cred_ssl_server_trust_t **cred_p,
void *baton,
const char *realm,
apr_uint32_t failures,
const svn_auth_ssl_server_cert_info_t *cert_info,
svn_boolean_t may_save,
apr_pool_t *pool);
static svn_error_t *ssl_client_cert_prompt
(svn_auth_cred_ssl_client_cert_t **cred_p,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
static svn_error_t *ssl_client_cert_pw_prompt
(svn_auth_cred_ssl_client_cert_pw_t **cred_p,
void *baton,
const char *realm,
svn_boolean_t may_save,
apr_pool_t *pool);
public:
static Prompter *makeCPrompter(jobject jprompter);
~Prompter();
svn_auth_provider_object_t *getProviderUsername();
svn_auth_provider_object_t *getProviderSimple();
svn_auth_provider_object_t *getProviderServerSSLTrust();
svn_auth_provider_object_t *getProviderClientSSL();
svn_auth_provider_object_t *getProviderClientSSLPassword();
};
#endif
