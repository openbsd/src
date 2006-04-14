/* Generated from /home/biorn/src/lib/libkrb5/../../kerberosV/src/lib/hdb/hdb.asn1 */
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
encode_HDBFlags(unsigned char *p, size_t len, const HDBFlags *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
{
unsigned char c = 0;
*p-- = c; len--; ret++;
c = 0;
*p-- = c; len--; ret++;
c = 0;
if(data->immutable) c |= 1<<2;
if(data->user_to_user) c |= 1<<3;
if(data->ok_as_delegate) c |= 1<<4;
if(data->require_hwauth) c |= 1<<5;
if(data->change_pw) c |= 1<<6;
if(data->require_preauth) c |= 1<<7;
*p-- = c; len--; ret++;
c = 0;
if(data->invalid) c |= 1<<0;
if(data->client) c |= 1<<1;
if(data->server) c |= 1<<2;
if(data->postdate) c |= 1<<3;
if(data->renewable) c |= 1<<4;
if(data->proxiable) c |= 1<<5;
if(data->forwardable) c |= 1<<6;
if(data->initial) c |= 1<<7;
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
decode_HDBFlags(const unsigned char *p, size_t len, HDBFlags *data, size_t *size)
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
data->initial = (*p >> 7) & 1;
data->forwardable = (*p >> 6) & 1;
data->proxiable = (*p >> 5) & 1;
data->renewable = (*p >> 4) & 1;
data->postdate = (*p >> 3) & 1;
data->server = (*p >> 2) & 1;
data->client = (*p >> 1) & 1;
data->invalid = (*p >> 0) & 1;
p++; len--; reallen--; ret++;
data->require_preauth = (*p >> 7) & 1;
data->change_pw = (*p >> 6) & 1;
data->require_hwauth = (*p >> 5) & 1;
data->ok_as_delegate = (*p >> 4) & 1;
data->user_to_user = (*p >> 3) & 1;
data->immutable = (*p >> 2) & 1;
p += reallen; len -= reallen; ret += reallen;
if(size) *size = ret;
return 0;
fail:
free_HDBFlags(data);
return e;
}

void
free_HDBFlags(HDBFlags *data)
{
}

size_t
length_HDBFlags(const HDBFlags *data)
{
size_t ret = 0;
ret += 7;
return ret;
}

int
copy_HDBFlags(const HDBFlags *from, HDBFlags *to)
{
*(to) = *(from);
return 0;
}

unsigned HDBFlags2int(HDBFlags f)
{
unsigned r = 0;
if(f.initial) r |= (1U << 0);
if(f.forwardable) r |= (1U << 1);
if(f.proxiable) r |= (1U << 2);
if(f.renewable) r |= (1U << 3);
if(f.postdate) r |= (1U << 4);
if(f.server) r |= (1U << 5);
if(f.client) r |= (1U << 6);
if(f.invalid) r |= (1U << 7);
if(f.require_preauth) r |= (1U << 8);
if(f.change_pw) r |= (1U << 9);
if(f.require_hwauth) r |= (1U << 10);
if(f.ok_as_delegate) r |= (1U << 11);
if(f.user_to_user) r |= (1U << 12);
if(f.immutable) r |= (1U << 13);
return r;
}

HDBFlags int2HDBFlags(unsigned n)
{
	HDBFlags flags;

	flags.initial = (n >> 0) & 1;
	flags.forwardable = (n >> 1) & 1;
	flags.proxiable = (n >> 2) & 1;
	flags.renewable = (n >> 3) & 1;
	flags.postdate = (n >> 4) & 1;
	flags.server = (n >> 5) & 1;
	flags.client = (n >> 6) & 1;
	flags.invalid = (n >> 7) & 1;
	flags.require_preauth = (n >> 8) & 1;
	flags.change_pw = (n >> 9) & 1;
	flags.require_hwauth = (n >> 10) & 1;
	flags.ok_as_delegate = (n >> 11) & 1;
	flags.user_to_user = (n >> 12) & 1;
	flags.immutable = (n >> 13) & 1;
	return flags;
}

static struct units HDBFlags_units[] = {
	{"immutable",	1U << 13},
	{"user_to_user",	1U << 12},
	{"ok_as_delegate",	1U << 11},
	{"require_hwauth",	1U << 10},
	{"change_pw",	1U << 9},
	{"require_preauth",	1U << 8},
	{"invalid",	1U << 7},
	{"client",	1U << 6},
	{"server",	1U << 5},
	{"postdate",	1U << 4},
	{"renewable",	1U << 3},
	{"proxiable",	1U << 2},
	{"forwardable",	1U << 1},
	{"initial",	1U << 0},
	{NULL,	0}
};

const struct units * asn1_HDBFlags_units(void){
return HDBFlags_units;
}

