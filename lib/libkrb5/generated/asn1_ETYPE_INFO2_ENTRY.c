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
encode_ETYPE_INFO2_ENTRY(unsigned char *p, size_t len, const ETYPE_INFO2_ENTRY *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->s2kparams)
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, (data)->s2kparams, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->salt)
{
int oldret = ret;
ret = 0;
e = encode_KerberosString(p, len, (data)->salt, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_ENCTYPE(p, len, &(data)->etype, &l);
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
decode_ETYPE_INFO2_ENTRY(const unsigned char *p, size_t len, ETYPE_INFO2_ENTRY *data, size_t *size)
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
e = decode_ENCTYPE(p, len, &(data)->etype, &l);
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
(data)->salt = NULL;
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
(data)->salt = malloc(sizeof(*(data)->salt));
if((data)->salt == NULL) return ENOMEM;
e = decode_KerberosString(p, len, (data)->salt, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 2, &l);
if (e)
(data)->s2kparams = NULL;
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
(data)->s2kparams = malloc(sizeof(*(data)->s2kparams));
if((data)->s2kparams == NULL) return ENOMEM;
e = decode_octet_string(p, len, (data)->s2kparams, &l);
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
free_ETYPE_INFO2_ENTRY(data);
return e;
}

void
free_ETYPE_INFO2_ENTRY(ETYPE_INFO2_ENTRY *data)
{
free_ENCTYPE(&(data)->etype);
if((data)->salt) {
free_KerberosString((data)->salt);
free((data)->salt);
(data)->salt = NULL;
}
if((data)->s2kparams) {
free_octet_string((data)->s2kparams);
free((data)->s2kparams);
(data)->s2kparams = NULL;
}
}

size_t
length_ETYPE_INFO2_ENTRY(const ETYPE_INFO2_ENTRY *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_ENCTYPE(&(data)->etype);
ret += 1 + length_len(ret) + oldret;
}
if((data)->salt){
int oldret = ret;
ret = 0;
ret += length_KerberosString((data)->salt);
ret += 1 + length_len(ret) + oldret;
}
if((data)->s2kparams){
int oldret = ret;
ret = 0;
ret += length_octet_string((data)->s2kparams);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_ETYPE_INFO2_ENTRY(const ETYPE_INFO2_ENTRY *from, ETYPE_INFO2_ENTRY *to)
{
if(copy_ENCTYPE(&(from)->etype, &(to)->etype)) return ENOMEM;
if((from)->salt) {
(to)->salt = malloc(sizeof(*(to)->salt));
if((to)->salt == NULL) return ENOMEM;
if(copy_KerberosString((from)->salt, (to)->salt)) return ENOMEM;
}else
(to)->salt = NULL;
if((from)->s2kparams) {
(to)->s2kparams = malloc(sizeof(*(to)->s2kparams));
if((to)->s2kparams == NULL) return ENOMEM;
if(copy_octet_string((from)->s2kparams, (to)->s2kparams)) return ENOMEM;
}else
(to)->s2kparams = NULL;
return 0;
}

