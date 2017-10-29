#include <stdio.h>
#include "svn_pools.h"
#include "svn_io.h"
#include <apr_general.h>
#include "../svn_test.h"
static svn_error_t *
test_stream_from_string(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
int i;
apr_pool_t *subpool = svn_pool_create(pool);
#define NUM_TEST_STRINGS 4
#define TEST_BUF_SIZE 10
static const char * const strings[NUM_TEST_STRINGS] = {
"",
"This is a string.",
"This is, by comparison to the previous string, a much longer string.",
"And if you thought that last string was long, you just wait until "
"I'm finished here. I mean, how can a string really claim to be long "
"when it fits on a single line of 80-columns? Give me a break. "
"Now, I'm not saying that I'm the longest string out there--far from "
"it--but I feel that it is safe to assume that I'm far longer than my "
"peers. And that demands some amount of respect, wouldn't you say?"
};
*msg = "test svn_stream_from_string";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < NUM_TEST_STRINGS; i++) {
svn_stream_t *stream;
char buffer[TEST_BUF_SIZE];
svn_stringbuf_t *inbuf, *outbuf;
apr_size_t len;
inbuf = svn_stringbuf_create(strings[i], subpool);
outbuf = svn_stringbuf_create("", subpool);
stream = svn_stream_from_stringbuf(inbuf, subpool);
len = TEST_BUF_SIZE;
while (len == TEST_BUF_SIZE) {
SVN_ERR(svn_stream_read(stream, buffer, &len));
svn_stringbuf_appendbytes(outbuf, buffer, len);
}
if (! svn_stringbuf_compare(inbuf, outbuf))
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Got unexpected result.");
svn_pool_clear(subpool);
}
for (i = 0; i < NUM_TEST_STRINGS; i++) {
svn_stream_t *stream;
svn_stringbuf_t *inbuf, *outbuf;
apr_size_t amt_read, len;
inbuf = svn_stringbuf_create(strings[i], subpool);
outbuf = svn_stringbuf_create("", subpool);
stream = svn_stream_from_stringbuf(outbuf, subpool);
amt_read = 0;
while (amt_read < inbuf->len) {
len = TEST_BUF_SIZE < (inbuf->len - amt_read)
? TEST_BUF_SIZE
: inbuf->len - amt_read;
SVN_ERR(svn_stream_write(stream, inbuf->data + amt_read, &len));
amt_read += len;
}
if (! svn_stringbuf_compare(inbuf, outbuf))
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Got unexpected result.");
svn_pool_clear(subpool);
}
#undef NUM_TEST_STRINGS
#undef TEST_BUF_SIZE
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
static svn_stringbuf_t *
generate_test_bytes(int num_bytes, apr_pool_t *pool) {
svn_stringbuf_t *buffer = svn_stringbuf_create("", pool);
int total, repeat, repeat_iter;
char c;
for (total = 0, repeat = repeat_iter = 1, c = 0; total < num_bytes; total++) {
svn_stringbuf_appendbytes(buffer, &c, 1);
repeat_iter--;
if (repeat_iter == 0) {
if (c == 127)
repeat++;
c = (c + 1) % 127;
repeat_iter = repeat;
}
}
return buffer;
}
static svn_error_t *
test_stream_compressed(const char **msg,
svn_boolean_t msg_only,
svn_test_opts_t *opts,
apr_pool_t *pool) {
#define NUM_TEST_STRINGS 5
#define TEST_BUF_SIZE 10
#define GENERATED_SIZE 20000
int i;
svn_stringbuf_t *bufs[NUM_TEST_STRINGS];
apr_pool_t *subpool = svn_pool_create(pool);
static const char * const strings[NUM_TEST_STRINGS - 1] = {
"",
"This is a string.",
"This is, by comparison to the previous string, a much longer string.",
"And if you thought that last string was long, you just wait until "
"I'm finished here. I mean, how can a string really claim to be long "
"when it fits on a single line of 80-columns? Give me a break. "
"Now, I'm not saying that I'm the longest string out there--far from "
"it--but I feel that it is safe to assume that I'm far longer than my "
"peers. And that demands some amount of respect, wouldn't you say?"
};
*msg = "test compressed streams";
if (msg_only)
return SVN_NO_ERROR;
for (i = 0; i < (NUM_TEST_STRINGS - 1); i++)
bufs[i] = svn_stringbuf_create(strings[i], pool);
bufs[NUM_TEST_STRINGS - 1] = generate_test_bytes(GENERATED_SIZE, pool);
for (i = 0; i < NUM_TEST_STRINGS; i++) {
svn_stream_t *stream;
svn_stringbuf_t *origbuf, *inbuf, *outbuf;
char buf[TEST_BUF_SIZE];
apr_size_t len;
origbuf = bufs[i];
inbuf = svn_stringbuf_create("", subpool);
outbuf = svn_stringbuf_create("", subpool);
stream = svn_stream_compressed(svn_stream_from_stringbuf(outbuf,
subpool),
subpool);
len = origbuf->len;
SVN_ERR(svn_stream_write(stream, origbuf->data, &len));
SVN_ERR(svn_stream_close(stream));
stream = svn_stream_compressed(svn_stream_from_stringbuf(outbuf,
subpool),
subpool);
len = TEST_BUF_SIZE;
while (len >= TEST_BUF_SIZE) {
len = TEST_BUF_SIZE;
SVN_ERR(svn_stream_read(stream, buf, &len));
if (len > 0)
svn_stringbuf_appendbytes(inbuf, buf, len);
}
if (! svn_stringbuf_compare(inbuf, origbuf))
return svn_error_create(SVN_ERR_TEST_FAILED, NULL,
"Got unexpected result.");
SVN_ERR(svn_stream_close(stream));
svn_pool_clear(subpool);
}
#undef NUM_TEST_STRINGS
#undef TEST_BUF_SIZE
#undef GENEREATED_SIZE
svn_pool_destroy(subpool);
return SVN_NO_ERROR;
}
struct svn_test_descriptor_t test_funcs[] = {
SVN_TEST_NULL,
SVN_TEST_PASS(test_stream_from_string),
SVN_TEST_PASS(test_stream_compressed),
SVN_TEST_NULL
};