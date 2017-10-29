#include "apr.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
module AP_MODULE_DECLARE_DATA version_module;
static ap_version_t httpd_version;
static int compare_version(char *version_string, const char **error) {
char *p = version_string, *ep;
int version[3] = {0, 0, 0};
int c = 0;
*error = "Version appears to be invalid. It must have the format "
"major[.minor[.patch]] where major, minor and patch are "
"numbers.";
if (!apr_isdigit(*p)) {
return 0;
}
ep = version_string + strlen(version_string);
while (p <= ep && c < 3) {
if (*p == '.') {
*p = '\0';
}
if (!*p) {
version[c++] = atoi(version_string);
version_string = ++p;
continue;
}
if (!apr_isdigit(*p)) {
break;
}
++p;
}
if (p < ep) {
return 0;
}
*error = NULL;
if (httpd_version.major > version[0]) {
return 1;
} else if (httpd_version.major < version[0]) {
return -1;
} else if (httpd_version.minor > version[1]) {
return 1;
} else if (httpd_version.minor < version[1]) {
return -1;
} else if (httpd_version.patch > version[2]) {
return 1;
} else if (httpd_version.patch < version[2]) {
return -1;
}
return 0;
}
static int match_version(apr_pool_t *pool, char *version_string,
const char **error) {
ap_regex_t *compiled;
const char *to_match;
int rc;
compiled = ap_pregcomp(pool, version_string, AP_REG_EXTENDED);
if (!compiled) {
*error = "Unable to compile regular expression";
return 0;
}
*error = NULL;
to_match = apr_psprintf(pool, "%d.%d.%d%s",
httpd_version.major,
httpd_version.minor,
httpd_version.patch,
httpd_version.add_string);
rc = !ap_regexec(compiled, to_match, 0, NULL, 0);
ap_pregfree(pool, compiled);
return rc;
}
static const char *start_ifversion(cmd_parms *cmd, void *mconfig,
const char *arg1, const char *arg2,
const char *arg3) {
const char *endp;
int reverse = 0, done = 0, match = 0, compare;
const char *p, *error;
char c;
if (!arg2) {
arg2 = arg1;
arg1 = "=";
}
if (!arg3 && *arg2 == '>' && !arg2[1]) {
arg3 = ">";
arg2 = arg1;
arg1 = "=";
}
endp = arg2 + strlen(arg2);
if ( endp == arg2
|| (!(arg3 && *arg3 == '>' && !arg3[1]) && *--endp != '>')) {
return apr_pstrcat(cmd->pool, cmd->cmd->name,
"> directive missing closing '>'", NULL);
}
p = arg1;
if (*p == '!') {
reverse = 1;
if (p[1]) {
++p;
}
}
c = *p++;
if (!*p || (*p == '=' && !p[1] && c != '~')) {
if (!httpd_version.major) {
ap_get_server_revision(&httpd_version);
}
done = 1;
switch (c) {
case '=':
if (*arg2 != '/') {
compare = compare_version(apr_pstrmemdup(cmd->temp_pool, arg2,
endp-arg2),
&error);
if (error) {
return error;
}
match = !compare;
break;
}
if (endp == ++arg2 || *--endp != '/') {
return "Missing delimiting / of regular expression.";
}
case '~':
match = match_version(cmd->temp_pool,
apr_pstrmemdup(cmd->temp_pool, arg2,
endp-arg2),
&error);
if (error) {
return error;
}
break;
case '<':
compare = compare_version(apr_pstrmemdup(cmd->temp_pool, arg2,
endp-arg2),
&error);
if (error) {
return error;
}
match = ((-1 == compare) || (*p && !compare));
break;
case '>':
compare = compare_version(apr_pstrmemdup(cmd->temp_pool, arg2,
endp-arg2),
&error);
if (error) {
return error;
}
match = ((1 == compare) || (*p && !compare));
break;
default:
done = 0;
break;
}
}
if (!done) {
return apr_pstrcat(cmd->pool, "unrecognized operator '", arg1, "'",
NULL);
}
if ((!reverse && match) || (reverse && !match)) {
ap_directive_t *parent = NULL;
ap_directive_t *current = NULL;
const char *retval;
retval = ap_build_cont_config(cmd->pool, cmd->temp_pool, cmd,
&current, &parent, "<IfVersion");
*(ap_directive_t **)mconfig = current;
return retval;
}
*(ap_directive_t **)mconfig = NULL;
return ap_soak_end_container(cmd, "<IfVersion");
}
static const command_rec version_cmds[] = {
AP_INIT_TAKE123("<IfVersion", start_ifversion, NULL, EXEC_ON_READ | OR_ALL,
"a comparison operator, a version (and a delimiter)"),
{ NULL }
};
AP_DECLARE_MODULE(version) = {
STANDARD20_MODULE_STUFF,
NULL,
NULL,
NULL,
NULL,
version_cmds,
NULL,
};