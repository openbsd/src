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
encode_TicketFlags(unsigned char *p, size_t len, const TicketFlags *data, size_t *size)
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
if(data->anonymous) c |= 1<<1;
if(data->ok_as_delegate) c |= 1<<2;
if(data->transited_policy_checked) c |= 1<<3;
if(data->hw_authent) c |= 1<<4;
if(data->pre_authent) c |= 1<<5;
if(data->initial) c |= 1<<6;
if(data->renewable) c |= 1<<7;
*p-- = c; len--; ret++;
c = 0;
if(data->invalid) c |= 1<<0;
if(data->postdated) c |= 1<<1;
if(data->may_postdate) c |= 1<<2;
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

e = der_put_length_and_tag (p, len, ret, UNIV, PRIM,UT_BitString, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_TicketFlags(const unsigned char *p, size_t len, TicketFlags *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, UNIV, PRIM, UT_BitString,&reallen, &l);
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
data->may_postdate = (*p >> 2) & 1;
data->postdated = (*p >> 1) & 1;
data->invalid = (*p >> 0) & 1;
p++; len--; reallen--; ret++;
data->renewable = (*p >> 7) & 1;
data->initial = (*p >> 6) & 1;
data->pre_authent = (*p >> 5) & 1;
data->hw_authent = (*p >> 4) & 1;
data->transited_policy_checked = (*p >> 3) & 1;
data->ok_as_delegate = (*p >> 2) & 1;
data->anonymous = (*p >> 1) & 1;
p += reallen; len -= reallen; ret += reallen;
if(size) *size = ret;
return 0;
fail:
free_TicketFlags(data);
return e;
}

void
free_TicketFlags(TicketFlags *data)
{
}

size_t
length_TicketFlags(const TicketFlags *data)
{
size_t ret = 0;
ret += 7;
return ret;
}

int
copy_TicketFlags(const TicketFlags *from, TicketFlags *to)
{
*(to) = *(from);
return 0;
}

unsigned TicketFlags2int(TicketFlags f)
{
unsigned r = 0;
if(f.reserved) r |= (1U << 0);
if(f.forwardable) r |= (1U << 1);
if(f.forwarded) r |= (1U << 2);
if(f.proxiable) r |= (1U << 3);
if(f.proxy) r |= (1U << 4);
if(f.may_postdate) r |= (1U << 5);
if(f.postdated) r |= (1U << 6);
if(f.invalid) r |= (1U << 7);
if(f.renewable) r |= (1U << 8);
if(f.initial) r |= (1U << 9);
if(f.pre_authent) r |= (1U << 10);
if(f.hw_authent) r |= (1U << 11);
if(f.transited_policy_checked) r |= (1U << 12);
if(f.ok_as_delegate) r |= (1U << 13);
if(f.anonymous) r |= (1U << 14);
return r;
}

TicketFlags int2TicketFlags(unsigned n)
{
	TicketFlags flags;

	flags.reserved = (n >> 0) & 1;
	flags.forwardable = (n >> 1) & 1;
	flags.forwarded = (n >> 2) & 1;
	flags.proxiable = (n >> 3) & 1;
	flags.proxy = (n >> 4) & 1;
	flags.may_postdate = (n >> 5) & 1;
	flags.postdated = (n >> 6) & 1;
	flags.invalid = (n >> 7) & 1;
	flags.renewable = (n >> 8) & 1;
	flags.initial = (n >> 9) & 1;
	flags.pre_authent = (n >> 10) & 1;
	flags.hw_authent = (n >> 11) & 1;
	flags.transited_policy_checked = (n >> 12) & 1;
	flags.ok_as_delegate = (n >> 13) & 1;
	flags.anonymous = (n >> 14) & 1;
	return flags;
}

struct units TicketFlags_units[] = {
	{"anonymous",	1U << 14},
	{"ok_as_delegate",	1U << 13},
	{"transited_policy_checked",	1U << 12},
	{"hw_authent",	1U << 11},
	{"pre_authent",	1U << 10},
	{"initial",	1U << 9},
	{"renewable",	1U << 8},
	{"invalid",	1U << 7},
	{"postdated",	1U << 6},
	{"may_postdate",	1U << 5},
	{"proxy",	1U << 4},
	{"proxiable",	1U << 3},
	{"forwarded",	1U << 2},
	{"forwardable",	1U << 1},
	{"reserved",	1U << 0},
	{NULL,	0}
};

