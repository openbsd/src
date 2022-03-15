/* $OpenBSD: a_object.c,v 1.41 2022/03/15 18:47:22 jsing Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/buffer.h>
#include <openssl/objects.h>

#include "asn1_locl.h"

const ASN1_ITEM ASN1_OBJECT_it = {
	.itype = ASN1_ITYPE_PRIMITIVE,
	.utype = V_ASN1_OBJECT,
	.sname = "ASN1_OBJECT",
};

ASN1_OBJECT *
ASN1_OBJECT_new(void)
{
	ASN1_OBJECT *a;

	if ((a = calloc(1, sizeof(ASN1_OBJECT))) == NULL) {
		ASN1error(ERR_R_MALLOC_FAILURE);
		return (NULL);
	}
	a->flags = ASN1_OBJECT_FLAG_DYNAMIC;

	return a;
}

void
ASN1_OBJECT_free(ASN1_OBJECT *a)
{
	if (a == NULL)
		return;
	if (a->flags & ASN1_OBJECT_FLAG_DYNAMIC_STRINGS) {
		free((void *)a->sn);
		free((void *)a->ln);
		a->sn = a->ln = NULL;
	}
	if (a->flags & ASN1_OBJECT_FLAG_DYNAMIC_DATA) {
		freezero((void *)a->data, a->length);
		a->data = NULL;
		a->length = 0;
	}
	if (a->flags & ASN1_OBJECT_FLAG_DYNAMIC)
		free(a);
}

ASN1_OBJECT *
ASN1_OBJECT_create(int nid, unsigned char *data, int len,
    const char *sn, const char *ln)
{
	ASN1_OBJECT o;

	o.sn = sn;
	o.ln = ln;
	o.data = data;
	o.nid = nid;
	o.length = len;
	o.flags = ASN1_OBJECT_FLAG_DYNAMIC | ASN1_OBJECT_FLAG_DYNAMIC_STRINGS |
	    ASN1_OBJECT_FLAG_DYNAMIC_DATA;
	return (OBJ_dup(&o));
}

int
i2d_ASN1_OBJECT(const ASN1_OBJECT *a, unsigned char **pp)
{
	unsigned char *p;
	int objsize;

	if ((a == NULL) || (a->data == NULL))
		return (0);

	objsize = ASN1_object_size(0, a->length, V_ASN1_OBJECT);
	if (pp == NULL)
		return objsize;

	p = *pp;
	ASN1_put_object(&p, 0, a->length, V_ASN1_OBJECT, V_ASN1_UNIVERSAL);
	memcpy(p, a->data, a->length);
	p += a->length;

	*pp = p;
	return (objsize);
}

int
a2d_ASN1_OBJECT(unsigned char *out, int olen, const char *buf, int num)
{
	int i, first, len = 0, c, use_bn;
	char ftmp[24], *tmp = ftmp;
	int tmpsize = sizeof ftmp;
	const char *p;
	unsigned long l;
	BIGNUM *bl = NULL;

	if (num == 0)
		return (0);
	else if (num == -1)
		num = strlen(buf);

	p = buf;
	c = *(p++);
	num--;
	if ((c >= '0') && (c <= '2')) {
		first= c-'0';
	} else {
		ASN1error(ASN1_R_FIRST_NUM_TOO_LARGE);
		goto err;
	}

	if (num <= 0) {
		ASN1error(ASN1_R_MISSING_SECOND_NUMBER);
		goto err;
	}
	c = *(p++);
	num--;
	for (;;) {
		if (num <= 0)
			break;
		if ((c != '.') && (c != ' ')) {
			ASN1error(ASN1_R_INVALID_SEPARATOR);
			goto err;
		}
		l = 0;
		use_bn = 0;
		for (;;) {
			if (num <= 0)
				break;
			num--;
			c = *(p++);
			if ((c == ' ') || (c == '.'))
				break;
			if ((c < '0') || (c > '9')) {
				ASN1error(ASN1_R_INVALID_DIGIT);
				goto err;
			}
			if (!use_bn && l >= ((ULONG_MAX - 80) / 10L)) {
				use_bn = 1;
				if (!bl)
					bl = BN_new();
				if (!bl || !BN_set_word(bl, l))
					goto err;
			}
			if (use_bn) {
				if (!BN_mul_word(bl, 10L) ||
				    !BN_add_word(bl, c-'0'))
					goto err;
			} else
				l = l * 10L + (long)(c - '0');
		}
		if (len == 0) {
			if ((first < 2) && (l >= 40)) {
				ASN1error(ASN1_R_SECOND_NUMBER_TOO_LARGE);
				goto err;
			}
			if (use_bn) {
				if (!BN_add_word(bl, first * 40))
					goto err;
			} else
				l += (long)first * 40;
		}
		i = 0;
		if (use_bn) {
			int blsize;
			blsize = BN_num_bits(bl);
			blsize = (blsize + 6) / 7;
			if (blsize > tmpsize) {
				if (tmp != ftmp)
					free(tmp);
				tmpsize = blsize + 32;
				tmp = malloc(tmpsize);
				if (!tmp)
					goto err;
			}
			while (blsize--)
				tmp[i++] = (unsigned char)BN_div_word(bl, 0x80L);
		} else {

			for (;;) {
				tmp[i++] = (unsigned char)l & 0x7f;
				l >>= 7L;
				if (l == 0L)
					break;
			}

		}
		if (out != NULL) {
			if (len + i > olen) {
				ASN1error(ASN1_R_BUFFER_TOO_SMALL);
				goto err;
			}
			while (--i > 0)
				out[len++] = tmp[i]|0x80;
			out[len++] = tmp[0];
		} else
			len += i;
	}
	if (tmp != ftmp)
		free(tmp);
	BN_free(bl);
	return (len);

 err:
	if (tmp != ftmp)
		free(tmp);
	BN_free(bl);
	return (0);
}

static int
oid_parse_arc(CBS *cbs, uint64_t *out_arc)
{
	uint64_t arc = 0;
	uint8_t val;

	do {
		if (!CBS_get_u8(cbs, &val))
			return 0;
		if (arc == 0 && val == 0x80)
			return 0;
		if (arc > (UINT64_MAX >> 7))
			return 0;
		arc = (arc << 7) | (val & 0x7f);
	} while (val & 0x80);

	*out_arc = arc;

	return 1;
}

static int
oid_add_arc_txt(CBB *cbb, uint64_t arc, int first)
{
	const char *fmt = ".%llu";
	char s[22]; /* Digits in decimal representation of 2^64-1, plus '.' and NUL. */
	int n;

	if (first)
		fmt = "%llu";
	n = snprintf(s, sizeof(s), fmt, (unsigned long long)arc);
	if (n < 0 || (size_t)n >= sizeof(s))
		return 0;
	if (!CBB_add_bytes(cbb, s, n))
		return 0;

	return 1;
}

static int
c2a_ASN1_OBJECT(CBS *cbs, CBB *cbb)
{
	uint64_t arc, si1, si2;

	/*
	 * X.690 section 8.19 - the first two subidentifiers are encoded as
	 * (x * 40) + y, with x being limited to [0,1,2].
	 */
	if (!oid_parse_arc(cbs, &arc))
		return 0;
	if ((si1 = arc / 40) > 2)
		si1 = 2;
	si2 = arc - si1 * 40;

	if (!oid_add_arc_txt(cbb, si1, 1))
		return 0;
	if (!oid_add_arc_txt(cbb, si2, 0))
		return 0;

	while (CBS_len(cbs) > 0) {
		if (!oid_parse_arc(cbs, &arc))
			return 0;
		if (!oid_add_arc_txt(cbb, arc, 0))
			return 0;
	}

	/* NUL terminate. */
	if (!CBB_add_u8(cbb, 0))
		return 0;

	return 1;
}

static int
i2t_ASN1_OBJECT_oid(const ASN1_OBJECT *aobj, CBB *cbb)
{
	CBS cbs;

	CBS_init(&cbs, aobj->data, aobj->length);

	return c2a_ASN1_OBJECT(&cbs, cbb);
}

static int
i2t_ASN1_OBJECT_name(const ASN1_OBJECT *aobj, CBB *cbb, const char **out_name)
{
	const char *name;
	int nid;

	*out_name = NULL;

	if ((nid = OBJ_obj2nid(aobj)) == NID_undef)
		return 0;

	if ((name = OBJ_nid2ln(nid)) == NULL)
		name = OBJ_nid2sn(nid);
	if (name == NULL)
		return 0;

	*out_name = name;

	if (!CBB_add_bytes(cbb, name, strlen(name)))
		return 0;

	/* NUL terminate. */
	if (!CBB_add_u8(cbb, 0))
		return 0;

	return 1;
}

static int
i2t_ASN1_OBJECT_cbb(const ASN1_OBJECT *aobj, CBB *cbb, int no_name)
{
	const char *name;

	if (!no_name) {
		if (i2t_ASN1_OBJECT_name(aobj, cbb, &name))
			return 1;
		if (name != NULL)
			return 0;
	}
	return i2t_ASN1_OBJECT_oid(aobj, cbb);
}

int
i2t_ASN1_OBJECT_internal(const ASN1_OBJECT *aobj, char *buf, int buf_len, int no_name)
{
	uint8_t *data = NULL;
	size_t data_len;
	CBB cbb;
	int ret = 0;

	if (buf_len < 0)
		return 0;
	if (buf_len > 0)
		buf[0] = '\0';

	if (!CBB_init(&cbb, 0))
		goto err;
	if (!i2t_ASN1_OBJECT_cbb(aobj, &cbb, no_name))
		goto err;
	if (!CBB_finish(&cbb, &data, &data_len))
		goto err;

	ret = strlcpy(buf, data, buf_len);
 err:
	CBB_cleanup(&cbb);
	free(data);

	return ret;
}

int
i2t_ASN1_OBJECT(char *buf, int buf_len, const ASN1_OBJECT *aobj)
{
	return i2t_ASN1_OBJECT_internal(aobj, buf, buf_len, 0);
}

int
i2a_ASN1_OBJECT(BIO *bp, const ASN1_OBJECT *aobj)
{
	uint8_t *data = NULL;
	size_t data_len;
	CBB cbb;
	int ret = -1;

	if (aobj == NULL || aobj->data == NULL)
		return BIO_write(bp, "NULL", 4);

	if (!CBB_init(&cbb, 0))
		goto err;
	if (!i2t_ASN1_OBJECT_cbb(aobj, &cbb, 0)) {
		ret = BIO_write(bp, "<INVALID>", 9);
		goto err;
	}
	if (!CBB_finish(&cbb, &data, &data_len))
		goto err;

	ret = BIO_write(bp, data, strlen(data));

 err:
	CBB_cleanup(&cbb);
	free(data);

	return ret;
}

ASN1_OBJECT *
d2i_ASN1_OBJECT(ASN1_OBJECT **a, const unsigned char **pp, long length)
{
	const unsigned char *p;
	long len;
	int tag, xclass;
	int inf, i;
	ASN1_OBJECT *ret = NULL;

	p = *pp;
	inf = ASN1_get_object(&p, &len, &tag, &xclass, length);
	if (inf & 0x80) {
		i = ASN1_R_BAD_OBJECT_HEADER;
		goto err;
	}

	if (tag != V_ASN1_OBJECT) {
		i = ASN1_R_EXPECTING_AN_OBJECT;
		goto err;
	}
	ret = c2i_ASN1_OBJECT(a, &p, len);
	if (ret)
		*pp = p;
	return ret;

 err:
	ASN1error(i);
	return (NULL);
}

ASN1_OBJECT *
c2i_ASN1_OBJECT(ASN1_OBJECT **a, const unsigned char **pp, long len)
{
	ASN1_OBJECT *ret;
	const unsigned char *p;
	unsigned char *data;
	int i, length;

	/*
	 * Sanity check OID encoding:
	 * - need at least one content octet
	 * - MSB must be clear in the last octet
	 * - can't have leading 0x80 in subidentifiers, see: X.690 8.19.2
	 */
	if (len <= 0 || len > INT_MAX || pp == NULL || (p = *pp) == NULL ||
	    p[len - 1] & 0x80) {
		ASN1error(ASN1_R_INVALID_OBJECT_ENCODING);
		return (NULL);
	}

	/* Now 0 < len <= INT_MAX, so the cast is safe. */
	length = (int)len;
	for (i = 0; i < length; i++, p++) {
		if (*p == 0x80 && (!i || !(p[-1] & 0x80))) {
			ASN1error(ASN1_R_INVALID_OBJECT_ENCODING);
			return (NULL);
		}
	}

	if ((a == NULL) || ((*a) == NULL) ||
	    !((*a)->flags & ASN1_OBJECT_FLAG_DYNAMIC)) {
		if ((ret = ASN1_OBJECT_new()) == NULL)
			return (NULL);
	} else
		ret = *a;

	p = *pp;

	/* detach data from object */
	data = (unsigned char *)ret->data;
	freezero(data, ret->length);

	data = malloc(length);
	if (data == NULL) {
		ASN1error(ERR_R_MALLOC_FAILURE);
		goto err;
	}

	memcpy(data, p, length);

	/* If there are dynamic strings, free them here, and clear the flag. */
	if ((ret->flags & ASN1_OBJECT_FLAG_DYNAMIC_STRINGS) != 0) {
		free((void *)ret->sn);
		free((void *)ret->ln);
		ret->flags &= ~ASN1_OBJECT_FLAG_DYNAMIC_STRINGS;
	}

	/* reattach data to object, after which it remains const */
	ret->data = data;
	ret->length = length;
	ret->sn = NULL;
	ret->ln = NULL;
	ret->flags |= ASN1_OBJECT_FLAG_DYNAMIC_DATA;
	p += length;

	if (a != NULL)
		*a = ret;
	*pp = p;
	return (ret);

 err:
	if (a == NULL || ret != *a)
		ASN1_OBJECT_free(ret);
	return (NULL);
}
