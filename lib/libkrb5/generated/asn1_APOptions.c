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
encode_APOptions(unsigned char *p, size_t len, const APOptions *data, size_t *size)
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
if(data->mutual_required) c |= 1<<5;
if(data->use_session_key) c |= 1<<6;
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
decode_APOptions(const unsigned char *p, size_t len, APOptions *data, size_t *size)
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
data->use_session_key = (*p >> 6) & 1;
data->mutual_required = (*p >> 5) & 1;
p += reallen; len -= reallen; ret += reallen;
if(size) *size = ret;
return 0;
fail:
free_APOptions(data);
return e;
}

void
free_APOptions(APOptions *data)
{
}

size_t
length_APOptions(const APOptions *data)
{
size_t ret = 0;
ret += 7;
return ret;
}

int
copy_APOptions(const APOptions *from, APOptions *to)
{
*(to) = *(from);
return 0;
}

unsigned APOptions2int(APOptions f)
{
unsigned r = 0;
if(f.reserved) r |= (1U << 0);
if(f.use_session_key) r |= (1U << 1);
if(f.mutual_required) r |= (1U << 2);
return r;
}

APOptions int2APOptions(unsigned n)
{
	APOptions flags;

	flags.reserved = (n >> 0) & 1;
	flags.use_session_key = (n >> 1) & 1;
	flags.mutual_required = (n >> 2) & 1;
	return flags;
}

static struct units APOptions_units[] = {
	{"mutual_required",	1U << 2},
	{"use_session_key",	1U << 1},
	{"reserved",	1U << 0},
	{NULL,	0}
};

const struct units * asn1_APOptions_units(void){
return APOptions_units;
}

