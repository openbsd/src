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
encode_KDC_REQ_BODY(unsigned char *p, size_t len, const KDC_REQ_BODY *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->additional_tickets)
{
int oldret = ret;
ret = 0;
for(i = ((data)->additional_tickets)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
e = encode_Ticket(p, len, &((data)->additional_tickets)->val[i], &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 11, &l);
BACK;
ret += oldret;
}
if((data)->enc_authorization_data)
{
int oldret = ret;
ret = 0;
e = encode_EncryptedData(p, len, (data)->enc_authorization_data, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 10, &l);
BACK;
ret += oldret;
}
if((data)->addresses)
{
int oldret = ret;
ret = 0;
e = encode_HostAddresses(p, len, (data)->addresses, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 9, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
for(i = (&(data)->etype)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
e = encode_ENCTYPE(p, len, &(&(data)->etype)->val[i], &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 8, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->nonce, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 7, &l);
BACK;
ret += oldret;
}
if((data)->rtime)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->rtime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 6, &l);
BACK;
ret += oldret;
}
if((data)->till)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->till, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
if((data)->from)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->from, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
if((data)->sname)
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, (data)->sname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, &(data)->realm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->cname)
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, (data)->cname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KDCOptions(p, len, &(data)->kdc_options, &l);
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
decode_KDC_REQ_BODY(const unsigned char *p, size_t len, KDC_REQ_BODY *data, size_t *size)
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
e = decode_KDCOptions(p, len, &(data)->kdc_options, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 3, &l);
if (e)
(data)->sname = NULL;
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
(data)->sname = malloc(sizeof(*(data)->sname));
if((data)->sname == NULL) return ENOMEM;
e = decode_PrincipalName(p, len, (data)->sname, &l);
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
(data)->from = NULL;
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
(data)->from = malloc(sizeof(*(data)->from));
if((data)->from == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->from, &l);
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
(data)->till = NULL;
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
(data)->till = malloc(sizeof(*(data)->till));
if((data)->till == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->till, &l);
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
(data)->rtime = NULL;
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
(data)->rtime = malloc(sizeof(*(data)->rtime));
if((data)->rtime == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->rtime, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 8, &l);
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
(&(data)->etype)->len = 0;
(&(data)->etype)->val = NULL;
while(ret < origlen) {
(&(data)->etype)->len++;
(&(data)->etype)->val = realloc((&(data)->etype)->val, sizeof(*((&(data)->etype)->val)) * (&(data)->etype)->len);
e = decode_ENCTYPE(p, len, &(&(data)->etype)->val[(&(data)->etype)->len-1], &l);
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
{
size_t newlen, oldlen;

e = der_match_tag (p, len, CONTEXT, CONS, 9, &l);
if (e)
(data)->addresses = NULL;
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
(data)->addresses = malloc(sizeof(*(data)->addresses));
if((data)->addresses == NULL) return ENOMEM;
e = decode_HostAddresses(p, len, (data)->addresses, &l);
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
(data)->enc_authorization_data = NULL;
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
(data)->enc_authorization_data = malloc(sizeof(*(data)->enc_authorization_data));
if((data)->enc_authorization_data == NULL) return ENOMEM;
e = decode_EncryptedData(p, len, (data)->enc_authorization_data, &l);
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
(data)->additional_tickets = NULL;
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
(data)->additional_tickets = malloc(sizeof(*(data)->additional_tickets));
if((data)->additional_tickets == NULL) return ENOMEM;
e = der_match_tag_and_length (p, len, UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
if(len < reallen)
return ASN1_OVERRUN;
len = reallen;
{
size_t origlen = len;
int oldret = ret;
ret = 0;
((data)->additional_tickets)->len = 0;
((data)->additional_tickets)->val = NULL;
while(ret < origlen) {
((data)->additional_tickets)->len++;
((data)->additional_tickets)->val = realloc(((data)->additional_tickets)->val, sizeof(*(((data)->additional_tickets)->val)) * ((data)->additional_tickets)->len);
e = decode_Ticket(p, len, &((data)->additional_tickets)->val[((data)->additional_tickets)->len-1], &l);
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
free_KDC_REQ_BODY(data);
return e;
}

void
free_KDC_REQ_BODY(KDC_REQ_BODY *data)
{
free_KDCOptions(&(data)->kdc_options);
if((data)->cname) {
free_PrincipalName((data)->cname);
free((data)->cname);
}
free_Realm(&(data)->realm);
if((data)->sname) {
free_PrincipalName((data)->sname);
free((data)->sname);
}
if((data)->from) {
free_KerberosTime((data)->from);
free((data)->from);
}
if((data)->till) {
free_KerberosTime((data)->till);
free((data)->till);
}
if((data)->rtime) {
free_KerberosTime((data)->rtime);
free((data)->rtime);
}
while((&(data)->etype)->len){
free_ENCTYPE(&(&(data)->etype)->val[(&(data)->etype)->len-1]);
(&(data)->etype)->len--;
}
free((&(data)->etype)->val);
if((data)->addresses) {
free_HostAddresses((data)->addresses);
free((data)->addresses);
}
if((data)->enc_authorization_data) {
free_EncryptedData((data)->enc_authorization_data);
free((data)->enc_authorization_data);
}
if((data)->additional_tickets) {
while(((data)->additional_tickets)->len){
free_Ticket(&((data)->additional_tickets)->val[((data)->additional_tickets)->len-1]);
((data)->additional_tickets)->len--;
}
free(((data)->additional_tickets)->val);
free((data)->additional_tickets);
}
}

size_t
length_KDC_REQ_BODY(const KDC_REQ_BODY *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_KDCOptions(&(data)->kdc_options);
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
if((data)->sname){
int oldret = ret;
ret = 0;
ret += length_PrincipalName((data)->sname);
ret += 1 + length_len(ret) + oldret;
}
if((data)->from){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->from);
ret += 1 + length_len(ret) + oldret;
}
if((data)->till){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->till);
ret += 1 + length_len(ret) + oldret;
}
if((data)->rtime){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->rtime);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->nonce);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = (&(data)->etype)->len - 1; i >= 0; --i){
ret += length_ENCTYPE(&(&(data)->etype)->val[i]);
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret) + oldret;
}
if((data)->addresses){
int oldret = ret;
ret = 0;
ret += length_HostAddresses((data)->addresses);
ret += 1 + length_len(ret) + oldret;
}
if((data)->enc_authorization_data){
int oldret = ret;
ret = 0;
ret += length_EncryptedData((data)->enc_authorization_data);
ret += 1 + length_len(ret) + oldret;
}
if((data)->additional_tickets){
int oldret = ret;
ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = ((data)->additional_tickets)->len - 1; i >= 0; --i){
ret += length_Ticket(&((data)->additional_tickets)->val[i]);
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_KDC_REQ_BODY(const KDC_REQ_BODY *from, KDC_REQ_BODY *to)
{
if(copy_KDCOptions(&(from)->kdc_options, &(to)->kdc_options)) return ENOMEM;
if((from)->cname) {
(to)->cname = malloc(sizeof(*(to)->cname));
if((to)->cname == NULL) return ENOMEM;
if(copy_PrincipalName((from)->cname, (to)->cname)) return ENOMEM;
}else
(to)->cname = NULL;
if(copy_Realm(&(from)->realm, &(to)->realm)) return ENOMEM;
if((from)->sname) {
(to)->sname = malloc(sizeof(*(to)->sname));
if((to)->sname == NULL) return ENOMEM;
if(copy_PrincipalName((from)->sname, (to)->sname)) return ENOMEM;
}else
(to)->sname = NULL;
if((from)->from) {
(to)->from = malloc(sizeof(*(to)->from));
if((to)->from == NULL) return ENOMEM;
if(copy_KerberosTime((from)->from, (to)->from)) return ENOMEM;
}else
(to)->from = NULL;
if((from)->till) {
(to)->till = malloc(sizeof(*(to)->till));
if((to)->till == NULL) return ENOMEM;
if(copy_KerberosTime((from)->till, (to)->till)) return ENOMEM;
}else
(to)->till = NULL;
if((from)->rtime) {
(to)->rtime = malloc(sizeof(*(to)->rtime));
if((to)->rtime == NULL) return ENOMEM;
if(copy_KerberosTime((from)->rtime, (to)->rtime)) return ENOMEM;
}else
(to)->rtime = NULL;
*(&(to)->nonce) = *(&(from)->nonce);
if(((&(to)->etype)->val = malloc((&(from)->etype)->len * sizeof(*(&(to)->etype)->val))) == NULL && (&(from)->etype)->len != 0)
return ENOMEM;
for((&(to)->etype)->len = 0; (&(to)->etype)->len < (&(from)->etype)->len; (&(to)->etype)->len++){
if(copy_ENCTYPE(&(&(from)->etype)->val[(&(to)->etype)->len], &(&(to)->etype)->val[(&(to)->etype)->len])) return ENOMEM;
}
if((from)->addresses) {
(to)->addresses = malloc(sizeof(*(to)->addresses));
if((to)->addresses == NULL) return ENOMEM;
if(copy_HostAddresses((from)->addresses, (to)->addresses)) return ENOMEM;
}else
(to)->addresses = NULL;
if((from)->enc_authorization_data) {
(to)->enc_authorization_data = malloc(sizeof(*(to)->enc_authorization_data));
if((to)->enc_authorization_data == NULL) return ENOMEM;
if(copy_EncryptedData((from)->enc_authorization_data, (to)->enc_authorization_data)) return ENOMEM;
}else
(to)->enc_authorization_data = NULL;
if((from)->additional_tickets) {
(to)->additional_tickets = malloc(sizeof(*(to)->additional_tickets));
if((to)->additional_tickets == NULL) return ENOMEM;
if((((to)->additional_tickets)->val = malloc(((from)->additional_tickets)->len * sizeof(*((to)->additional_tickets)->val))) == NULL && ((from)->additional_tickets)->len != 0)
return ENOMEM;
for(((to)->additional_tickets)->len = 0; ((to)->additional_tickets)->len < ((from)->additional_tickets)->len; ((to)->additional_tickets)->len++){
if(copy_Ticket(&((from)->additional_tickets)->val[((to)->additional_tickets)->len], &((to)->additional_tickets)->val[((to)->additional_tickets)->len])) return ENOMEM;
}
}else
(to)->additional_tickets = NULL;
return 0;
}

