#include <string.h>
#include <stdlib.h>
#include <apr_pools.h>
#include <apr_time.h>
#include <apr_strings.h>
#include "svn_time.h"
#include "svn_utf.h"
#include "svn_error.h"
#include "svn_private_config.h"
static const char * const timestamp_format =
"%04d-%02d-%02dT%02d:%02d:%02d.%06dZ";
static const char * const old_timestamp_format =
"%3s %d %3s %d %02d:%02d:%02d.%06d (day %03d, dst %d, gmt_off %06d)";
#define SVN_TIME__MAX_LENGTH 80
static const char * const human_timestamp_format =
"%.4d-%.2d-%.2d %.2d:%.2d:%.2d %+.2d%.2d";
#define human_timestamp_format_suffix _(" (%a, %d %b %Y)")
#define SVN_SLEEP_ENV_VAR "SVN_I_LOVE_CORRUPTED_WORKING_COPIES_SO_DISABLE_SLEEP_FOR_TIMESTAMPS"
const char *
svn_time_to_cstring(apr_time_t when, apr_pool_t *pool) {
const char *t_cstr;
apr_time_exp_t exploded_time;
apr_time_exp_gmt(&exploded_time, when);
t_cstr = apr_psprintf(pool,
timestamp_format,
exploded_time.tm_year + 1900,
exploded_time.tm_mon + 1,
exploded_time.tm_mday,
exploded_time.tm_hour,
exploded_time.tm_min,
exploded_time.tm_sec,
exploded_time.tm_usec);
return t_cstr;
}
static int
find_matching_string(char *str, apr_size_t size, const char strings[][4]) {
apr_size_t i;
for (i = 0; i < size; i++)
if (strings[i] && (strcmp(str, strings[i]) == 0))
return i;
return -1;
}
svn_error_t *
svn_time_from_cstring(apr_time_t *when, const char *data, apr_pool_t *pool) {
apr_time_exp_t exploded_time;
apr_status_t apr_err;
char wday[4], month[4];
char *c;
exploded_time.tm_year = strtol(data, &c, 10);
if (*c++ != '-') goto fail;
exploded_time.tm_mon = strtol(c, &c, 10);
if (*c++ != '-') goto fail;
exploded_time.tm_mday = strtol(c, &c, 10);
if (*c++ != 'T') goto fail;
exploded_time.tm_hour = strtol(c, &c, 10);
if (*c++ != ':') goto fail;
exploded_time.tm_min = strtol(c, &c, 10);
if (*c++ != ':') goto fail;
exploded_time.tm_sec = strtol(c, &c, 10);
if (*c++ != '.') goto fail;
exploded_time.tm_usec = strtol(c, &c, 10);
if (*c++ != 'Z') goto fail;
exploded_time.tm_year -= 1900;
exploded_time.tm_mon -= 1;
exploded_time.tm_wday = 0;
exploded_time.tm_yday = 0;
exploded_time.tm_isdst = 0;
exploded_time.tm_gmtoff = 0;
apr_err = apr_time_exp_gmt_get(when, &exploded_time);
if (apr_err == APR_SUCCESS)
return SVN_NO_ERROR;
return svn_error_create(SVN_ERR_BAD_DATE, NULL, NULL);
fail:
if (sscanf(data,
old_timestamp_format,
wday,
&exploded_time.tm_mday,
month,
&exploded_time.tm_year,
&exploded_time.tm_hour,
&exploded_time.tm_min,
&exploded_time.tm_sec,
&exploded_time.tm_usec,
&exploded_time.tm_yday,
&exploded_time.tm_isdst,
&exploded_time.tm_gmtoff) == 11) {
exploded_time.tm_year -= 1900;
exploded_time.tm_yday -= 1;
exploded_time.tm_wday = find_matching_string(wday, 7, apr_day_snames);
exploded_time.tm_mon = find_matching_string(month, 12, apr_month_snames);
apr_err = apr_time_exp_gmt_get(when, &exploded_time);
if (apr_err != APR_SUCCESS)
return svn_error_create(SVN_ERR_BAD_DATE, NULL, NULL);
return SVN_NO_ERROR;
}
else
return svn_error_create(SVN_ERR_BAD_DATE, NULL, NULL);
}
const char *
svn_time_to_human_cstring(apr_time_t when, apr_pool_t *pool) {
apr_time_exp_t exploded_time;
apr_size_t len, retlen;
apr_status_t ret;
char *datestr, *curptr, human_datestr[SVN_TIME__MAX_LENGTH];
apr_time_exp_lt(&exploded_time, when);
datestr = apr_palloc(pool, SVN_TIME__MAX_LENGTH);
len = apr_snprintf(datestr,
SVN_TIME__MAX_LENGTH,
human_timestamp_format,
exploded_time.tm_year + 1900,
exploded_time.tm_mon + 1,
exploded_time.tm_mday,
exploded_time.tm_hour,
exploded_time.tm_min,
exploded_time.tm_sec,
exploded_time.tm_gmtoff / (60 * 60),
(abs(exploded_time.tm_gmtoff) / 60) % 60);
if (len >= SVN_TIME__MAX_LENGTH)
return datestr;
curptr = datestr + len;
ret = apr_strftime(human_datestr,
&retlen,
SVN_TIME__MAX_LENGTH - len,
human_timestamp_format_suffix,
&exploded_time);
if (ret || retlen == 0)
*curptr = '\0';
else {
const char *utf8_string;
svn_error_t *err;
err = svn_utf_cstring_to_utf8(&utf8_string, human_datestr, pool);
if (err) {
*curptr = '\0';
svn_error_clear(err);
} else
apr_cpystrn(curptr, utf8_string, SVN_TIME__MAX_LENGTH - len);
}
return datestr;
}
void
svn_sleep_for_timestamps(void) {
apr_time_t now, then;
char *sleep_env_var;
sleep_env_var = getenv(SVN_SLEEP_ENV_VAR);
if (! sleep_env_var || apr_strnatcasecmp(sleep_env_var, "yes") != 0) {
now = apr_time_now();
then = apr_time_make(apr_time_sec(now) + 1, APR_USEC_PER_SEC / 10);
apr_sleep(then - now);
}
}
