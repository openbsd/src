/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/asn1/k5.asn1 */
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
encode_PA_SAM_CHALLENGE_2(unsigned char *p, size_t len, const PA_SAM_CHALLENGE_2 *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
int oldret = ret;
ret = 0;
for(i = (&(data)->sam_cksum)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
e = encode_Checksum(p, len, &(&(data)->sam_cksum)->val[i], &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_PA_SAM_CHALLENGE_2_BODY(p, len, &(data)->sam_body, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
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
decode_PA_SAM_CHALLENGE_2(const unsigned char *p, size_t len, PA_SAM_CHALLENGE_2 *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, ASN1_C_UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
{
int dce_fix;
if((dce_fix = fix_dce(reallen, &len)) < 0)
return ASN1_BAD_FORMAT;
{
size_t newlen, oldlen;

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 0, &l);
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
e = decode_PA_SAM_CHALLENGE_2_BODY(p, len, &(data)->sam_body, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 1, &l);
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
e = der_match_tag_and_length (p, len, ASN1_C_UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
if(len < reallen)
return ASN1_OVERRUN;
len = reallen;
{
size_t origlen = len;
int oldret = ret;
ret = 0;
(&(data)->sam_cksum)->len = 0;
(&(data)->sam_cksum)->val = NULL;
while(ret < origlen) {
(&(data)->sam_cksum)->len++;
(&(data)->sam_cksum)->val = realloc((&(data)->sam_cksum)->val, sizeof(*((&(data)->sam_cksum)->val)) * (&(data)->sam_cksum)->len);
e = decode_Checksum(p, len, &(&(data)->sam_cksum)->val[(&(data)->sam_cksum)->len-1], &l);
FORW;
len = origlen - ret;
}
ret += oldret;
}
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
if(size) *size = ret;
return 0;
fail:
free_PA_SAM_CHALLENGE_2(data);
return e;
}

void
free_PA_SAM_CHALLENGE_2(PA_SAM_CHALLENGE_2 *data)
{
free_PA_SAM_CHALLENGE_2_BODY(&(data)->sam_body);
while((&(data)->sam_cksum)->len){
free_Checksum(&(&(data)->sam_cksum)->val[(&(data)->sam_cksum)->len-1]);
(&(data)->sam_cksum)->len--;
}
free((&(data)->sam_cksum)->val);
(&(data)->sam_cksum)->val = NULL;
}

size_t
length_PA_SAM_CHALLENGE_2(const PA_SAM_CHALLENGE_2 *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_PA_SAM_CHALLENGE_2_BODY(&(data)->sam_body);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = (&(data)->sam_cksum)->len - 1; i >= 0; --i){
int oldret = ret;
ret = 0;
ret += length_Checksum(&(&(data)->sam_cksum)->val[i]);
ret += oldret;
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_PA_SAM_CHALLENGE_2(const PA_SAM_CHALLENGE_2 *from, PA_SAM_CHALLENGE_2 *to)
{
if(copy_PA_SAM_CHALLENGE_2_BODY(&(from)->sam_body, &(to)->sam_body)) return ENOMEM;
if(((&(to)->sam_cksum)->val = malloc((&(from)->sam_cksum)->len * sizeof(*(&(to)->sam_cksum)->val))) == NULL && (&(from)->sam_cksum)->len != 0)
return ENOMEM;
for((&(to)->sam_cksum)->len = 0; (&(to)->sam_cksum)->len < (&(from)->sam_cksum)->len; (&(to)->sam_cksum)->len++){
if(copy_Checksum(&(&(from)->sam_cksum)->val[(&(to)->sam_cksum)->len], &(&(to)->sam_cksum)->val[(&(to)->sam_cksum)->len])) return ENOMEM;
}
return 0;
}

