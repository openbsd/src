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
encode_PA_ENC_SAM_RESPONSE_ENC(unsigned char *p, size_t len, const PA_ENC_SAM_RESPONSE_ENC *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->sam_sad)
{
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, (data)->sam_sad, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->sam_nonce, &l);
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
decode_PA_ENC_SAM_RESPONSE_ENC(const unsigned char *p, size_t len, PA_ENC_SAM_RESPONSE_ENC *data, size_t *size)
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
e = decode_integer(p, len, &(data)->sam_nonce, &l);
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
(data)->sam_sad = NULL;
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
(data)->sam_sad = malloc(sizeof(*(data)->sam_sad));
if((data)->sam_sad == NULL) return ENOMEM;
e = decode_general_string(p, len, (data)->sam_sad, &l);
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
if(size) *size = ret;
return 0;
fail:
free_PA_ENC_SAM_RESPONSE_ENC(data);
return e;
}

void
free_PA_ENC_SAM_RESPONSE_ENC(PA_ENC_SAM_RESPONSE_ENC *data)
{
if((data)->sam_sad) {
free_general_string((data)->sam_sad);
free((data)->sam_sad);
(data)->sam_sad = NULL;
}
}

size_t
length_PA_ENC_SAM_RESPONSE_ENC(const PA_ENC_SAM_RESPONSE_ENC *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->sam_nonce);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sam_sad){
int oldret = ret;
ret = 0;
ret += length_general_string((data)->sam_sad);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_PA_ENC_SAM_RESPONSE_ENC(const PA_ENC_SAM_RESPONSE_ENC *from, PA_ENC_SAM_RESPONSE_ENC *to)
{
*(&(to)->sam_nonce) = *(&(from)->sam_nonce);
if((from)->sam_sad) {
(to)->sam_sad = malloc(sizeof(*(to)->sam_sad));
if((to)->sam_sad == NULL) return ENOMEM;
if(copy_general_string((from)->sam_sad, (to)->sam_sad)) return ENOMEM;
}else
(to)->sam_sad = NULL;
return 0;
}

