/* Generated from /usr/src/kerberosV/lib/asn1/../../src/lib/asn1/k5.asn1 */
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
encode_AuthorizationData(unsigned char *p, size_t len, const AuthorizationData *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
for(i = (data)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, &(&(data)->val[i])->ad_data, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(&(data)->val[i])->ad_type, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 0, &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_AuthorizationData(const unsigned char *p, size_t len, AuthorizationData *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, UNIV, CONS, UT_Sequence,&reallen, &l);
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
e = der_match_tag_and_length (p, len, UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
{
int dce_fix;
if((dce_fix = fix_dce(reallen, &len)) < 0)
return ASN1_BAD_FORMAT;
{
size_t newlen, oldlen;

e = der_match_tag (p, len, CONTEXT, CONS, 0, &l);
if (e)
return e;
else {
p += l;
len -= l;
ret += l;
e = der_get_length (p, len, &newlen, &l);
FORW;
{
int dce_fix;
oldlen = len;
if((dce_fix = fix_dce(newlen, &len)) < 0)return ASN1_BAD_FORMAT;
e = decode_integer(p, len, &(&(data)->val[(data)->len-1])->ad_type, &l);
FORW;
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}else 
len = oldlen - newlen;
}
}
}
{
size_t newlen, oldlen;

e = der_match_tag (p, len, CONTEXT, CONS, 1, &l);
if (e)
return e;
else {
p += l;
len -= l;
ret += l;
e = der_get_length (p, len, &newlen, &l);
FORW;
{
int dce_fix;
oldlen = len;
if((dce_fix = fix_dce(newlen, &len)) < 0)return ASN1_BAD_FORMAT;
e = decode_octet_string(p, len, &(&(data)->val[(data)->len-1])->ad_data, &l);
FORW;
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}else 
len = oldlen - newlen;
}
}
}
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}
}
len = origlen - ret;
}
ret += oldret;
}
if(size) *size = ret;
return 0;
fail:
free_AuthorizationData(data);
return e;
}

void
free_AuthorizationData(AuthorizationData *data)
{
while((data)->len){
free_octet_string(&(&(data)->val[(data)->len-1])->ad_data);
(data)->len--;
}
free((data)->val);
}

size_t
length_AuthorizationData(const AuthorizationData *data)
{
size_t ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = (data)->len - 1; i >= 0; --i){
{
int oldret = ret;
ret = 0;
ret += length_integer(&(&(data)->val[i])->ad_type);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_octet_string(&(&(data)->val[i])->ad_data);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
}
ret += 1 + length_len(ret) + oldret;
}
return ret;
}

int
copy_AuthorizationData(const AuthorizationData *from, AuthorizationData *to)
{
if(((to)->val = malloc((from)->len * sizeof(*(to)->val))) == NULL && (from)->len != 0)
return ENOMEM;
for((to)->len = 0; (to)->len < (from)->len; (to)->len++){
*(&(&(to)->val[(to)->len])->ad_type) = *(&(&(from)->val[(to)->len])->ad_type);
if(copy_octet_string(&(&(from)->val[(to)->len])->ad_data, &(&(to)->val[(to)->len])->ad_data)) return ENOMEM;
}
return 0;
}

