/* Generated from /home/biorn/src/lib/libgssapi/../../kerberosV/src/lib/gssapi/spnego.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <spnego_asn1.h>
#include <asn1_err.h>
#include <der.h>
#include <parse_units.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_NegTokenInit(unsigned char *p, size_t len, const NegTokenInit *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->mechListMIC)
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, (data)->mechListMIC, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
if((data)->mechToken)
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, (data)->mechToken, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->reqFlags)
{
int oldret = ret;
ret = 0;
e = encode_ContextFlags(p, len, (data)->reqFlags, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
if((data)->mechTypes)
{
int oldret = ret;
ret = 0;
e = encode_MechTypeList(p, len, (data)->mechTypes, &l);
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
decode_NegTokenInit(const unsigned char *p, size_t len, NegTokenInit *data, size_t *size)
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
(data)->mechTypes = NULL;
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
(data)->mechTypes = malloc(sizeof(*(data)->mechTypes));
if((data)->mechTypes == NULL) return ENOMEM;
e = decode_MechTypeList(p, len, (data)->mechTypes, &l);
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
(data)->reqFlags = NULL;
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
(data)->reqFlags = malloc(sizeof(*(data)->reqFlags));
if((data)->reqFlags == NULL) return ENOMEM;
e = decode_ContextFlags(p, len, (data)->reqFlags, &l);
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
(data)->mechToken = NULL;
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
(data)->mechToken = malloc(sizeof(*(data)->mechToken));
if((data)->mechToken == NULL) return ENOMEM;
e = decode_octet_string(p, len, (data)->mechToken, &l);
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
(data)->mechListMIC = NULL;
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
(data)->mechListMIC = malloc(sizeof(*(data)->mechListMIC));
if((data)->mechListMIC == NULL) return ENOMEM;
e = decode_octet_string(p, len, (data)->mechListMIC, &l);
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
free_NegTokenInit(data);
return e;
}

void
free_NegTokenInit(NegTokenInit *data)
{
if((data)->mechTypes) {
free_MechTypeList((data)->mechTypes);
free((data)->mechTypes);
(data)->mechTypes = NULL;
}
if((data)->reqFlags) {
free_ContextFlags((data)->reqFlags);
free((data)->reqFlags);
(data)->reqFlags = NULL;
}
if((data)->mechToken) {
free_octet_string((data)->mechToken);
free((data)->mechToken);
(data)->mechToken = NULL;
}
if((data)->mechListMIC) {
free_octet_string((data)->mechListMIC);
free((data)->mechListMIC);
(data)->mechListMIC = NULL;
}
}

size_t
length_NegTokenInit(const NegTokenInit *data)
{
size_t ret = 0;
if((data)->mechTypes){
int oldret = ret;
ret = 0;
ret += length_MechTypeList((data)->mechTypes);
ret += 1 + length_len(ret) + oldret;
}
if((data)->reqFlags){
int oldret = ret;
ret = 0;
ret += length_ContextFlags((data)->reqFlags);
ret += 1 + length_len(ret) + oldret;
}
if((data)->mechToken){
int oldret = ret;
ret = 0;
ret += length_octet_string((data)->mechToken);
ret += 1 + length_len(ret) + oldret;
}
if((data)->mechListMIC){
int oldret = ret;
ret = 0;
ret += length_octet_string((data)->mechListMIC);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_NegTokenInit(const NegTokenInit *from, NegTokenInit *to)
{
if((from)->mechTypes) {
(to)->mechTypes = malloc(sizeof(*(to)->mechTypes));
if((to)->mechTypes == NULL) return ENOMEM;
if(copy_MechTypeList((from)->mechTypes, (to)->mechTypes)) return ENOMEM;
}else
(to)->mechTypes = NULL;
if((from)->reqFlags) {
(to)->reqFlags = malloc(sizeof(*(to)->reqFlags));
if((to)->reqFlags == NULL) return ENOMEM;
if(copy_ContextFlags((from)->reqFlags, (to)->reqFlags)) return ENOMEM;
}else
(to)->reqFlags = NULL;
if((from)->mechToken) {
(to)->mechToken = malloc(sizeof(*(to)->mechToken));
if((to)->mechToken == NULL) return ENOMEM;
if(copy_octet_string((from)->mechToken, (to)->mechToken)) return ENOMEM;
}else
(to)->mechToken = NULL;
if((from)->mechListMIC) {
(to)->mechListMIC = malloc(sizeof(*(to)->mechListMIC));
if((to)->mechListMIC == NULL) return ENOMEM;
if(copy_octet_string((from)->mechListMIC, (to)->mechListMIC)) return ENOMEM;
}else
(to)->mechListMIC = NULL;
return 0;
}

