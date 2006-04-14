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
encode_Authenticator(unsigned char *p, size_t len, const Authenticator *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->authorization_data)
{
int oldret = ret;
ret = 0;
e = encode_AuthorizationData(p, len, (data)->authorization_data, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 8, &l);
BACK;
ret += oldret;
}
if((data)->seq_number)
{
int oldret = ret;
ret = 0;
e = encode_UNSIGNED(p, len, (data)->seq_number, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 7, &l);
BACK;
ret += oldret;
}
if((data)->subkey)
{
int oldret = ret;
ret = 0;
e = encode_EncryptionKey(p, len, (data)->subkey, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 6, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, &(data)->ctime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->cusec, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
if((data)->cksum)
{
int oldret = ret;
ret = 0;
e = encode_Checksum(p, len, (data)->cksum, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, &(data)->cname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, &(data)->crealm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->authenticator_vno, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_APPL, CONS, 2, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_Authenticator(const unsigned char *p, size_t len, Authenticator *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, ASN1_C_APPL, CONS, 2, &reallen, &l);
FORW;
{
int dce_fix;
if((dce_fix = fix_dce(reallen, &len)) < 0)
return ASN1_BAD_FORMAT;
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
e = decode_integer(p, len, &(data)->authenticator_vno, &l);
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
e = decode_Realm(p, len, &(data)->crealm, &l);
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
e = decode_PrincipalName(p, len, &(data)->cname, &l);
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
(data)->cksum = NULL;
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
(data)->cksum = malloc(sizeof(*(data)->cksum));
if((data)->cksum == NULL) return ENOMEM;
e = decode_Checksum(p, len, (data)->cksum, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 4, &l);
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
e = decode_integer(p, len, &(data)->cusec, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 5, &l);
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
e = decode_KerberosTime(p, len, &(data)->ctime, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 6, &l);
if (e)
(data)->subkey = NULL;
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
(data)->subkey = malloc(sizeof(*(data)->subkey));
if((data)->subkey == NULL) return ENOMEM;
e = decode_EncryptionKey(p, len, (data)->subkey, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 7, &l);
if (e)
(data)->seq_number = NULL;
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
(data)->seq_number = malloc(sizeof(*(data)->seq_number));
if((data)->seq_number == NULL) return ENOMEM;
e = decode_UNSIGNED(p, len, (data)->seq_number, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 8, &l);
if (e)
(data)->authorization_data = NULL;
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
(data)->authorization_data = malloc(sizeof(*(data)->authorization_data));
if((data)->authorization_data == NULL) return ENOMEM;
e = decode_AuthorizationData(p, len, (data)->authorization_data, &l);
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
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}
}
if(size) *size = ret;
return 0;
fail:
free_Authenticator(data);
return e;
}

void
free_Authenticator(Authenticator *data)
{
free_Realm(&(data)->crealm);
free_PrincipalName(&(data)->cname);
if((data)->cksum) {
free_Checksum((data)->cksum);
free((data)->cksum);
(data)->cksum = NULL;
}
free_KerberosTime(&(data)->ctime);
if((data)->subkey) {
free_EncryptionKey((data)->subkey);
free((data)->subkey);
(data)->subkey = NULL;
}
if((data)->seq_number) {
free_UNSIGNED((data)->seq_number);
free((data)->seq_number);
(data)->seq_number = NULL;
}
if((data)->authorization_data) {
free_AuthorizationData((data)->authorization_data);
free((data)->authorization_data);
(data)->authorization_data = NULL;
}
}

size_t
length_Authenticator(const Authenticator *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->authenticator_vno);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_Realm(&(data)->crealm);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_PrincipalName(&(data)->cname);
ret += 1 + length_len(ret) + oldret;
}
if((data)->cksum){
int oldret = ret;
ret = 0;
ret += length_Checksum((data)->cksum);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->cusec);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_KerberosTime(&(data)->ctime);
ret += 1 + length_len(ret) + oldret;
}
if((data)->subkey){
int oldret = ret;
ret = 0;
ret += length_EncryptionKey((data)->subkey);
ret += 1 + length_len(ret) + oldret;
}
if((data)->seq_number){
int oldret = ret;
ret = 0;
ret += length_UNSIGNED((data)->seq_number);
ret += 1 + length_len(ret) + oldret;
}
if((data)->authorization_data){
int oldret = ret;
ret = 0;
ret += length_AuthorizationData((data)->authorization_data);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
ret += 1 + length_len (ret);
return ret;
}

int
copy_Authenticator(const Authenticator *from, Authenticator *to)
{
*(&(to)->authenticator_vno) = *(&(from)->authenticator_vno);
if(copy_Realm(&(from)->crealm, &(to)->crealm)) return ENOMEM;
if(copy_PrincipalName(&(from)->cname, &(to)->cname)) return ENOMEM;
if((from)->cksum) {
(to)->cksum = malloc(sizeof(*(to)->cksum));
if((to)->cksum == NULL) return ENOMEM;
if(copy_Checksum((from)->cksum, (to)->cksum)) return ENOMEM;
}else
(to)->cksum = NULL;
*(&(to)->cusec) = *(&(from)->cusec);
if(copy_KerberosTime(&(from)->ctime, &(to)->ctime)) return ENOMEM;
if((from)->subkey) {
(to)->subkey = malloc(sizeof(*(to)->subkey));
if((to)->subkey == NULL) return ENOMEM;
if(copy_EncryptionKey((from)->subkey, (to)->subkey)) return ENOMEM;
}else
(to)->subkey = NULL;
if((from)->seq_number) {
(to)->seq_number = malloc(sizeof(*(to)->seq_number));
if((to)->seq_number == NULL) return ENOMEM;
if(copy_UNSIGNED((from)->seq_number, (to)->seq_number)) return ENOMEM;
}else
(to)->seq_number = NULL;
if((from)->authorization_data) {
(to)->authorization_data = malloc(sizeof(*(to)->authorization_data));
if((to)->authorization_data == NULL) return ENOMEM;
if(copy_AuthorizationData((from)->authorization_data, (to)->authorization_data)) return ENOMEM;
}else
(to)->authorization_data = NULL;
return 0;
}

