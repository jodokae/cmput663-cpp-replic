#if !defined(STRINGLIB_FASTSEARCH_H)
#define STRINGLIB_FASTSEARCH_H
#define FAST_COUNT 0
#define FAST_SEARCH 1
Py_LOCAL_INLINE(Py_ssize_t)
fastsearch(const STRINGLIB_CHAR* s, Py_ssize_t n,
const STRINGLIB_CHAR* p, Py_ssize_t m,
int mode) {
long mask;
Py_ssize_t skip, count = 0;
Py_ssize_t i, j, mlast, w;
w = n - m;
if (w < 0)
return -1;
if (m <= 1) {
if (m <= 0)
return -1;
if (mode == FAST_COUNT) {
for (i = 0; i < n; i++)
if (s[i] == p[0])
count++;
return count;
} else {
for (i = 0; i < n; i++)
if (s[i] == p[0])
return i;
}
return -1;
}
mlast = m - 1;
skip = mlast - 1;
for (mask = i = 0; i < mlast; i++) {
mask |= (1 << (p[i] & 0x1F));
if (p[i] == p[mlast])
skip = mlast - i - 1;
}
mask |= (1 << (p[mlast] & 0x1F));
for (i = 0; i <= w; i++) {
if (s[i+m-1] == p[m-1]) {
for (j = 0; j < mlast; j++)
if (s[i+j] != p[j])
break;
if (j == mlast) {
if (mode != FAST_COUNT)
return i;
count++;
i = i + mlast;
continue;
}
if (!(mask & (1 << (s[i+m] & 0x1F))))
i = i + m;
else
i = i + skip;
} else {
if (!(mask & (1 << (s[i+m] & 0x1F))))
i = i + m;
}
}
if (mode != FAST_COUNT)
return -1;
return count;
}
#endif
