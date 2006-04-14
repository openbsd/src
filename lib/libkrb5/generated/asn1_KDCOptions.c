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
encode_KDCOptions(unsigned char *p, size_t len, const KDCOptions *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
unsigned char c = 0;
if(data->validate) c |= 1<<0;
if(data->renew) c |= 1<<1;
if(data->enc_tkt_in_skey) c |= 1<<3;
if(data->renewable_ok) c |= 1<<4;
if(data->disable_transited_check) c |= 1<<5;
*p-- = c; len--; ret++;
c = 0;
*p-- = c; len--; ret++;
c = 0;
if(data->canonicalize) c |= 1<<0;
if(data->request_anonymous) c |= 1<<1;
if(data->unused11) c |= 1<<4;
if(data->unused10) c |= 1<<5;
if(data->unused9) c |= 1<<6;
if(data->renewable) c |= 1<<7;
*p-- = c; len--; ret++;
c = 0;
if(data->unused7) c |= 1<<0;
if(data->postdated) c |= 1<<1;
if(data->allow_postdate) c |= 1<<2;
if(data->proxy) c |= 1<<3;
if(data->proxiable) c |= 1<<4;
if(data->forwarded) c |= 1<<5;
if(data->forwardable) c |= 1<<6;
if(data->reserved) c |= 1<<7;
*p-- = c;
*p-- = 0;
len -= 2;
ret += 2;
}

e = der_put_length_and_tag (p, len, ret, ASN1_C_UNIV, PRIM,UT_BitString, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_KDCOptions(const unsigned char *p, size_t len, KDCOptions *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, ASN1_C_UNIV, PRIM, UT_BitString,&reallen, &l);
FORW;
if(len < reallen)
return ASN1_OVERRUN;
p++;
len--;
reallen--;
ret++;
data->reserved = (*p >> 7) & 1;
data->forwardable = (*p >> 6) & 1;
data->forwarded = (*p >> 5) & 1;
data->proxiable = (*p >> 4) & 1;
data->proxy = (*p >> 3) & 1;
data->allow_postdate = (*p >> 2) & 1;
data->postdated = (*p >> 1) & 1;
data->unused7 = (*p >> 0) & 1;
p++; len--; reallen--; ret++;
data->renewable = (*p >> 7) & 1;
data->unused9 = (*p >> 6) & 1;
data->unused10 = (*p >> 5) & 1;
data->unused11 = (*p >> 4) & 1;
data->request_anonymous = (*p >> 1) & 1;
data->canonicalize = (*p >> 0) & 1;
p++; len--; reallen--; ret++;
p++; len--; reallen--; ret++;
data->disable_transited_check = (*p >> 5) & 1;
data->renewable_ok = (*p >> 4) & 1;
data->enc_tkt_in_skey = (*p >> 3) & 1;
data->renew = (*p >> 1) & 1;
data->validate = (*p >> 0) & 1;
p += reallen; len -= reallen; ret += reallen;
if(size) *size = ret;
return 0;
fail:
free_KDCOptions(data);
return e;
}

void
free_KDCOptions(KDCOptions *data)
{
}

size_t
length_KDCOptions(const KDCOptions *data)
{
size_t ret = 0;
ret += 7;
return ret;
}

int
copy_KDCOptions(const KDCOptions *from, KDCOptions *to)
{
*(to) = *(from);
return 0;
}

unsigned KDCOptions2int(KDCOptions f)
{
unsigned r = 0;
if(f.reserved) r |= (1U << 0);
if(f.forwardable) r |= (1U << 1);
if(f.forwarded) r |= (1U << 2);
if(f.proxiable) r |= (1U << 3);
if(f.proxy) r |= (1U << 4);
if(f.allow_postdate) r |= (1U << 5);
if(f.postdated) r |= (1U << 6);
if(f.unused7) r |= (1U << 7);
if(f.renewable) r |= (1U << 8);
if(f.unused9) r |= (1U << 9);
if(f.unused10) r |= (1U << 10);
if(f.unused11) r |= (1U << 11);
if(f.request_anonymous) r |= (1U << 14);
if(f.canonicalize) r |= (1U << 15);
if(f.disable_transited_check) r |= (1U << 26);
if(f.renewable_ok) r |= (1U << 27);
if(f.enc_tkt_in_skey) r |= (1U << 28);
if(f.renew) r |= (1U << 30);
if(f.validate) r |= (1U << 31);
return r;
}

KDCOptions int2KDCOptions(unsigned n)
{
	KDCOptions flags;

	flags.reserved = (n >> 0) & 1;
	flags.forwardable = (n >> 1) & 1;
	flags.forwarded = (n >> 2) & 1;
	flags.proxiable = (n >> 3) & 1;
	flags.proxy = (n >> 4) & 1;
	flags.allow_postdate = (n >> 5) & 1;
	flags.postdated = (n >> 6) & 1;
	flags.unused7 = (n >> 7) & 1;
	flags.renewable = (n >> 8) & 1;
	flags.unused9 = (n >> 9) & 1;
	flags.unused10 = (n >> 10) & 1;
	flags.unused11 = (n >> 11) & 1;
	flags.request_anonymous = (n >> 14) & 1;
	flags.canonicalize = (n >> 15) & 1;
	flags.disable_transited_check = (n >> 26) & 1;
	flags.renewable_ok = (n >> 27) & 1;
	flags.enc_tkt_in_skey = (n >> 28) & 1;
	flags.renew = (n >> 30) & 1;
	flags.validate = (n >> 31) & 1;
	return flags;
}

static struct units KDCOptions_units[] = {
	{"validate",	1U << 31},
	{"renew",	1U << 30},
	{"enc_tkt_in_skey",	1U << 28},
	{"renewable_ok",	1U << 27},
	{"disable_transited_check",	1U << 26},
	{"canonicalize",	1U << 15},
	{"request_anonymous",	1U << 14},
	{"unused11",	1U << 11},
	{"unused10",	1U << 10},
	{"unused9",	1U << 9},
	{"renewable",	1U << 8},
	{"unused7",	1U << 7},
	{"postdated",	1U << 6},
	{"allow_postdate",	1U << 5},
	{"proxy",	1U << 4},
	{"proxiable",	1U << 3},
	{"forwarded",	1U << 2},
	{"forwardable",	1U << 1},
	{"reserved",	1U << 0},
	{NULL,	0}
};

const struct units * asn1_KDCOptions_units(void){
return KDCOptions_units;
}

