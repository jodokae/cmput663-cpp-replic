#include <apr.h>
#include <apr_pools.h>
#include <apr_general.h>
#include "diff.h"
typedef struct svn_diff__snake_t svn_diff__snake_t;
struct svn_diff__snake_t {
apr_off_t y;
svn_diff__lcs_t *lcs;
svn_diff__position_t *position[2];
};
static APR_INLINE void
svn_diff__snake(apr_off_t k,
svn_diff__snake_t *fp,
int idx,
svn_diff__lcs_t **freelist,
apr_pool_t *pool) {
svn_diff__position_t *start_position[2];
svn_diff__position_t *position[2];
svn_diff__lcs_t *lcs;
svn_diff__lcs_t *previous_lcs;
lcs = fp[k].lcs;
while (lcs) {
lcs->refcount--;
if (lcs->refcount)
break;
previous_lcs = lcs->next;
lcs->next = *freelist;
*freelist = lcs;
lcs = previous_lcs;
}
if (fp[k - 1].y + 1 > fp[k + 1].y) {
start_position[0] = fp[k - 1].position[0];
start_position[1] = fp[k - 1].position[1]->next;
previous_lcs = fp[k - 1].lcs;
} else {
start_position[0] = fp[k + 1].position[0]->next;
start_position[1] = fp[k + 1].position[1];
previous_lcs = fp[k + 1].lcs;
}
position[0] = start_position[0];
position[1] = start_position[1];
while (position[0]->node == position[1]->node) {
position[0] = position[0]->next;
position[1] = position[1]->next;
}
if (position[1] != start_position[1]) {
lcs = *freelist;
if (lcs) {
*freelist = lcs->next;
} else {
lcs = apr_palloc(pool, sizeof(*lcs));
}
lcs->position[idx] = start_position[0];
lcs->position[abs(1 - idx)] = start_position[1];
lcs->length = position[1]->offset - start_position[1]->offset;
lcs->next = previous_lcs;
lcs->refcount = 1;
fp[k].lcs = lcs;
} else {
fp[k].lcs = previous_lcs;
}
if (previous_lcs) {
previous_lcs->refcount++;
}
fp[k].position[0] = position[0];
fp[k].position[1] = position[1];
fp[k].y = position[1]->offset;
}
static svn_diff__lcs_t *
svn_diff__lcs_reverse(svn_diff__lcs_t *lcs) {
svn_diff__lcs_t *next;
svn_diff__lcs_t *prev;
next = NULL;
while (lcs != NULL) {
prev = lcs->next;
lcs->next = next;
next = lcs;
lcs = prev;
}
return next;
}
svn_diff__lcs_t *
svn_diff__lcs(svn_diff__position_t *position_list1,
svn_diff__position_t *position_list2,
apr_pool_t *pool) {
int idx;
apr_off_t length[2];
svn_diff__snake_t *fp;
apr_off_t d;
apr_off_t k;
apr_off_t p = 0;
svn_diff__lcs_t *lcs, *lcs_freelist = NULL;
svn_diff__position_t sentinel_position[2];
lcs = apr_palloc(pool, sizeof(*lcs));
lcs->position[0] = apr_pcalloc(pool, sizeof(*lcs->position[0]));
lcs->position[0]->offset = position_list1 ? position_list1->offset + 1 : 1;
lcs->position[1] = apr_pcalloc(pool, sizeof(*lcs->position[1]));
lcs->position[1]->offset = position_list2 ? position_list2->offset + 1 : 1;
lcs->length = 0;
lcs->refcount = 1;
lcs->next = NULL;
if (position_list1 == NULL || position_list2 == NULL)
return lcs;
length[0] = position_list1->offset - position_list1->next->offset + 1;
length[1] = position_list2->offset - position_list2->next->offset + 1;
idx = length[0] > length[1] ? 1 : 0;
fp = apr_pcalloc(pool,
sizeof(*fp) * (apr_size_t)(length[0] + length[1] + 3));
fp += length[idx] + 1;
sentinel_position[idx].next = position_list1->next;
position_list1->next = &sentinel_position[idx];
sentinel_position[idx].offset = position_list1->offset + 1;
sentinel_position[abs(1 - idx)].next = position_list2->next;
position_list2->next = &sentinel_position[abs(1 - idx)];
sentinel_position[abs(1 - idx)].offset = position_list2->offset + 1;
sentinel_position[0].node = (void*)&sentinel_position[0];
sentinel_position[1].node = (void*)&sentinel_position[1];
d = length[abs(1 - idx)] - length[idx];
fp[-1].position[0] = sentinel_position[0].next;
fp[-1].position[1] = &sentinel_position[1];
p = 0;
do {
for (k = -p; k < d; k++) {
svn_diff__snake(k, fp, idx, &lcs_freelist, pool);
}
for (k = d + p; k >= d; k--) {
svn_diff__snake(k, fp, idx, &lcs_freelist, pool);
}
p++;
} while (fp[d].position[1] != &sentinel_position[1]);
lcs->next = fp[d].lcs;
lcs = svn_diff__lcs_reverse(lcs);
position_list1->next = sentinel_position[idx].next;
position_list2->next = sentinel_position[abs(1 - idx)].next;
return lcs;
}
