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
encode_NegTokenTarg(unsigned char *p, size_t len, const NegTokenTarg *data, size_t *size)
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
if((data)->responseToken)
{
int oldret = ret;
ret = 0;
e = encode_octet_string(p, len, (data)->responseToken, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->supportedMech)
{
int oldret = ret;
ret = 0;
e = encode_MechType(p, len, (data)->supportedMech, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
if((data)->negResult)
{
int oldret = ret;
ret = 0;
e = encode_enumerated(p, len, (data)->negResult, &l);
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
decode_NegTokenTarg(const unsigned char *p, size_t len, NegTokenTarg *data, size_t *size)
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
(data)->negResult = NULL;
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
(data)->negResult = malloc(sizeof(*(data)->negResult));
if((data)->negResult == NULL) return ENOMEM;
e = decode_enumerated(p, len, (data)->negResult, &l);
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
(data)->supportedMech = NULL;
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
(data)->supportedMech = malloc(sizeof(*(data)->supportedMech));
if((data)->supportedMech == NULL) return ENOMEM;
e = decode_MechType(p, len, (data)->supportedMech, &l);
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
(data)->responseToken = NULL;
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
(data)->responseToken = malloc(sizeof(*(data)->responseToken));
if((data)->responseToken == NULL) return ENOMEM;
e = decode_octet_string(p, len, (data)->responseToken, &l);
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
free_NegTokenTarg(data);
return e;
}

void
free_NegTokenTarg(NegTokenTarg *data)
{
if((data)->negResult) {
free((data)->negResult);
(data)->negResult = NULL;
}
if((data)->supportedMech) {
free_MechType((data)->supportedMech);
free((data)->supportedMech);
(data)->supportedMech = NULL;
}
if((data)->responseToken) {
free_octet_string((data)->responseToken);
free((data)->responseToken);
(data)->responseToken = NULL;
}
if((data)->mechListMIC) {
free_octet_string((data)->mechListMIC);
free((data)->mechListMIC);
(data)->mechListMIC = NULL;
}
}

size_t
length_NegTokenTarg(const NegTokenTarg *data)
{
size_t ret = 0;
if((data)->negResult){
int oldret = ret;
ret = 0;
ret += length_enumerated((data)->negResult);
ret += 1 + length_len(ret) + oldret;
}
if((data)->supportedMech){
int oldret = ret;
ret = 0;
ret += length_MechType((data)->supportedMech);
ret += 1 + length_len(ret) + oldret;
}
if((data)->responseToken){
int oldret = ret;
ret = 0;
ret += length_octet_string((data)->responseToken);
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
copy_NegTokenTarg(const NegTokenTarg *from, NegTokenTarg *to)
{
if((from)->negResult) {
(to)->negResult = malloc(sizeof(*(to)->negResult));
if((to)->negResult == NULL) return ENOMEM;
*((to)->negResult) = *((from)->negResult);
}else
(to)->negResult = NULL;
if((from)->supportedMech) {
(to)->supportedMech = malloc(sizeof(*(to)->supportedMech));
if((to)->supportedMech == NULL) return ENOMEM;
if(copy_MechType((from)->supportedMech, (to)->supportedMech)) return ENOMEM;
}else
(to)->supportedMech = NULL;
if((from)->responseToken) {
(to)->responseToken = malloc(sizeof(*(to)->responseToken));
if((to)->responseToken == NULL) return ENOMEM;
if(copy_octet_string((from)->responseToken, (to)->responseToken)) return ENOMEM;
}else
(to)->responseToken = NULL;
if((from)->mechListMIC) {
(to)->mechListMIC = malloc(sizeof(*(to)->mechListMIC));
if((to)->mechListMIC == NULL) return ENOMEM;
if(copy_octet_string((from)->mechListMIC, (to)->mechListMIC)) return ENOMEM;
}else
(to)->mechListMIC = NULL;
return 0;
}

