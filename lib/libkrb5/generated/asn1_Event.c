/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/hdb/hdb.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <krb5_asn1.h>
#include <hdb_asn1.h>
#include <asn1_err.h>
#include <der.h>
#include <parse_units.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_Event(unsigned char *p, size_t len, const Event *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->principal)
{
int oldret = ret;
ret = 0;
e = encode_Principal(p, len, (data)->principal, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, &(data)->time, &l);
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
decode_Event(const unsigned char *p, size_t len, Event *data, size_t *size)
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
e = decode_KerberosTime(p, len, &(data)->time, &l);
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
(data)->principal = NULL;
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
(data)->principal = malloc(sizeof(*(data)->principal));
if((data)->principal == NULL) return ENOMEM;
e = decode_Principal(p, len, (data)->principal, &l);
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
free_Event(data);
return e;
}

void
free_Event(Event *data)
{
free_KerberosTime(&(data)->time);
if((data)->principal) {
free_Principal((data)->principal);
free((data)->principal);
(data)->principal = NULL;
}
}

size_t
length_Event(const Event *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_KerberosTime(&(data)->time);
ret += 1 + length_len(ret) + oldret;
}
if((data)->principal){
int oldret = ret;
ret = 0;
ret += length_Principal((data)->principal);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_Event(const Event *from, Event *to)
{
if(copy_KerberosTime(&(from)->time, &(to)->time)) return ENOMEM;
if((from)->principal) {
(to)->principal = malloc(sizeof(*(to)->principal));
if((to)->principal == NULL) return ENOMEM;
if(copy_Principal((from)->principal, (to)->principal)) return ENOMEM;
}else
(to)->principal = NULL;
return 0;
}

