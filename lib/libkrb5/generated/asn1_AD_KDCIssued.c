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
encode_AD_KDCIssued(unsigned char *p, size_t len, const AD_KDCIssued *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
int oldret = ret;
ret = 0;
e = encode_AuthorizationData(p, len, &(data)->elements, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
if((data)->i_sname)
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, (data)->i_sname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->i_realm)
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, (data)->i_realm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_Checksum(p, len, &(data)->ad_checksum, &l);
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
decode_AD_KDCIssued(const unsigned char *p, size_t len, AD_KDCIssued *data, size_t *size)
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
e = decode_Checksum(p, len, &(data)->ad_checksum, &l);
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
(data)->i_realm = NULL;
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
(data)->i_realm = malloc(sizeof(*(data)->i_realm));
if((data)->i_realm == NULL) return ENOMEM;
e = decode_Realm(p, len, (data)->i_realm, &l);
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
(data)->i_sname = NULL;
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
(data)->i_sname = malloc(sizeof(*(data)->i_sname));
if((data)->i_sname == NULL) return ENOMEM;
e = decode_PrincipalName(p, len, (data)->i_sname, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 3, &l);
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
e = decode_AuthorizationData(p, len, &(data)->elements, &l);
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
free_AD_KDCIssued(data);
return e;
}

void
free_AD_KDCIssued(AD_KDCIssued *data)
{
free_Checksum(&(data)->ad_checksum);
if((data)->i_realm) {
free_Realm((data)->i_realm);
free((data)->i_realm);
(data)->i_realm = NULL;
}
if((data)->i_sname) {
free_PrincipalName((data)->i_sname);
free((data)->i_sname);
(data)->i_sname = NULL;
}
free_AuthorizationData(&(data)->elements);
}

size_t
length_AD_KDCIssued(const AD_KDCIssued *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_Checksum(&(data)->ad_checksum);
ret += 1 + length_len(ret) + oldret;
}
if((data)->i_realm){
int oldret = ret;
ret = 0;
ret += length_Realm((data)->i_realm);
ret += 1 + length_len(ret) + oldret;
}
if((data)->i_sname){
int oldret = ret;
ret = 0;
ret += length_PrincipalName((data)->i_sname);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_AuthorizationData(&(data)->elements);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_AD_KDCIssued(const AD_KDCIssued *from, AD_KDCIssued *to)
{
if(copy_Checksum(&(from)->ad_checksum, &(to)->ad_checksum)) return ENOMEM;
if((from)->i_realm) {
(to)->i_realm = malloc(sizeof(*(to)->i_realm));
if((to)->i_realm == NULL) return ENOMEM;
if(copy_Realm((from)->i_realm, (to)->i_realm)) return ENOMEM;
}else
(to)->i_realm = NULL;
if((from)->i_sname) {
(to)->i_sname = malloc(sizeof(*(to)->i_sname));
if((to)->i_sname == NULL) return ENOMEM;
if(copy_PrincipalName((from)->i_sname, (to)->i_sname)) return ENOMEM;
}else
(to)->i_sname = NULL;
if(copy_AuthorizationData(&(from)->elements, &(to)->elements)) return ENOMEM;
return 0;
}

