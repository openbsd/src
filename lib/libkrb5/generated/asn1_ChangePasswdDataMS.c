/* Generated from /usr/src/lib/libkrb5/../../kerberosV/src/lib/asn1/k5.asn1 */
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
encode_ChangePasswdDataMS(unsigned char *p, size_t len, const ChangePasswdDataMS *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->targrealm)
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, (data)->targrealm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->targname)
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, (data)->targname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, &(data)->newpasswd, &l);
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
decode_ChangePasswdDataMS(const unsigned char *p, size_t len, ChangePasswdDataMS *data, size_t *size)
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
e = decode_octet_string(p, len, &(data)->newpasswd, &l);
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
(data)->targname = NULL;
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
(data)->targname = malloc(sizeof(*(data)->targname));
if((data)->targname == NULL) return ENOMEM;
e = decode_PrincipalName(p, len, (data)->targname, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 2, &l);
if (e)
(data)->targrealm = NULL;
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
(data)->targrealm = malloc(sizeof(*(data)->targrealm));
if((data)->targrealm == NULL) return ENOMEM;
e = decode_Realm(p, len, (data)->targrealm, &l);
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
free_ChangePasswdDataMS(data);
return e;
}

void
free_ChangePasswdDataMS(ChangePasswdDataMS *data)
{
free_octet_string(&(data)->newpasswd);
if((data)->targname) {
free_PrincipalName((data)->targname);
free((data)->targname);
}
if((data)->targrealm) {
free_Realm((data)->targrealm);
free((data)->targrealm);
}
}

size_t
length_ChangePasswdDataMS(const ChangePasswdDataMS *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_octet_string(&(data)->newpasswd);
ret += 1 + length_len(ret) + oldret;
}
if((data)->targname){
int oldret = ret;
ret = 0;
ret += length_PrincipalName((data)->targname);
ret += 1 + length_len(ret) + oldret;
}
if((data)->targrealm){
int oldret = ret;
ret = 0;
ret += length_Realm((data)->targrealm);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_ChangePasswdDataMS(const ChangePasswdDataMS *from, ChangePasswdDataMS *to)
{
if(copy_octet_string(&(from)->newpasswd, &(to)->newpasswd)) return ENOMEM;
if((from)->targname) {
(to)->targname = malloc(sizeof(*(to)->targname));
if((to)->targname == NULL) return ENOMEM;
if(copy_PrincipalName((from)->targname, (to)->targname)) return ENOMEM;
}else
(to)->targname = NULL;
if((from)->targrealm) {
(to)->targrealm = malloc(sizeof(*(to)->targrealm));
if((to)->targrealm == NULL) return ENOMEM;
if(copy_Realm((from)->targrealm, (to)->targrealm)) return ENOMEM;
}else
(to)->targrealm = NULL;
return 0;
}

