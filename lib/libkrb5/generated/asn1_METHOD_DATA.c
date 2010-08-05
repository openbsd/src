/* Generated from /home/src/src/lib/libkrb5/../../kerberosV/src/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <krb5_asn1.h>
#include <asn1_err.h>
#include <der.h>
#include <parse_units.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_METHOD_DATA(unsigned char *p, size_t len, const METHOD_DATA *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
for(i = (data)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
e = encode_PA_DATA(p, len, &(data)->val[i], &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_METHOD_DATA(const unsigned char *p, size_t len, METHOD_DATA *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, ASN1_C_UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
if(len < reallen)
return ASN1_OVERRUN;
len = reallen;
{
size_t origlen = len;
int oldret = ret;
ret = 0;
(data)->len = 0;
(data)->val = NULL;
while(ret < origlen) {
(data)->len++;
(data)->val = realloc((data)->val, sizeof(*((data)->val)) * (data)->len);
e = decode_PA_DATA(p, len, &(data)->val[(data)->len-1], &l);
FORW;
len = origlen - ret;
}
ret += oldret;
}
if(size) *size = ret;
return 0;
fail:
free_METHOD_DATA(data);
return e;
}

void
free_METHOD_DATA(METHOD_DATA *data)
{
while((data)->len){
free_PA_DATA(&(data)->val[(data)->len-1]);
(data)->len--;
}
free((data)->val);
(data)->val = NULL;
}

size_t
length_METHOD_DATA(const METHOD_DATA *data)
{
size_t ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = (data)->len - 1; i >= 0; --i){
int oldret = ret;
ret = 0;
ret += length_PA_DATA(&(data)->val[i]);
ret += oldret;
}
ret += 1 + length_len(ret) + oldret;
}
return ret;
}

int
copy_METHOD_DATA(const METHOD_DATA *from, METHOD_DATA *to)
{
if(((to)->val = calloc((from)->len, sizeof(*(to)->val))) == NULL && (from)->len != 0)
return ENOMEM;
for((to)->len = 0; (to)->len < (from)->len; (to)->len++){
if(copy_PA_DATA(&(from)->val[(to)->len], &(to)->val[(to)->len])) return ENOMEM;
}
return 0;
}

