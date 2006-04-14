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
encode_ContextFlags(unsigned char *p, size_t len, const ContextFlags *data, size_t *size)
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
*p-- = c; len--; ret++;
c = 0;
if(data->integFlag) c |= 1<<1;
if(data->confFlag) c |= 1<<2;
if(data->anonFlag) c |= 1<<3;
if(data->sequenceFlag) c |= 1<<4;
if(data->replayFlag) c |= 1<<5;
if(data->mutualFlag) c |= 1<<6;
if(data->delegFlag) c |= 1<<7;
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
decode_ContextFlags(const unsigned char *p, size_t len, ContextFlags *data, size_t *size)
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
data->delegFlag = (*p >> 7) & 1;
data->mutualFlag = (*p >> 6) & 1;
data->replayFlag = (*p >> 5) & 1;
data->sequenceFlag = (*p >> 4) & 1;
data->anonFlag = (*p >> 3) & 1;
data->confFlag = (*p >> 2) & 1;
data->integFlag = (*p >> 1) & 1;
p += reallen; len -= reallen; ret += reallen;
if(size) *size = ret;
return 0;
fail:
free_ContextFlags(data);
return e;
}

void
free_ContextFlags(ContextFlags *data)
{
}

size_t
length_ContextFlags(const ContextFlags *data)
{
size_t ret = 0;
ret += 7;
return ret;
}

int
copy_ContextFlags(const ContextFlags *from, ContextFlags *to)
{
*(to) = *(from);
return 0;
}

unsigned ContextFlags2int(ContextFlags f)
{
unsigned r = 0;
if(f.delegFlag) r |= (1U << 0);
if(f.mutualFlag) r |= (1U << 1);
if(f.replayFlag) r |= (1U << 2);
if(f.sequenceFlag) r |= (1U << 3);
if(f.anonFlag) r |= (1U << 4);
if(f.confFlag) r |= (1U << 5);
if(f.integFlag) r |= (1U << 6);
return r;
}

ContextFlags int2ContextFlags(unsigned n)
{
	ContextFlags flags;

	flags.delegFlag = (n >> 0) & 1;
	flags.mutualFlag = (n >> 1) & 1;
	flags.replayFlag = (n >> 2) & 1;
	flags.sequenceFlag = (n >> 3) & 1;
	flags.anonFlag = (n >> 4) & 1;
	flags.confFlag = (n >> 5) & 1;
	flags.integFlag = (n >> 6) & 1;
	return flags;
}

static struct units ContextFlags_units[] = {
	{"integFlag",	1U << 6},
	{"confFlag",	1U << 5},
	{"anonFlag",	1U << 4},
	{"sequenceFlag",	1U << 3},
	{"replayFlag",	1U << 2},
	{"mutualFlag",	1U << 1},
	{"delegFlag",	1U << 0},
	{NULL,	0}
};

const struct units * asn1_ContextFlags_units(void){
return ContextFlags_units;
}

