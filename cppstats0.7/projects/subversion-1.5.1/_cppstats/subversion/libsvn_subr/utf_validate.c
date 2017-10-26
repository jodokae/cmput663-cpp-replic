#include "utf_impl.h"
static const char octet_category[256] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
4, 4,
5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
6,
7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
8,
9, 9,
10,
11, 11, 11,
12,
13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13
};
#define FSM_START 0
#define FSM_80BF 1
#define FSM_A0BF 2
#define FSM_80BF80BF 3
#define FSM_809F 4
#define FSM_90BF 5
#define FSM_80BF80BF80BF 6
#define FSM_808F 7
#define FSM_ERROR 8
static const char machine [9][14] = {
{
FSM_START,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_80BF,
FSM_A0BF,
FSM_80BF80BF,
FSM_809F,
FSM_80BF80BF,
FSM_90BF,
FSM_80BF80BF80BF,
FSM_808F,
FSM_ERROR
},
{
FSM_ERROR,
FSM_START,
FSM_START,
FSM_START,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
{
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_80BF,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
{
FSM_ERROR,
FSM_80BF,
FSM_80BF,
FSM_80BF,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
{
FSM_ERROR,
FSM_80BF,
FSM_80BF,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
{
FSM_ERROR,
FSM_ERROR,
FSM_80BF80BF,
FSM_80BF80BF,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
{
FSM_ERROR,
FSM_80BF80BF,
FSM_80BF80BF,
FSM_80BF80BF,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
{
FSM_ERROR,
FSM_80BF80BF,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
{
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR,
FSM_ERROR
},
};
const char *
svn_utf__last_valid(const char *data, apr_size_t len) {
const char *start = data, *end = data + len;
int state = FSM_START;
while (data < end) {
unsigned char octet = *data++;
int category = octet_category[octet];
state = machine[state][category];
if (state == FSM_START)
start = data;
}
return start;
}
svn_boolean_t
svn_utf__cstring_is_valid(const char *data) {
int state = FSM_START;
while (*data) {
unsigned char octet = *data++;
int category = octet_category[octet];
state = machine[state][category];
}
return state == FSM_START ? TRUE : FALSE;
}
svn_boolean_t
svn_utf__is_valid(const char *data, apr_size_t len) {
const char *end = data + len;
int state = FSM_START;
while (data < end) {
unsigned char octet = *data++;
int category = octet_category[octet];
state = machine[state][category];
}
return state == FSM_START ? TRUE : FALSE;
}
const char *
svn_utf__last_valid2(const char *data, apr_size_t len) {
const char *start = data, *end = data + len;
int state = FSM_START;
while (data < end) {
unsigned char octet = *data++;
switch (state) {
case FSM_START:
if (octet <= 0x7F)
break;
else if (octet <= 0xC1)
state = FSM_ERROR;
else if (octet <= 0xDF)
state = FSM_80BF;
else if (octet == 0xE0)
state = FSM_A0BF;
else if (octet <= 0xEC)
state = FSM_80BF80BF;
else if (octet == 0xED)
state = FSM_809F;
else if (octet <= 0xEF)
state = FSM_80BF80BF;
else if (octet == 0xF0)
state = FSM_90BF;
else if (octet <= 0xF3)
state = FSM_80BF80BF80BF;
else if (octet <= 0xF4)
state = FSM_808F;
else
state = FSM_ERROR;
break;
case FSM_80BF:
if (octet >= 0x80 && octet <= 0xBF)
state = FSM_START;
else
state = FSM_ERROR;
break;
case FSM_A0BF:
if (octet >= 0xA0 && octet <= 0xBF)
state = FSM_80BF;
else
state = FSM_ERROR;
break;
case FSM_80BF80BF:
if (octet >= 0x80 && octet <= 0xBF)
state = FSM_80BF;
else
state = FSM_ERROR;
break;
case FSM_809F:
if (octet >= 0x80 && octet <= 0x9F)
state = FSM_80BF;
else
state = FSM_ERROR;
break;
case FSM_90BF:
if (octet >= 0x90 && octet <= 0xBF)
state = FSM_80BF80BF;
else
state = FSM_ERROR;
break;
case FSM_80BF80BF80BF:
if (octet >= 0x80 && octet <= 0xBF)
state = FSM_80BF80BF;
else
state = FSM_ERROR;
break;
case FSM_808F:
if (octet >= 0x80 && octet <= 0x8F)
state = FSM_80BF80BF;
else
state = FSM_ERROR;
break;
default:
case FSM_ERROR:
return start;
}
if (state == FSM_START)
start = data;
}
return start;
}
