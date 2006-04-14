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
encode_CBCParameter(unsigned char *p, size_t len, const CBCParameter *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
e = encode_octet_string(p, len, data, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_CBCParameter(const unsigned char *p, size_t len, CBCParameter *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = decode_octet_string(p, len, data, &l);
FORW;
if(size) *size = ret;
return 0;
fail:
free_CBCParameter(data);
return e;
}

void
free_CBCParameter(CBCParameter *data)
{
free_octet_string(data);
}

size_t
length_CBCParameter(const CBCParameter *data)
{
size_t ret = 0;
ret += length_octet_string(data);
return ret;
}

int
copy_CBCParameter(const CBCParameter *from, CBCParameter *to)
{
if(copy_octet_string(from, to)) return ENOMEM;
return 0;
}

