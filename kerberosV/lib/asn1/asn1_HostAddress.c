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
encode_HostAddress(unsigned char *p, size_t len, const HostAddress *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, &(data)->address, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->addr_type, &l);
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
decode_HostAddress(const unsigned char *p, size_t len, HostAddress *data, size_t *size)
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
e = decode_integer(p, len, &(data)->addr_type, &l);
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
e = decode_octet_string(p, len, &(data)->address, &l);
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
free_HostAddress(data);
return e;
}

void
free_HostAddress(HostAddress *data)
{
free_octet_string(&(data)->address);
}

size_t
length_HostAddress(const HostAddress *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->addr_type);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_octet_string(&(data)->address);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_HostAddress(const HostAddress *from, HostAddress *to)
{
*(&(to)->addr_type) = *(&(from)->addr_type);
if(copy_octet_string(&(from)->address, &(to)->address)) return ENOMEM;
return 0;
}

