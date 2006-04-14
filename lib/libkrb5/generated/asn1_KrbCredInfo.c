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
encode_KrbCredInfo(unsigned char *p, size_t len, const KrbCredInfo *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
if((data)->caddr)
{
int oldret = ret;
ret = 0;
e = encode_HostAddresses(p, len, (data)->caddr, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 10, &l);
BACK;
ret += oldret;
}
if((data)->sname)
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, (data)->sname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 9, &l);
BACK;
ret += oldret;
}
if((data)->srealm)
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, (data)->srealm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 8, &l);
BACK;
ret += oldret;
}
if((data)->renew_till)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->renew_till, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 7, &l);
BACK;
ret += oldret;
}
if((data)->endtime)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->endtime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 6, &l);
BACK;
ret += oldret;
}
if((data)->starttime)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->starttime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 5, &l);
BACK;
ret += oldret;
}
if((data)->authtime)
{
int oldret = ret;
ret = 0;
e = encode_KerberosTime(p, len, (data)->authtime, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 4, &l);
BACK;
ret += oldret;
}
if((data)->flags)
{
int oldret = ret;
ret = 0;
e = encode_TicketFlags(p, len, (data)->flags, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
BACK;
ret += oldret;
}
if((data)->pname)
{
int oldret = ret;
ret = 0;
e = encode_PrincipalName(p, len, (data)->pname, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
BACK;
ret += oldret;
}
if((data)->prealm)
{
int oldret = ret;
ret = 0;
e = encode_Realm(p, len, (data)->prealm, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
BACK;
ret += oldret;
}
{
int oldret = ret;
ret = 0;
e = encode_EncryptionKey(p, len, &(data)->key, &l);
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
decode_KrbCredInfo(const unsigned char *p, size_t len, KrbCredInfo *data, size_t *size)
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 1, &l);
if (e)
(data)->prealm = NULL;
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
(data)->prealm = malloc(sizeof(*(data)->prealm));
if((data)->prealm == NULL) return ENOMEM;
e = decode_Realm(p, len, (data)->prealm, &l);
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
(data)->pname = NULL;
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
(data)->pname = malloc(sizeof(*(data)->pname));
if((data)->pname == NULL) return ENOMEM;
e = decode_PrincipalName(p, len, (data)->pname, &l);
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
(data)->flags = NULL;
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
(data)->flags = malloc(sizeof(*(data)->flags));
if((data)->flags == NULL) return ENOMEM;
e = decode_TicketFlags(p, len, (data)->flags, &l);
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
(data)->authtime = NULL;
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
(data)->authtime = malloc(sizeof(*(data)->authtime));
if((data)->authtime == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->authtime, &l);
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
(data)->starttime = NULL;
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
(data)->starttime = malloc(sizeof(*(data)->starttime));
if((data)->starttime == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->starttime, &l);
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
(data)->endtime = NULL;
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
(data)->endtime = malloc(sizeof(*(data)->endtime));
if((data)->endtime == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->endtime, &l);
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
(data)->renew_till = NULL;
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
(data)->renew_till = malloc(sizeof(*(data)->renew_till));
if((data)->renew_till == NULL) return ENOMEM;
e = decode_KerberosTime(p, len, (data)->renew_till, &l);
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
(data)->srealm = NULL;
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
(data)->srealm = malloc(sizeof(*(data)->srealm));
if((data)->srealm == NULL) return ENOMEM;
e = decode_Realm(p, len, (data)->srealm, &l);
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

e = der_match_tag (p, len, ASN1_C_CONTEXT, CONS, 10, &l);
if (e)
(data)->caddr = NULL;
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
(data)->caddr = malloc(sizeof(*(data)->caddr));
if((data)->caddr == NULL) return ENOMEM;
e = decode_HostAddresses(p, len, (data)->caddr, &l);
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
free_KrbCredInfo(data);
return e;
}

void
free_KrbCredInfo(KrbCredInfo *data)
{
free_EncryptionKey(&(data)->key);
if((data)->prealm) {
free_Realm((data)->prealm);
free((data)->prealm);
(data)->prealm = NULL;
}
if((data)->pname) {
free_PrincipalName((data)->pname);
free((data)->pname);
(data)->pname = NULL;
}
if((data)->flags) {
free_TicketFlags((data)->flags);
free((data)->flags);
(data)->flags = NULL;
}
if((data)->authtime) {
free_KerberosTime((data)->authtime);
free((data)->authtime);
(data)->authtime = NULL;
}
if((data)->starttime) {
free_KerberosTime((data)->starttime);
free((data)->starttime);
(data)->starttime = NULL;
}
if((data)->endtime) {
free_KerberosTime((data)->endtime);
free((data)->endtime);
(data)->endtime = NULL;
}
if((data)->renew_till) {
free_KerberosTime((data)->renew_till);
free((data)->renew_till);
(data)->renew_till = NULL;
}
if((data)->srealm) {
free_Realm((data)->srealm);
free((data)->srealm);
(data)->srealm = NULL;
}
if((data)->sname) {
free_PrincipalName((data)->sname);
free((data)->sname);
(data)->sname = NULL;
}
if((data)->caddr) {
free_HostAddresses((data)->caddr);
free((data)->caddr);
(data)->caddr = NULL;
}
}

size_t
length_KrbCredInfo(const KrbCredInfo *data)
{
size_t ret = 0;
{
int oldret = ret;
ret = 0;
ret += length_EncryptionKey(&(data)->key);
ret += 1 + length_len(ret) + oldret;
}
if((data)->prealm){
int oldret = ret;
ret = 0;
ret += length_Realm((data)->prealm);
ret += 1 + length_len(ret) + oldret;
}
if((data)->pname){
int oldret = ret;
ret = 0;
ret += length_PrincipalName((data)->pname);
ret += 1 + length_len(ret) + oldret;
}
if((data)->flags){
int oldret = ret;
ret = 0;
ret += length_TicketFlags((data)->flags);
ret += 1 + length_len(ret) + oldret;
}
if((data)->authtime){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->authtime);
ret += 1 + length_len(ret) + oldret;
}
if((data)->starttime){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->starttime);
ret += 1 + length_len(ret) + oldret;
}
if((data)->endtime){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->endtime);
ret += 1 + length_len(ret) + oldret;
}
if((data)->renew_till){
int oldret = ret;
ret = 0;
ret += length_KerberosTime((data)->renew_till);
ret += 1 + length_len(ret) + oldret;
}
if((data)->srealm){
int oldret = ret;
ret = 0;
ret += length_Realm((data)->srealm);
ret += 1 + length_len(ret) + oldret;
}
if((data)->sname){
int oldret = ret;
ret = 0;
ret += length_PrincipalName((data)->sname);
ret += 1 + length_len(ret) + oldret;
}
if((data)->caddr){
int oldret = ret;
ret = 0;
ret += length_HostAddresses((data)->caddr);
ret += 1 + length_len(ret) + oldret;
}
ret += 1 + length_len(ret);
return ret;
}

int
copy_KrbCredInfo(const KrbCredInfo *from, KrbCredInfo *to)
{
if(copy_EncryptionKey(&(from)->key, &(to)->key)) return ENOMEM;
if((from)->prealm) {
(to)->prealm = malloc(sizeof(*(to)->prealm));
if((to)->prealm == NULL) return ENOMEM;
if(copy_Realm((from)->prealm, (to)->prealm)) return ENOMEM;
}else
(to)->prealm = NULL;
if((from)->pname) {
(to)->pname = malloc(sizeof(*(to)->pname));
if((to)->pname == NULL) return ENOMEM;
if(copy_PrincipalName((from)->pname, (to)->pname)) return ENOMEM;
}else
(to)->pname = NULL;
if((from)->flags) {
(to)->flags = malloc(sizeof(*(to)->flags));
if((to)->flags == NULL) return ENOMEM;
if(copy_TicketFlags((from)->flags, (to)->flags)) return ENOMEM;
}else
(to)->flags = NULL;
if((from)->authtime) {
(to)->authtime = malloc(sizeof(*(to)->authtime));
if((to)->authtime == NULL) return ENOMEM;
if(copy_KerberosTime((from)->authtime, (to)->authtime)) return ENOMEM;
}else
(to)->authtime = NULL;
if((from)->starttime) {
(to)->starttime = malloc(sizeof(*(to)->starttime));
if((to)->starttime == NULL) return ENOMEM;
if(copy_KerberosTime((from)->starttime, (to)->starttime)) return ENOMEM;
}else
(to)->starttime = NULL;
if((from)->endtime) {
(to)->endtime = malloc(sizeof(*(to)->endtime));
if((to)->endtime == NULL) return ENOMEM;
if(copy_KerberosTime((from)->endtime, (to)->endtime)) return ENOMEM;
}else
(to)->endtime = NULL;
if((from)->renew_till) {
(to)->renew_till = malloc(sizeof(*(to)->renew_till));
if((to)->renew_till == NULL) return ENOMEM;
if(copy_KerberosTime((from)->renew_till, (to)->renew_till)) return ENOMEM;
}else
(to)->renew_till = NULL;
if((from)->srealm) {
(to)->srealm = malloc(sizeof(*(to)->srealm));
if((to)->srealm == NULL) return ENOMEM;
if(copy_Realm((from)->srealm, (to)->srealm)) return ENOMEM;
}else
(to)->srealm = NULL;
if((from)->sname) {
(to)->sname = malloc(sizeof(*(to)->sname));
if((to)->sname == NULL) return ENOMEM;
if(copy_PrincipalName((from)->sname, (to)->sname)) return ENOMEM;
}else
(to)->sname = NULL;
if((from)->caddr) {
(to)->caddr = malloc(sizeof(*(to)->caddr));
if((to)->caddr == NULL) return ENOMEM;
if(copy_HostAddresses((from)->caddr, (to)->caddr)) return ENOMEM;
}else
(to)->caddr = NULL;
return 0;
}

