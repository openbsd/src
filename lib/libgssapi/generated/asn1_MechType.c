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
encode_MechType(unsigned char *p, size_t len, const MechType *data, size_t *size)
{
size_t ret = 0;
size_t l;
int i, e;

i = 0;
e = encode_oid(p, len, data, &l);
BACK;
*size = ret;
return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_MechType(const unsigned char *p, size_t len, MechType *data, size_t *size)
{
size_t ret = 0, reallen;
size_t l;
int e;

memset(data, 0, sizeof(*data));
reallen = 0;
e = decode_oid(p, len, data, &l);
FORW;
if(size) *size = ret;
return 0;
fail:
free_MechType(data);
return e;
}

void
free_MechType(MechType *data)
{
free_oid(data);
}

size_t
length_MechType(const MechType *data)
{
size_t ret = 0;
ret += length_oid(data);
return ret;
}

int
copy_MechType(const MechType *from, MechType *to)
{
if(copy_oid(from, to)) return ENOMEM;
return 0;
}

