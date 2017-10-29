#include <assert.h>
#include <apr_general.h>
#include <apr_hash.h>
#include "svn_delta.h"
#include "delta.h"
#define ADLER32_MASK 0x0000ffff
#define ADLER32_CHAR_MASK 0x000000ff
struct adler32 {
apr_uint32_t s1;
apr_uint32_t s2;
apr_uint32_t len;
};
static APR_INLINE void
adler32_in(struct adler32 *ad, const char c) {
ad->s1 += ((apr_uint32_t) (c)) & ADLER32_CHAR_MASK;
ad->s1 &= ADLER32_MASK;
ad->s2 += ad->s1;
ad->s2 &= ADLER32_MASK;
ad->len++;
}
static APR_INLINE void
adler32_out(struct adler32 *ad, const char c) {
ad->s1 -= ((apr_uint32_t) (c)) & ADLER32_CHAR_MASK;
ad->s1 &= ADLER32_MASK;
ad->s2 -= (ad->len * (((apr_uint32_t) c) & ADLER32_CHAR_MASK)) + 1;
ad->s2 &= ADLER32_MASK;
--ad->len;
}
static APR_INLINE apr_uint32_t
adler32_sum(const struct adler32 *ad) {
return (ad->s2 << 16) | (ad->s1);
}
static APR_INLINE struct adler32 *
init_adler32(struct adler32 *ad, const char *data, apr_uint32_t datalen) {
ad->s1 = 1;
ad->s2 = 0;
ad->len = 0;
while (datalen--)
adler32_in(ad, *(data++));
return ad;
}
#define MATCH_BLOCKSIZE 64
struct block {
apr_uint32_t adlersum;
apr_size_t pos;
};
struct blocks {
apr_size_t max;
struct block *slots;
};
static apr_size_t hash_func(apr_uint32_t sum) {
return sum ^ (sum >> 12);
}
static void
add_block(struct blocks *blocks, apr_uint32_t adlersum, apr_size_t pos) {
apr_size_t h = hash_func(adlersum) & blocks->max;
while (blocks->slots[h].pos != (apr_size_t)-1) {
if (blocks->slots[h].adlersum == adlersum)
return;
h = (h + 1) & blocks->max;
}
blocks->slots[h].adlersum = adlersum;
blocks->slots[h].pos = pos;
}
static apr_size_t
find_block(const struct blocks *blocks, apr_uint32_t adlersum) {
apr_size_t h = hash_func(adlersum) & blocks->max;
while (blocks->slots[h].adlersum != adlersum
&& blocks->slots[h].pos != (apr_size_t)-1)
h = (h + 1) & blocks->max;
return blocks->slots[h].pos;
}
static void
init_blocks_table(const char *data,
apr_size_t datalen,
struct blocks *blocks,
apr_pool_t *pool) {
apr_size_t i;
struct adler32 adler;
apr_size_t nblocks;
apr_size_t nslots = 1;
nblocks = datalen / MATCH_BLOCKSIZE + 1;
while (nslots <= nblocks)
nslots *= 2;
nslots *= 2;
blocks->max = nslots - 1;
blocks->slots = apr_palloc(pool, nslots * sizeof(*(blocks->slots)));
for (i = 0; i < nslots; ++i) {
blocks->slots[i].adlersum = 0;
blocks->slots[i].pos = (apr_size_t)-1;
}
for (i = 0; i < datalen; i += MATCH_BLOCKSIZE) {
apr_uint32_t step =
((i + MATCH_BLOCKSIZE) >= datalen) ? (datalen - i) : MATCH_BLOCKSIZE;
apr_uint32_t adlersum =
adler32_sum(init_adler32(&adler, data + i, step));
add_block(blocks, adlersum, i);
}
}
static svn_boolean_t
find_match(const struct blocks *blocks,
const struct adler32 *rolling,
const char *a,
apr_size_t asize,
const char *b,
apr_size_t bsize,
apr_size_t bpos,
apr_size_t *aposp,
apr_size_t *alenp,
apr_size_t *badvancep,
apr_size_t *pending_insert_lenp) {
apr_uint32_t sum = adler32_sum(rolling);
apr_size_t alen, badvance, apos;
apr_size_t tpos, tlen;
tpos = find_block(blocks, sum);
if (tpos == (apr_size_t)-1)
return FALSE;
tlen = ((tpos + MATCH_BLOCKSIZE) >= asize)
? (asize - tpos) : MATCH_BLOCKSIZE;
if (memcmp(a + tpos, b + bpos, tlen) != 0)
return FALSE;
apos = tpos;
alen = tlen;
badvance = tlen;
while ((apos + alen < asize)
&& (bpos + badvance < bsize)
&& (a[apos + alen] == b[bpos + badvance])) {
++alen;
++badvance;
}
while (apos > 0
&& bpos > 0
&& a[apos - 1] == b[bpos - 1]
&& *pending_insert_lenp > 0) {
--(*pending_insert_lenp);
--apos;
--bpos;
++alen;
}
*aposp = apos;
*alenp = alen;
*badvancep = badvance;
return TRUE;
}
static void
compute_delta(svn_txdelta__ops_baton_t *build_baton,
const char *a,
apr_uint32_t asize,
const char *b,
apr_uint32_t bsize,
apr_pool_t *pool) {
struct blocks blocks;
struct adler32 rolling;
apr_size_t sz, lo, hi, pending_insert_start = 0, pending_insert_len = 0;
if (bsize < MATCH_BLOCKSIZE) {
svn_txdelta__insert_op(build_baton, svn_txdelta_new,
0, bsize, b, pool);
return;
}
init_blocks_table(a, asize, &blocks, pool);
init_adler32(&rolling, b, MATCH_BLOCKSIZE);
for (sz = bsize, lo = 0, hi = MATCH_BLOCKSIZE; lo < sz;) {
apr_size_t apos = 0;
apr_size_t alen = 1;
apr_size_t badvance = 1;
apr_size_t next;
svn_boolean_t match;
match = find_match(&blocks, &rolling, a, asize, b, bsize, lo, &apos,
&alen, &badvance, &pending_insert_len);
if (! match)
++pending_insert_len;
else {
if (pending_insert_len > 0) {
svn_txdelta__insert_op(build_baton, svn_txdelta_new,
0, pending_insert_len,
b + pending_insert_start, pool);
pending_insert_len = 0;
}
pending_insert_start = lo + badvance;
svn_txdelta__insert_op(build_baton, svn_txdelta_source,
apos, alen, NULL, pool);
}
next = lo;
for (; next < lo + badvance; ++next) {
adler32_out(&rolling, b[next]);
if (next + MATCH_BLOCKSIZE < bsize)
adler32_in(&rolling, b[next + MATCH_BLOCKSIZE]);
}
lo = next;
hi = lo + MATCH_BLOCKSIZE;
}
if (pending_insert_len > 0) {
svn_txdelta__insert_op(build_baton, svn_txdelta_new,
0, pending_insert_len,
b + pending_insert_start, pool);
}
}
void
svn_txdelta__xdelta(svn_txdelta__ops_baton_t *build_baton,
const char *data,
apr_size_t source_len,
apr_size_t target_len,
apr_pool_t *pool) {
assert(source_len != 0);
compute_delta(build_baton, data, source_len,
data + source_len, target_len,
pool);
}