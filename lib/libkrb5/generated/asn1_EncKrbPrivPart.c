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
encode_EncKrbPrivPart(unsigned char *p, size_t len, const EncKrbPrivPart *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->r_address)
{
int oldret = ret;
ret = 0;
e = encode_HostAddress(p, len, (data)->r_address, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
if((data)->s_address)
{
int oldret = ret;
ret = 0;
e = encode_HostAddress(p, len, (data)->s_address, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
if((data)->seq_number)
{
int oldret = ret;
ret = 0;
e = encode_UNSIGNED(p, len, (data)->seq_number, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
if((data)->usec)
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, (data)->usec, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->timestamp)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->timestamp, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, &(data)->user_data, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_APPL, CONS, 28, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_EncKrbPrivPart(const unsigned char *p, size_t len, EncKrbPrivPart *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, ASN1_C_APPL, CONS, 28, &reallen, &l);
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
e = decode_octet_string(p, len, &(data)->user_data, &l);
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
(data)->timestamp = NULL;
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
(data)->timestamp = malloc(sizeof(*(data)->timestamp));
if((data)->timestamp == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->timestamp, &l);
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
(data)->usec = NULL;
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
(data)->usec = malloc(sizeof(*(data)->usec));
if((data)->usec == NULL) return ENOMEM;
e = decode_integer(p, len, (data)->usec, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 4, &l);
if (e)
(data)->s_address = NULL;
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
(data)->s_address = malloc(sizeof(*(data)->s_address));
if((data)->s_address == NULL) return ENOMEM;
e = decode_HostAddress(p, len, (data)->s_address, &l);
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
(data)->r_address = NULL;
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
(data)->r_address = malloc(sizeof(*(data)->r_address));
if((data)->r_address == NULL) return ENOMEM;
e = decode_HostAddress(p, len, (data)->r_address, &l);
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
free_EncKrbPrivPart(data);
return e;
}

void
free_EncKrbPrivPart(EncKrbPrivPart *data)
{
free_octet_string(&(data)->user_data);
if((data)->timestamp) {
free_KerberosTime((data)->timestamp);
free((data)->timestamp);
(data)->timestamp = NULL;
}
if((data)->usec) {
free((data)->usec);
(data)->usec = NULL;
}
if((data)->seq_number) {
free_UNSIGNED((data)->seq_number);
free((data)->seq_number);
(data)->seq_number = NULL;
}
if((data)->s_address) {
free_HostAddress((data)->s_address);
free((data)->s_address);
(data)->s_address = NULL;
}
if((data)->r_address) {
free_HostAddress((data)->r_address);
free((data)->r_address);
(data)->r_address = NULL;
}
}

size_t
length_EncKrbPrivPart(const EncKrbPrivPart *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_octet_string(&(data)->user_data);
ret += 1 + length_len(ret) + oldret;
}
if((data)->timestamp){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->timestamp);
ret += 1 + length_len(ret) + oldret;
}
if((data)->usec){
int oldret = ret;
ret = 0;
ret += length_integer((data)->usec);
ret += 1 + length_len(ret) + oldret;
}
if((data)->seq_number){
int oldret = ret;
ret = 0;
ret += length_UNSIGNED((data)->seq_number);
ret += 1 + length_len(ret) + oldret;
}
if((data)->s_address){
int oldret = ret;
ret = 0;
ret += length_HostAddress((data)->s_address);
ret += 1 + length_len(ret) + oldret;
}
if((data)->r_address){
int oldret = ret;
ret = 0;
ret += length_HostAddress((data)->r_address);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
ret += 1 + length_len (ret);
return ret;
}

int
copy_EncKrbPrivPart(const EncKrbPrivPart *from, EncKrbPrivPart *to)
{
if(copy_octet_string(&(from)->user_data, &(to)->user_data)) return ENOMEM;
if((from)->timestamp) {
(to)->timestamp = malloc(sizeof(*(to)->timestamp));
if((to)->timestamp == NULL) return ENOMEM;
if(copy_KerberosTime((from)->timestamp, (to)->timestamp)) return ENOMEM;
}else
(to)->timestamp = NULL;
if((from)->usec) {
(to)->usec = malloc(sizeof(*(to)->usec));
if((to)->usec == NULL) return ENOMEM;
*((to)->usec) = *((from)->usec);
}else
(to)->usec = NULL;
if((from)->seq_number) {
(to)->seq_number = malloc(sizeof(*(to)->seq_number));
if((to)->seq_number == NULL) return ENOMEM;
if(copy_UNSIGNED((from)->seq_number, (to)->seq_number)) return ENOMEM;
}else
(to)->seq_number = NULL;
if((from)->s_address) {
(to)->s_address = malloc(sizeof(*(to)->s_address));
if((to)->s_address == NULL) return ENOMEM;
if(copy_HostAddress((from)->s_address, (to)->s_address)) return ENOMEM;
}else
(to)->s_address = NULL;
if((from)->r_address) {
(to)->r_address = malloc(sizeof(*(to)->r_address));
if((to)->r_address == NULL) return ENOMEM;
if(copy_HostAddress((from)->r_address, (to)->r_address)) return ENOMEM;
}else
(to)->r_address = NULL;
return 0;
}

