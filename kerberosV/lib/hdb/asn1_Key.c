/* Generated from /usr/src/kerberosV/lib/hdb/../../src/lib/hdb/hdb.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <krb5_asn1.h>
#include <hdb_asn1.h>
#include <asn1_err.h>
#include <der.h>
#include <parse_units.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_Key(unsigned char *p, size_t len, const Key *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->salt)
{
int oldret = ret;
ret = 0;
e = encode_Salt(p, len, (data)->salt, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_EncryptionKey(p, len, &(data)->key, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
if((data)->mkvno)
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, (data)->mkvno, &l);
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
decode_Key(const unsigned char *p, size_t len, Key *data, size_t *size)
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
(data)->mkvno = NULL;
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
(data)->mkvno = malloc(sizeof(*(data)->mkvno));
if((data)->mkvno == NULL) return ENOMEM;
e = decode_integer(p, len, (data)->mkvno, &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 2, &l);
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
e = decode_Salt(p, len, (data)->salt, &l);
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
free_Key(data);
return e;
}

void
free_Key(Key *data)
{
if((data)->mkvno) {
free((data)->mkvno);
}
free_EncryptionKey(&(data)->key);
if((data)->salt) {
free_Salt((data)->salt);
free((data)->salt);
}
}

size_t
length_Key(const Key *data)
{
size_t ret = 0;
if((data)->mkvno){
int oldret = ret;
ret = 0;
ret += length_integer((data)->mkvno);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_EncryptionKey(&(data)->key);
ret += 1 + length_len(ret) + oldret;
}
if((data)->salt){
int oldret = ret;
ret = 0;
ret += length_Salt((data)->salt);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_Key(const Key *from, Key *to)
{
if((from)->mkvno) {
(to)->mkvno = malloc(sizeof(*(to)->mkvno));
if((to)->mkvno == NULL) return ENOMEM;
*((to)->mkvno) = *((from)->mkvno);
}else
(to)->mkvno = NULL;
if(copy_EncryptionKey(&(from)->key, &(to)->key)) return ENOMEM;
if((from)->salt) {
(to)->salt = malloc(sizeof(*(to)->salt));
if((to)->salt == NULL) return ENOMEM;
if(copy_Salt((from)->salt, (to)->salt)) return ENOMEM;
}else
(to)->salt = NULL;
return 0;
}

