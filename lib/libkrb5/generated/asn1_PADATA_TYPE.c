/* Generated from /usr/src/lib/libkrb5/../../kerberosV/src/lib/asn1/k5.asn1 */
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
encode_PADATA_TYPE(unsigned char *p, size_t len, const PADATA_TYPE *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
e = encode_integer(p, len, (const int*)data, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_PADATA_TYPE(const unsigned char *p, size_t len, PADATA_TYPE *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = decode_integer(p, len, (int*)data, &l);
FORW;
if(size) *size = ret;
return 0;
fail:
free_PADATA_TYPE(data);
return e;
}

void
free_PADATA_TYPE(PADATA_TYPE *data)
{
}

size_t
length_PADATA_TYPE(const PADATA_TYPE *data)
{
size_t ret = 0;
ret += length_integer((const int*)data);
return ret;
}

int
copy_PADATA_TYPE(const PADATA_TYPE *from, PADATA_TYPE *to)
{
*(to) = *(from);
return 0;
}

