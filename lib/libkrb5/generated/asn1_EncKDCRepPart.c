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
encode_EncKDCRepPart(unsigned char *p, size_t len, const EncKDCRepPart *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->caddr)
{
int oldret = ret;
ret = 0;
e = encode_HostAddresses(p, len, (data)->caddr, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 11, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, &(data)->sname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 10, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, &(data)->srealm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 9, &l);
BACK;
ret += oldret;
}
if((data)->renew_till)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->renew_till, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 8, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, &(data)->endtime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 7, &l);
BACK;
ret += oldret;
}
if((data)->starttime)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->starttime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 6, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, &(data)->authtime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_TicketFlags(p, len, &(data)->flags, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
if((data)->key_expiration)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->key_expiration, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->nonce, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_LastReq(p, len, &(data)->last_req, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_EncryptionKey(p, len, &(data)->key, &l);
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
decode_EncKDCRepPart(const unsigned char *p, size_t len, EncKDCRepPart *data, size_t *size)
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
e = decode_EncryptionKey(p, len, &(data)->key, &l);
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
e = decode_LastReq(p, len, &(data)->last_req, &l);
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
e = decode_integer(p, len, &(data)->nonce, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 3, &l);
if (e)
(data)->key_expiration = NULL;
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
(data)->key_expiration = malloc(sizeof(*(data)->key_expiration));
if((data)->key_expiration == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->key_expiration, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 4, &l);
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
e = decode_TicketFlags(p, len, &(data)->flags, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 5, &l);
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
e = decode_KerberosTime(p, len, &(data)->authtime, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 6, &l);
if (e)
(data)->starttime = NULL;
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
(data)->starttime = malloc(sizeof(*(data)->starttime));
if((data)->starttime == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->starttime, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 7, &l);
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
e = decode_KerberosTime(p, len, &(data)->endtime, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 8, &l);
if (e)
(data)->renew_till = NULL;
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
(data)->renew_till = malloc(sizeof(*(data)->renew_till));
if((data)->renew_till == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->renew_till, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 9, &l);
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
e = decode_Realm(p, len, &(data)->srealm, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 10, &l);
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
e = decode_PrincipalName(p, len, &(data)->sname, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 11, &l);
if (e)
(data)->caddr = NULL;
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
(data)->caddr = malloc(sizeof(*(data)->caddr));
if((data)->caddr == NULL) return ENOMEM;
e = decode_HostAddresses(p, len, (data)->caddr, &l);
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
free_EncKDCRepPart(data);
return e;
}

void
free_EncKDCRepPart(EncKDCRepPart *data)
{
free_EncryptionKey(&(data)->key);
free_LastReq(&(data)->last_req);
if((data)->key_expiration) {
free_KerberosTime((data)->key_expiration);
free((data)->key_expiration);
}
free_TicketFlags(&(data)->flags);
free_KerberosTime(&(data)->authtime);
if((data)->starttime) {
free_KerberosTime((data)->starttime);
free((data)->starttime);
}
free_KerberosTime(&(data)->endtime);
if((data)->renew_till) {
free_KerberosTime((data)->renew_till);
free((data)->renew_till);
}
free_Realm(&(data)->srealm);
free_PrincipalName(&(data)->sname);
if((data)->caddr) {
free_HostAddresses((data)->caddr);
free((data)->caddr);
}
}

size_t
length_EncKDCRepPart(const EncKDCRepPart *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_EncryptionKey(&(data)->key);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_LastReq(&(data)->last_req);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->nonce);
ret += 1 + length_len(ret) + oldret;
}
if((data)->key_expiration){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->key_expiration);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_TicketFlags(&(data)->flags);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_KerberosTime(&(data)->authtime);
ret += 1 + length_len(ret) + oldret;
}
if((data)->starttime){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->starttime);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_KerberosTime(&(data)->endtime);
ret += 1 + length_len(ret) + oldret;
}
if((data)->renew_till){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->renew_till);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_Realm(&(data)->srealm);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_PrincipalName(&(data)->sname);
ret += 1 + length_len(ret) + oldret;
}
if((data)->caddr){
int oldret = ret;
ret = 0;
ret += length_HostAddresses((data)->caddr);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_EncKDCRepPart(const EncKDCRepPart *from, EncKDCRepPart *to)
{
if(copy_EncryptionKey(&(from)->key, &(to)->key)) return ENOMEM;
if(copy_LastReq(&(from)->last_req, &(to)->last_req)) return ENOMEM;
*(&(to)->nonce) = *(&(from)->nonce);
if((from)->key_expiration) {
(to)->key_expiration = malloc(sizeof(*(to)->key_expiration));
if((to)->key_expiration == NULL) return ENOMEM;
if(copy_KerberosTime((from)->key_expiration, (to)->key_expiration)) return ENOMEM;
}else
(to)->key_expiration = NULL;
if(copy_TicketFlags(&(from)->flags, &(to)->flags)) return ENOMEM;
if(copy_KerberosTime(&(from)->authtime, &(to)->authtime)) return ENOMEM;
if((from)->starttime) {
(to)->starttime = malloc(sizeof(*(to)->starttime));
if((to)->starttime == NULL) return ENOMEM;
if(copy_KerberosTime((from)->starttime, (to)->starttime)) return ENOMEM;
}else
(to)->starttime = NULL;
if(copy_KerberosTime(&(from)->endtime, &(to)->endtime)) return ENOMEM;
if((from)->renew_till) {
(to)->renew_till = malloc(sizeof(*(to)->renew_till));
if((to)->renew_till == NULL) return ENOMEM;
if(copy_KerberosTime((from)->renew_till, (to)->renew_till)) return ENOMEM;
}else
(to)->renew_till = NULL;
if(copy_Realm(&(from)->srealm, &(to)->srealm)) return ENOMEM;
if(copy_PrincipalName(&(from)->sname, &(to)->sname)) return ENOMEM;
if((from)->caddr) {
(to)->caddr = malloc(sizeof(*(to)->caddr));
if((to)->caddr == NULL) return ENOMEM;
if(copy_HostAddresses((from)->caddr, (to)->caddr)) return ENOMEM;
}else
(to)->caddr = NULL;
return 0;
}

