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
encode_PA_ENC_TS_ENC(unsigned char *p, size_t len, const PA_ENC_TS_ENC *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->pausec)
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, (data)->pausec, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, &(data)->patimestamp, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 0, &l);
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
decode_PA_ENC_TS_ENC(const unsigned char *p, size_t len, PA_ENC_TS_ENC *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
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
e = decode_KerberosTime(p, len, &(data)->patimestamp, &l);
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
(data)->pausec = NULL;
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
(data)->pausec = malloc(sizeof(*(data)->pausec));
if((data)->pausec == NULL) return ENOMEM;
e = decode_integer(p, len, (data)->pausec, &l);
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
free_PA_ENC_TS_ENC(data);
return e;
}

void
free_PA_ENC_TS_ENC(PA_ENC_TS_ENC *data)
{
free_KerberosTime(&(data)->patimestamp);
if((data)->pausec) {
free((data)->pausec);
}
}

size_t
length_PA_ENC_TS_ENC(const PA_ENC_TS_ENC *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_KerberosTime(&(data)->patimestamp);
ret += 1 + length_len(ret) + oldret;
}
if((data)->pausec){
int oldret = ret;
ret = 0;
ret += length_integer((data)->pausec);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_PA_ENC_TS_ENC(const PA_ENC_TS_ENC *from, PA_ENC_TS_ENC *to)
{
if(copy_KerberosTime(&(from)->patimestamp, &(to)->patimestamp)) return ENOMEM;
if((from)->pausec) {
(to)->pausec = malloc(sizeof(*(to)->pausec));
if((to)->pausec == NULL) return ENOMEM;
*((to)->pausec) = *((from)->pausec);
}else
(to)->pausec = NULL;
return 0;
}

