/* Generated from /usr/src/lib/libkrb5/../../kerberosV/src/lib/hdb/hdb.asn1 */
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
encode_hdb_entry(unsigned char *p, size_t len, const hdb_entry *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->generation)
{
int oldret = ret;
ret = 0;
e = encode_GENERATION(p, len, (data)->generation, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 12, &l);
BACK;
ret += oldret;
}
if((data)->etypes)
{
int oldret = ret;
ret = 0;
for(i = ((data)->etypes)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &((data)->etypes)->val[i], &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 11, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_HDBFlags(p, len, &(data)->flags, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 10, &l);
BACK;
ret += oldret;
}
if((data)->max_renew)
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, (data)->max_renew, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 9, &l);
BACK;
ret += oldret;
}
if((data)->max_life)
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, (data)->max_life, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 8, &l);
BACK;
ret += oldret;
}
if((data)->pw_end)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->pw_end, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 7, &l);
BACK;
ret += oldret;
}
if((data)->valid_end)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->valid_end, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 6, &l);
BACK;
ret += oldret;
}
if((data)->valid_start)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->valid_start, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
if((data)->modified_by)
{
int oldret = ret;
ret = 0;
e = encode_Event(p, len, (data)->modified_by, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_Event(p, len, &(data)->created_by, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
for(i = (&(data)->keys)->len - 1; i >= 0; --i) {
int oldret = ret;
ret = 0;
e = encode_Key(p, len, &(&(data)->keys)->val[i], &l);
BACK;
ret += oldret;
}
e = der_put_length_and_tag (p, len, ret, UNIV, CONS, UT_Sequence, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_integer(p, len, &(data)->kvno, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
if((data)->principal)
{
int oldret = ret;
ret = 0;
e = encode_Principal(p, len, (data)->principal, &l);
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
decode_hdb_entry(const unsigned char *p, size_t len, hdb_entry *data, size_t *size)
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
(data)->principal = NULL;
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
(data)->principal = malloc(sizeof(*(data)->principal));
if((data)->principal == NULL) return ENOMEM;
e = decode_Principal(p, len, (data)->principal, &l);
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
e = decode_integer(p, len, &(data)->kvno, &l);
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
e = der_match_tag_and_length (p, len, UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
if(len < reallen)
return ASN1_OVERRUN;
len = reallen;
{
size_t origlen = len;
int oldret = ret;
ret = 0;
(&(data)->keys)->len = 0;
(&(data)->keys)->val = NULL;
while(ret < origlen) {
(&(data)->keys)->len++;
(&(data)->keys)->val = realloc((&(data)->keys)->val, sizeof(*((&(data)->keys)->val)) * (&(data)->keys)->len);
e = decode_Key(p, len, &(&(data)->keys)->val[(&(data)->keys)->len-1], &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 3, &l);
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
e = decode_Event(p, len, &(data)->created_by, &l);
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
(data)->modified_by = NULL;
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
(data)->modified_by = malloc(sizeof(*(data)->modified_by));
if((data)->modified_by == NULL) return ENOMEM;
e = decode_Event(p, len, (data)->modified_by, &l);
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
(data)->valid_start = NULL;
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
(data)->valid_start = malloc(sizeof(*(data)->valid_start));
if((data)->valid_start == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->valid_start, &l);
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
(data)->valid_end = NULL;
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
(data)->valid_end = malloc(sizeof(*(data)->valid_end));
if((data)->valid_end == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->valid_end, &l);
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
(data)->pw_end = NULL;
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
(data)->pw_end = malloc(sizeof(*(data)->pw_end));
if((data)->pw_end == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->pw_end, &l);
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
(data)->max_life = NULL;
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
(data)->max_life = malloc(sizeof(*(data)->max_life));
if((data)->max_life == NULL) return ENOMEM;
e = decode_integer(p, len, (data)->max_life, &l);
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
(data)->max_renew = NULL;
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
(data)->max_renew = malloc(sizeof(*(data)->max_renew));
if((data)->max_renew == NULL) return ENOMEM;
e = decode_integer(p, len, (data)->max_renew, &l);
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
e = decode_HDBFlags(p, len, &(data)->flags, &l);
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
(data)->etypes = NULL;
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
(data)->etypes = malloc(sizeof(*(data)->etypes));
if((data)->etypes == NULL) return ENOMEM;
e = der_match_tag_and_length (p, len, UNIV, CONS, UT_Sequence,&reallen, &l);
FORW;
if(len < reallen)
return ASN1_OVERRUN;
len = reallen;
{
size_t origlen = len;
int oldret = ret;
ret = 0;
((data)->etypes)->len = 0;
((data)->etypes)->val = NULL;
while(ret < origlen) {
((data)->etypes)->len++;
((data)->etypes)->val = realloc(((data)->etypes)->val, sizeof(*(((data)->etypes)->val)) * ((data)->etypes)->len);
e = decode_integer(p, len, &((data)->etypes)->val[((data)->etypes)->len-1], &l);
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

e = der_match_tag (p, len, CONTEXT, CONS, 12, &l);
if (e)
(data)->generation = NULL;
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
(data)->generation = malloc(sizeof(*(data)->generation));
if((data)->generation == NULL) return ENOMEM;
e = decode_GENERATION(p, len, (data)->generation, &l);
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
free_hdb_entry(data);
return e;
}

void
free_hdb_entry(hdb_entry *data)
{
if((data)->principal) {
free_Principal((data)->principal);
free((data)->principal);
}
while((&(data)->keys)->len){
free_Key(&(&(data)->keys)->val[(&(data)->keys)->len-1]);
(&(data)->keys)->len--;
}
free((&(data)->keys)->val);
free_Event(&(data)->created_by);
if((data)->modified_by) {
free_Event((data)->modified_by);
free((data)->modified_by);
}
if((data)->valid_start) {
free_KerberosTime((data)->valid_start);
free((data)->valid_start);
}
if((data)->valid_end) {
free_KerberosTime((data)->valid_end);
free((data)->valid_end);
}
if((data)->pw_end) {
free_KerberosTime((data)->pw_end);
free((data)->pw_end);
}
if((data)->max_life) {
free((data)->max_life);
}
if((data)->max_renew) {
free((data)->max_renew);
}
free_HDBFlags(&(data)->flags);
if((data)->etypes) {
while(((data)->etypes)->len){
((data)->etypes)->len--;
}
free(((data)->etypes)->val);
free((data)->etypes);
}
if((data)->generation) {
free_GENERATION((data)->generation);
free((data)->generation);
}
}

size_t
length_hdb_entry(const hdb_entry *data)
{
size_t ret = 0;
if((data)->principal){
int oldret = ret;
ret = 0;
ret += length_Principal((data)->principal);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_integer(&(data)->kvno);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = (&(data)->keys)->len - 1; i >= 0; --i){
ret += length_Key(&(&(data)->keys)->val[i]);
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_Event(&(data)->created_by);
ret += 1 + length_len(ret) + oldret;
}
if((data)->modified_by){
int oldret = ret;
ret = 0;
ret += length_Event((data)->modified_by);
ret += 1 + length_len(ret) + oldret;
}
if((data)->valid_start){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->valid_start);
ret += 1 + length_len(ret) + oldret;
}
if((data)->valid_end){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->valid_end);
ret += 1 + length_len(ret) + oldret;
}
if((data)->pw_end){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->pw_end);
ret += 1 + length_len(ret) + oldret;
}
if((data)->max_life){
int oldret = ret;
ret = 0;
ret += length_integer((data)->max_life);
ret += 1 + length_len(ret) + oldret;
}
if((data)->max_renew){
int oldret = ret;
ret = 0;
ret += length_integer((data)->max_renew);
ret += 1 + length_len(ret) + oldret;
}
{
int oldret = ret;
ret = 0;
ret += length_HDBFlags(&(data)->flags);
ret += 1 + length_len(ret) + oldret;
}
if((data)->etypes){
int oldret = ret;
ret = 0;
{
int oldret = ret;
int i;
ret = 0;
for(i = ((data)->etypes)->len - 1; i >= 0; --i){
ret += length_integer(&((data)->etypes)->val[i]);
}
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret) + oldret;
}
if((data)->generation){
int oldret = ret;
ret = 0;
ret += length_GENERATION((data)->generation);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_hdb_entry(const hdb_entry *from, hdb_entry *to)
{
if((from)->principal) {
(to)->principal = malloc(sizeof(*(to)->principal));
if((to)->principal == NULL) return ENOMEM;
if(copy_Principal((from)->principal, (to)->principal)) return ENOMEM;
}else
(to)->principal = NULL;
*(&(to)->kvno) = *(&(from)->kvno);
if(((&(to)->keys)->val = malloc((&(from)->keys)->len * sizeof(*(&(to)->keys)->val))) == NULL && (&(from)->keys)->len != 0)
return ENOMEM;
for((&(to)->keys)->len = 0; (&(to)->keys)->len < (&(from)->keys)->len; (&(to)->keys)->len++){
if(copy_Key(&(&(from)->keys)->val[(&(to)->keys)->len], &(&(to)->keys)->val[(&(to)->keys)->len])) return ENOMEM;
}
if(copy_Event(&(from)->created_by, &(to)->created_by)) return ENOMEM;
if((from)->modified_by) {
(to)->modified_by = malloc(sizeof(*(to)->modified_by));
if((to)->modified_by == NULL) return ENOMEM;
if(copy_Event((from)->modified_by, (to)->modified_by)) return ENOMEM;
}else
(to)->modified_by = NULL;
if((from)->valid_start) {
(to)->valid_start = malloc(sizeof(*(to)->valid_start));
if((to)->valid_start == NULL) return ENOMEM;
if(copy_KerberosTime((from)->valid_start, (to)->valid_start)) return ENOMEM;
}else
(to)->valid_start = NULL;
if((from)->valid_end) {
(to)->valid_end = malloc(sizeof(*(to)->valid_end));
if((to)->valid_end == NULL) return ENOMEM;
if(copy_KerberosTime((from)->valid_end, (to)->valid_end)) return ENOMEM;
}else
(to)->valid_end = NULL;
if((from)->pw_end) {
(to)->pw_end = malloc(sizeof(*(to)->pw_end));
if((to)->pw_end == NULL) return ENOMEM;
if(copy_KerberosTime((from)->pw_end, (to)->pw_end)) return ENOMEM;
}else
(to)->pw_end = NULL;
if((from)->max_life) {
(to)->max_life = malloc(sizeof(*(to)->max_life));
if((to)->max_life == NULL) return ENOMEM;
*((to)->max_life) = *((from)->max_life);
}else
(to)->max_life = NULL;
if((from)->max_renew) {
(to)->max_renew = malloc(sizeof(*(to)->max_renew));
if((to)->max_renew == NULL) return ENOMEM;
*((to)->max_renew) = *((from)->max_renew);
}else
(to)->max_renew = NULL;
if(copy_HDBFlags(&(from)->flags, &(to)->flags)) return ENOMEM;
if((from)->etypes) {
(to)->etypes = malloc(sizeof(*(to)->etypes));
if((to)->etypes == NULL) return ENOMEM;
if((((to)->etypes)->val = malloc(((from)->etypes)->len * sizeof(*((to)->etypes)->val))) == NULL && ((from)->etypes)->len != 0)
return ENOMEM;
for(((to)->etypes)->len = 0; ((to)->etypes)->len < ((from)->etypes)->len; ((to)->etypes)->len++){
*(&((to)->etypes)->val[((to)->etypes)->len]) = *(&((from)->etypes)->val[((to)->etypes)->len]);
}
}else
(to)->etypes = NULL;
if((from)->generation) {
(to)->generation = malloc(sizeof(*(to)->generation));
if((to)->generation == NULL) return ENOMEM;
if(copy_GENERATION((from)->generation, (to)->generation)) return ENOMEM;
}else
(to)->generation = NULL;
return 0;
}

