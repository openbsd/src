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
encode_PA_SAM_CHALLENGE_2_BODY(unsigned char *p, size_t len, const PA_SAM_CHALLENGE_2_BODY *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->sam_etype, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 9, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->sam_nonce, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 8, &l);
BACK;
ret += oldret;
}
if((data)->sam_pk_for_sad)
{
int oldret = ret;
ret = 0;
e = encode_EncryptionKey(p, len, (data)->sam_pk_for_sad, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 7, &l);
BACK;
ret += oldret;
}
if((data)->sam_response_prompt)
{
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, (data)->sam_response_prompt, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 6, &l);
BACK;
ret += oldret;
}
if((data)->sam_challenge)
{
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, (data)->sam_challenge, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
if((data)->sam_challenge_label)
{
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, (data)->sam_challenge_label, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
if((data)->sam_track_id)
{
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, (data)->sam_track_id, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
if((data)->sam_type_name)
{
int oldret = ret;
ret = 0;
e = encode_general_string(p, len, (data)->sam_type_name, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_SAMFlags(p, len, &(data)->sam_flags, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->sam_type, &l);
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
decode_PA_SAM_CHALLENGE_2_BODY(const unsigned char *p, size_t len, PA_SAM_CHALLENGE_2_BODY *data, size_t *size)
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
e = decode_integer(p, len, &(data)->sam_type, &l);
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
e = decode_SAMFlags(p, len, &(data)->sam_flags, &l);
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
(data)->sam_type_name = NULL;
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
(data)->sam_type_name = malloc(sizeof(*(data)->sam_type_name));
if((data)->sam_type_name == NULL) return ENOMEM;
e = decode_general_string(p, len, (data)->sam_type_name, &l);
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
(data)->sam_track_id = NULL;
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
(data)->sam_track_id = malloc(sizeof(*(data)->sam_track_id));
if((data)->sam_track_id == NULL) return ENOMEM;
e = decode_general_string(p, len, (data)->sam_track_id, &l);
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
(data)->sam_challenge_label = NULL;
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
(data)->sam_challenge_label = malloc(sizeof(*(data)->sam_challenge_label));
if((data)->sam_challenge_label == NULL) return ENOMEM;
e = decode_general_string(p, len, (data)->sam_challenge_label, &l);
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
(data)->sam_challenge = NULL;
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
(data)->sam_challenge = malloc(sizeof(*(data)->sam_challenge));
if((data)->sam_challenge == NULL) return ENOMEM;
e = decode_general_string(p, len, (data)->sam_challenge, &l);
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
(data)->sam_response_prompt = NULL;
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
(data)->sam_response_prompt = malloc(sizeof(*(data)->sam_response_prompt));
if((data)->sam_response_prompt == NULL) return ENOMEM;
e = decode_general_string(p, len, (data)->sam_response_prompt, &l);
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
(data)->sam_pk_for_sad = NULL;
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
(data)->sam_pk_for_sad = malloc(sizeof(*(data)->sam_pk_for_sad));
if((data)->sam_pk_for_sad == NULL) return ENOMEM;
e = decode_EncryptionKey(p, len, (data)->sam_pk_for_sad, &l);
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
e = decode_integer(p, len, &(data)->sam_nonce, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 9, &l);
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
e = decode_integer(p, len, &(data)->sam_etype, &l);
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
free_PA_SAM_CHALLENGE_2_BODY(data);
return e;
}

void
free_PA_SAM_CHALLENGE_2_BODY(PA_SAM_CHALLENGE_2_BODY *data)
{
free_SAMFlags(&(data)->sam_flags);
if((data)->sam_type_name) {
free_general_string((data)->sam_type_name);
free((data)->sam_type_name);
(data)->sam_type_name = NULL;
}
if((data)->sam_track_id) {
free_general_string((data)->sam_track_id);
free((data)->sam_track_id);
(data)->sam_track_id = NULL;
}
if((data)->sam_challenge_label) {
free_general_string((data)->sam_challenge_label);
free((data)->sam_challenge_label);
(data)->sam_challenge_label = NULL;
}
if((data)->sam_challenge) {
free_general_string((data)->sam_challenge);
free((data)->sam_challenge);
(data)->sam_challenge = NULL;
}
if((data)->sam_response_prompt) {
free_general_string((data)->sam_response_prompt);
free((data)->sam_response_prompt);
(data)->sam_response_prompt = NULL;
}
if((data)->sam_pk_for_sad) {
free_EncryptionKey((data)->sam_pk_for_sad);
free((data)->sam_pk_for_sad);
(data)->sam_pk_for_sad = NULL;
}
}

size_t
length_PA_SAM_CHALLENGE_2_BODY(const PA_SAM_CHALLENGE_2_BODY *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->sam_type);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_SAMFlags(&(data)->sam_flags);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sam_type_name){
int oldret = ret;
ret = 0;
ret += length_general_string((data)->sam_type_name);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sam_track_id){
int oldret = ret;
ret = 0;
ret += length_general_string((data)->sam_track_id);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sam_challenge_label){
int oldret = ret;
ret = 0;
ret += length_general_string((data)->sam_challenge_label);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sam_challenge){
int oldret = ret;
ret = 0;
ret += length_general_string((data)->sam_challenge);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sam_response_prompt){
int oldret = ret;
ret = 0;
ret += length_general_string((data)->sam_response_prompt);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sam_pk_for_sad){
int oldret = ret;
ret = 0;
ret += length_EncryptionKey((data)->sam_pk_for_sad);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->sam_nonce);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->sam_etype);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_PA_SAM_CHALLENGE_2_BODY(const PA_SAM_CHALLENGE_2_BODY *from, PA_SAM_CHALLENGE_2_BODY *to)
{
*(&(to)->sam_type) = *(&(from)->sam_type);
if(copy_SAMFlags(&(from)->sam_flags, &(to)->sam_flags)) return ENOMEM;
if((from)->sam_type_name) {
(to)->sam_type_name = malloc(sizeof(*(to)->sam_type_name));
if((to)->sam_type_name == NULL) return ENOMEM;
if(copy_general_string((from)->sam_type_name, (to)->sam_type_name)) return ENOMEM;
}else
(to)->sam_type_name = NULL;
if((from)->sam_track_id) {
(to)->sam_track_id = malloc(sizeof(*(to)->sam_track_id));
if((to)->sam_track_id == NULL) return ENOMEM;
if(copy_general_string((from)->sam_track_id, (to)->sam_track_id)) return ENOMEM;
}else
(to)->sam_track_id = NULL;
if((from)->sam_challenge_label) {
(to)->sam_challenge_label = malloc(sizeof(*(to)->sam_challenge_label));
if((to)->sam_challenge_label == NULL) return ENOMEM;
if(copy_general_string((from)->sam_challenge_label, (to)->sam_challenge_label)) return ENOMEM;
}else
(to)->sam_challenge_label = NULL;
if((from)->sam_challenge) {
(to)->sam_challenge = malloc(sizeof(*(to)->sam_challenge));
if((to)->sam_challenge == NULL) return ENOMEM;
if(copy_general_string((from)->sam_challenge, (to)->sam_challenge)) return ENOMEM;
}else
(to)->sam_challenge = NULL;
if((from)->sam_response_prompt) {
(to)->sam_response_prompt = malloc(sizeof(*(to)->sam_response_prompt));
if((to)->sam_response_prompt == NULL) return ENOMEM;
if(copy_general_string((from)->sam_response_prompt, (to)->sam_response_prompt)) return ENOMEM;
}else
(to)->sam_response_prompt = NULL;
if((from)->sam_pk_for_sad) {
(to)->sam_pk_for_sad = malloc(sizeof(*(to)->sam_pk_for_sad));
if((to)->sam_pk_for_sad == NULL) return ENOMEM;
if(copy_EncryptionKey((from)->sam_pk_for_sad, (to)->sam_pk_for_sad)) return ENOMEM;
}else
(to)->sam_pk_for_sad = NULL;
*(&(to)->sam_nonce) = *(&(from)->sam_nonce);
*(&(to)->sam_etype) = *(&(from)->sam_etype);
return 0;
}

