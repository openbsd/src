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
encode_EncAPRepPart(unsigned char *p, size_t len, const EncAPRepPart *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->seq_number)
{
int oldret = ret;
ret = 0;
e = encode_UNSIGNED(p, len, (data)->seq_number, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
if((data)->subkey)
{
int oldret = ret;
ret = 0;
e = encode_EncryptionKey(p, len, (data)->subkey, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->cusec, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, &(data)->ctime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 0, &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, APPL, CONS, 27, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_EncAPRepPart(const unsigned char *p, size_t len, EncAPRepPart *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, APPL, CONS, 27, &reallen, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 2, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 3, &l);
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
free_EncAPRepPart(data);
return e;
}

void
free_EncAPRepPart(EncAPRepPart *data)
{
free_KerberosTime(&(data)->ctime);
if((data)->subkey) {
free_EncryptionKey((data)->subkey);
free((data)->subkey);
}
if((data)->seq_number) {
free_UNSIGNED((data)->seq_number);
free((data)->seq_number);
}
}

size_t
length_EncAPRepPart(const EncAPRepPart *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_KerberosTime(&(data)->ctime);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->cusec);
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
ret += 1 + length_len(ret);
ret += 1 + length_len (ret);
return ret;
}

int
copy_EncAPRepPart(const EncAPRepPart *from, EncAPRepPart *to)
{
if(copy_KerberosTime(&(from)->ctime, &(to)->ctime)) return ENOMEM;
*(&(to)->cusec) = *(&(from)->cusec);
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
return 0;
}

