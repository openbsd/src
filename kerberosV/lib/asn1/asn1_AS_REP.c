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
encode_AS_REP(unsigned char *p, size_t len, const AS_REP *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
e = encode_KDC_REP(p, len, data, &l);
BACK;
e = der_put_length_and_tag (p, len, ret, APPL, CONS, 11, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_AS_REP(const unsigned char *p, size_t len, AS_REP *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = der_match_tag_and_length (p, len, APPL, CONS, 11, &reallen, &l);
FORW;
{
int dce_fix;
if((dce_fix = fix_dce(reallen, &len)) < 0)
return ASN1_BAD_FORMAT;
e = decode_KDC_REP(p, len, data, &l);
FORW;
if(dce_fix){
e = der_match_tag_and_length (p, len, (Der_class)0, (Der_type)0, 0, &reallen, &l);
FORW;
}
}
if(size) *size = ret;
return 0;
fail:
free_AS_REP(data);
return e;
}

void
free_AS_REP(AS_REP *data)
{
free_KDC_REP(data);
}

size_t
length_AS_REP(const AS_REP *data)
{
size_t ret = 0;
ret += length_KDC_REP(data);
ret += 1 + length_len (ret);
return ret;
}

int
copy_AS_REP(const AS_REP *from, AS_REP *to)
{
if(copy_KDC_REP(from, to)) return ENOMEM;
return 0;
}

