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
encode_PrincipalName(unsigned char *p, size_t len, const PrincipalName *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
int oldret = ret;
ret = 0;
for(i = (&(data)->name_string)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, &(&(data)->name_string)->val[i], &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_NAME_TYPE(p, len, &(data)->name_type, &l);
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
decode_PrincipalName(const unsigned char *p, size_t len, PrincipalName *data, size_t *size)
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
e = decode_NAME_TYPE(p, len, &(data)->name_type, &l);
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
e = der_match_tag_and_length (p, len, UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
if(len < reallen)
return ASN1_OVERRUN;
len = reallen;
{
size_t origlen = len;
int oldret = ret;
ret = 0;
(&(data)->name_string)->len = 0;
(&(data)->name_string)->val = NULL;
while(ret < origlen) {
(&(data)->name_string)->len++;
(&(data)->name_string)->val = realloc((&(data)->name_string)->val, sizeof(*((&(data)->name_string)->val)) * (&(data)->name_string)->len);
e = decode_general_string(p, len, &(&(data)->name_string)->val[(&(data)->name_string)->len-1], &l);
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
free_PrincipalName(data);
return e;
}

void
free_PrincipalName(PrincipalName *data)
{
free_NAME_TYPE(&(data)->name_type);
while((&(data)->name_string)->len){
free_general_string(&(&(data)->name_string)->val[(&(data)->name_string)->len-1]);
(&(data)->name_string)->len--;
}
free((&(data)->name_string)->val);
}

size_t
length_PrincipalName(const PrincipalName *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_NAME_TYPE(&(data)->name_type);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = (&(data)->name_string)->len - 1; i >= 0; --i){
ret += length_general_string(&(&(data)->name_string)->val[i]);
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_PrincipalName(const PrincipalName *from, PrincipalName *to)
{
if(copy_NAME_TYPE(&(from)->name_type, &(to)->name_type)) return ENOMEM;
if(((&(to)->name_string)->val = malloc((&(from)->name_string)->len * sizeof(*(&(to)->name_string)->val))) == NULL && (&(from)->name_string)->len != 0)
return ENOMEM;
for((&(to)->name_string)->len = 0; (&(to)->name_string)->len < (&(from)->name_string)->len; (&(to)->name_string)->len++){
if(copy_general_string(&(&(from)->name_string)->val[(&(to)->name_string)->len], &(&(to)->name_string)->val[(&(to)->name_string)->len])) return ENOMEM;
}
return 0;
}

