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
encode_KRB_ERROR(unsigned char *p, size_t len, const KRB_ERROR *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->e_data)
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, (data)->e_data, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 12, &l);
BACK;
ret += oldret;
}
if((data)->e_text)
{
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, (data)->e_text, &l);
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
e = encode_Realm(p, len, &(data)->realm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 9, &l);
BACK;
ret += oldret;
}
if((data)->cname)
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, (data)->cname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 8, &l);
BACK;
ret += oldret;
}
if((data)->crealm)
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, (data)->crealm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 7, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->error_code, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 6, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->susec, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, &(data)->stime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
if((data)->cusec)
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, (data)->cusec, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
if((data)->ctime)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->ctime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_MESSAGE_TYPE(p, len, &(data)->msg_type, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->pvno, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 0, &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, APPL, CONS, 30, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_KRB_ERROR(const unsigned char *p, size_t len, KRB_ERROR *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, APPL, CONS, 30, &reallen, &l);
FORW;
{
int dce_fix;
if((dce_fix = fix_dce(reallen, &len)) < 0)
return ASN1_BAD_FORMAT;
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
e = decode_integer(p, len, &(data)->pvno, &l);
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
e = decode_MESSAGE_TYPE(p, len, &(data)->msg_type, &l);
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
(data)->ctime = NULL;
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
(data)->ctime = malloc(sizeof(*(data)->ctime));
if((data)->ctime == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->ctime, &l);
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
(data)->cusec = NULL;
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
(data)->cusec = malloc(sizeof(*(data)->cusec));
if((data)->cusec == NULL) return ENOMEM;
e = decode_integer(p, len, (data)->cusec, &l);
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
e = decode_KerberosTime(p, len, &(data)->stime, &l);
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
e = decode_integer(p, len, &(data)->susec, &l);
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
e = decode_integer(p, len, &(data)->error_code, &l);
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
(data)->crealm = NULL;
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
(data)->crealm = malloc(sizeof(*(data)->crealm));
if((data)->crealm == NULL) return ENOMEM;
e = decode_Realm(p, len, (data)->crealm, &l);
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
(data)->cname = NULL;
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
(data)->cname = malloc(sizeof(*(data)->cname));
if((data)->cname == NULL) return ENOMEM;
e = decode_PrincipalName(p, len, (data)->cname, &l);
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
e = decode_Realm(p, len, &(data)->realm, &l);
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
(data)->e_text = NULL;
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
(data)->e_text = malloc(sizeof(*(data)->e_text));
if((data)->e_text == NULL) return ENOMEM;
e = decode_general_string(p, len, (data)->e_text, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 12, &l);
if (e)
(data)->e_data = NULL;
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
(data)->e_data = malloc(sizeof(*(data)->e_data));
if((data)->e_data == NULL) return ENOMEM;
e = decode_octet_string(p, len, (data)->e_data, &l);
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
free_KRB_ERROR(data);
return e;
}

void
free_KRB_ERROR(KRB_ERROR *data)
{
free_MESSAGE_TYPE(&(data)->msg_type);
if((data)->ctime) {
free_KerberosTime((data)->ctime);
free((data)->ctime);
}
if((data)->cusec) {
free((data)->cusec);
}
free_KerberosTime(&(data)->stime);
if((data)->crealm) {
free_Realm((data)->crealm);
free((data)->crealm);
}
if((data)->cname) {
free_PrincipalName((data)->cname);
free((data)->cname);
}
free_Realm(&(data)->realm);
free_PrincipalName(&(data)->sname);
if((data)->e_text) {
free_general_string((data)->e_text);
free((data)->e_text);
}
if((data)->e_data) {
free_octet_string((data)->e_data);
free((data)->e_data);
}
}

size_t
length_KRB_ERROR(const KRB_ERROR *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->pvno);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_MESSAGE_TYPE(&(data)->msg_type);
ret += 1 + length_len(ret) + oldret;
}
if((data)->ctime){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->ctime);
ret += 1 + length_len(ret) + oldret;
}
if((data)->cusec){
int oldret = ret;
ret = 0;
ret += length_integer((data)->cusec);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_KerberosTime(&(data)->stime);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->susec);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->error_code);
ret += 1 + length_len(ret) + oldret;
}
if((data)->crealm){
int oldret = ret;
ret = 0;
ret += length_Realm((data)->crealm);
ret += 1 + length_len(ret) + oldret;
}
if((data)->cname){
int oldret = ret;
ret = 0;
ret += length_PrincipalName((data)->cname);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_Realm(&(data)->realm);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_PrincipalName(&(data)->sname);
ret += 1 + length_len(ret) + oldret;
}
if((data)->e_text){
int oldret = ret;
ret = 0;
ret += length_general_string((data)->e_text);
ret += 1 + length_len(ret) + oldret;
}
if((data)->e_data){
int oldret = ret;
ret = 0;
ret += length_octet_string((data)->e_data);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
ret += 1 + length_len (ret);
return ret;
}

int
copy_KRB_ERROR(const KRB_ERROR *from, KRB_ERROR *to)
{
*(&(to)->pvno) = *(&(from)->pvno);
if(copy_MESSAGE_TYPE(&(from)->msg_type, &(to)->msg_type)) return ENOMEM;
if((from)->ctime) {
(to)->ctime = malloc(sizeof(*(to)->ctime));
if((to)->ctime == NULL) return ENOMEM;
if(copy_KerberosTime((from)->ctime, (to)->ctime)) return ENOMEM;
}else
(to)->ctime = NULL;
if((from)->cusec) {
(to)->cusec = malloc(sizeof(*(to)->cusec));
if((to)->cusec == NULL) return ENOMEM;
*((to)->cusec) = *((from)->cusec);
}else
(to)->cusec = NULL;
if(copy_KerberosTime(&(from)->stime, &(to)->stime)) return ENOMEM;
*(&(to)->susec) = *(&(from)->susec);
*(&(to)->error_code) = *(&(from)->error_code);
if((from)->crealm) {
(to)->crealm = malloc(sizeof(*(to)->crealm));
if((to)->crealm == NULL) return ENOMEM;
if(copy_Realm((from)->crealm, (to)->crealm)) return ENOMEM;
}else
(to)->crealm = NULL;
if((from)->cname) {
(to)->cname = malloc(sizeof(*(to)->cname));
if((to)->cname == NULL) return ENOMEM;
if(copy_PrincipalName((from)->cname, (to)->cname)) return ENOMEM;
}else
(to)->cname = NULL;
if(copy_Realm(&(from)->realm, &(to)->realm)) return ENOMEM;
if(copy_PrincipalName(&(from)->sname, &(to)->sname)) return ENOMEM;
if((from)->e_text) {
(to)->e_text = malloc(sizeof(*(to)->e_text));
if((to)->e_text == NULL) return ENOMEM;
if(copy_general_string((from)->e_text, (to)->e_text)) return ENOMEM;
}else
(to)->e_text = NULL;
if((from)->e_data) {
(to)->e_data = malloc(sizeof(*(to)->e_data));
if((to)->e_data == NULL) return ENOMEM;
if(copy_octet_string((from)->e_data, (to)->e_data)) return ENOMEM;
}else
(to)->e_data = NULL;
return 0;
}

