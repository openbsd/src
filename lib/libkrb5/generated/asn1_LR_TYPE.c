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
encode_LR_TYPE(unsigned char *p, size_t len, const LR_TYPE *data, size_t *size)
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
decode_LR_TYPE(const unsigned char *p, size_t len, LR_TYPE *data, size_t *size)
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
free_LR_TYPE(data);
return e;
}

void
free_LR_TYPE(LR_TYPE *data)
{
}

size_t
length_LR_TYPE(const LR_TYPE *data)
{
size_t ret = 0;
ret += length_integer((const int*)data);
return ret;
}

int
copy_LR_TYPE(const LR_TYPE *from, LR_TYPE *to)
{
*(to) = *(from);
return 0;
}

