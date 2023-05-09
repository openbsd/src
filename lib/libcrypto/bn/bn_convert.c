/* $OpenBSD: bn_convert.c,v 1.8 2023/05/09 05:15:55 jsing Exp $ */
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

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

#include "bn_local.h"
#include "bytestring.h"

static const char hex_digits[] = "0123456789ABCDEF";

typedef enum {
	big,
	little,
} endianness_t;

/* ignore negative */
static int
bn2binpad(const BIGNUM *a, unsigned char *to, int tolen, endianness_t endianness)
{
	int n;
	size_t i, lasti, j, atop, mask;
	BN_ULONG l;

	/*
	 * In case |a| is fixed-top, BN_num_bytes can return bogus length,
	 * but it's assumed that fixed-top inputs ought to be "nominated"
	 * even for padded output, so it works out...
	 */
	n = BN_num_bytes(a);
	if (tolen == -1)
		tolen = n;
	else if (tolen < n) {	/* uncommon/unlike case */
		BIGNUM temp = *a;

		bn_correct_top(&temp);

		n = BN_num_bytes(&temp);
		if (tolen < n)
			return -1;
	}

	/* Swipe through whole available data and don't give away padded zero. */
	atop = a->dmax * BN_BYTES;
	if (atop == 0) {
		explicit_bzero(to, tolen);
		return tolen;
	}

	lasti = atop - 1;
	atop = a->top * BN_BYTES;

	if (endianness == big)
		to += tolen; /* start from the end of the buffer */

	for (i = 0, j = 0; j < (size_t)tolen; j++) {
		unsigned char val;

		l = a->d[i / BN_BYTES];
		mask = 0 - ((j - atop) >> (8 * sizeof(i) - 1));
		val = (unsigned char)(l >> (8 * (i % BN_BYTES)) & mask);

		if (endianness == big)
			*--to = val;
		else
			*to++ = val;

		i += (i - lasti) >> (8 * sizeof(i) - 1); /* stay on last limb */
	}

	return tolen;
}

int
BN_bn2bin(const BIGNUM *a, unsigned char *to)
{
	return bn2binpad(a, to, -1, big);
}

int
BN_bn2binpad(const BIGNUM *a, unsigned char *to, int tolen)
{
	if (tolen < 0)
		return -1;
	return bn2binpad(a, to, tolen, big);
}

BIGNUM *
BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret)
{
	unsigned int i, m;
	unsigned int n;
	BN_ULONG l;
	BIGNUM *bn = NULL;

	if (len < 0)
		return (NULL);
	if (ret == NULL)
		ret = bn = BN_new();
	if (ret == NULL)
		return (NULL);
	l = 0;
	n = len;
	if (n == 0) {
		ret->top = 0;
		return (ret);
	}
	i = ((n - 1) / BN_BYTES) + 1;
	m = ((n - 1) % (BN_BYTES));
	if (!bn_wexpand(ret, (int)i)) {
		BN_free(bn);
		return NULL;
	}
	ret->top = i;
	ret->neg = 0;
	while (n--) {
		l = (l << 8L) | *(s++);
		if (m-- == 0) {
			ret->d[--i] = l;
			l = 0;
			m = BN_BYTES - 1;
		}
	}
	/* need to call this due to clear byte at top if avoiding
	 * having the top bit set (-ve number) */
	bn_correct_top(ret);
	return (ret);
}

int
BN_bn2lebinpad(const BIGNUM *a, unsigned char *to, int tolen)
{
	if (tolen < 0)
		return -1;

	return bn2binpad(a, to, tolen, little);
}

BIGNUM *
BN_lebin2bn(const unsigned char *s, int len, BIGNUM *ret)
{
	unsigned int i, m, n;
	BN_ULONG l;
	BIGNUM *bn = NULL;

	if (ret == NULL)
		ret = bn = BN_new();
	if (ret == NULL)
		return NULL;


	s += len;
	/* Skip trailing zeroes. */
	for (; len > 0 && s[-1] == 0; s--, len--)
		continue;

	n = len;
	if (n == 0) {
		ret->top = 0;
		return ret;
	}

	i = ((n - 1) / BN_BYTES) + 1;
	m = (n - 1) % BN_BYTES;
	if (!bn_wexpand(ret, (int)i)) {
		BN_free(bn);
		return NULL;
	}

	ret->top = i;
	ret->neg = 0;
	l = 0;
	while (n-- > 0) {
		s--;
		l = (l << 8L) | *s;
		if (m-- == 0) {
			ret->d[--i] = l;
			l = 0;
			m = BN_BYTES - 1;
		}
	}

	/*
	 * need to call this due to clear byte at top if avoiding having the
	 * top bit set (-ve number)
	 */
	bn_correct_top(ret);

	return ret;
}

int
BN_asc2bn(BIGNUM **bn, const char *a)
{
	const char *p = a;
	if (*p == '-')
		p++;

	if (p[0] == '0' && (p[1] == 'X' || p[1] == 'x')) {
		if (!BN_hex2bn(bn, p + 2))
			return 0;
	} else {
		if (!BN_dec2bn(bn, p))
			return 0;
	}
	if (*a == '-')
		BN_set_negative(*bn, 1);
	return 1;
}

char *
BN_bn2dec(const BIGNUM *bn)
{
	int started = 0;
	BIGNUM *tmp = NULL;
	uint8_t *data = NULL;
	size_t data_len = 0;
	uint8_t *s = NULL;
	size_t s_len;
	BN_ULONG v, w;
	uint8_t c;
	CBB cbb;
	CBS cbs;
	int i;

	if (!CBB_init(&cbb, 0))
		goto err;

	if ((tmp = BN_dup(bn)) == NULL)
		goto err;

	/*
	 * Divide the BIGNUM by a large multiple of 10, then break the remainder
	 * into decimal digits. This produces a reversed string of digits,
	 * potentially with leading zeroes.
	 */
	while (!BN_is_zero(tmp)) {
		if ((w = BN_div_word(tmp, BN_DEC_CONV)) == -1)
			goto err;
		for (i = 0; i < BN_DEC_NUM; i++) {
			v = w % 10;
			if (!CBB_add_u8(&cbb, '0' + v))
				goto err;
			w /= 10;
		}
	}
	if (!CBB_finish(&cbb, &data, &data_len))
		goto err;

	if (data_len > SIZE_MAX - 3)
		goto err;
	if (!CBB_init(&cbb, data_len + 3))
		goto err;

	if (BN_is_negative(bn)) {
		if (!CBB_add_u8(&cbb, '-'))
			goto err;
	}

	/* Reverse digits and trim leading zeroes. */
	CBS_init(&cbs, data, data_len);
	while (CBS_len(&cbs) > 0) {
		if (!CBS_get_last_u8(&cbs, &c))
			goto err;
		if (!started && c == '0')
			continue;
		if (!CBB_add_u8(&cbb, c))
			goto err;
		started = 1;
	}

	if (!started) {
		if (!CBB_add_u8(&cbb, '0'))
			goto err;
	}
	if (!CBB_add_u8(&cbb, '\0'))
		goto err;
	if (!CBB_finish(&cbb, &s, &s_len))
		goto err;

 err:
	BN_free(tmp);
	CBB_cleanup(&cbb);
	freezero(data, data_len);

	return s;
}

int
BN_dec2bn(BIGNUM **bn, const char *a)
{
	BIGNUM *ret = NULL;
	BN_ULONG l = 0;
	int neg = 0, i, j;
	int num;

	if ((a == NULL) || (*a == '\0'))
		return (0);
	if (*a == '-') {
		neg = 1;
		a++;
	}

	for (i = 0; i <= (INT_MAX / 4) && isdigit((unsigned char)a[i]); i++)
		;
	if (i > INT_MAX / 4)
		return (0);

	num = i + neg;
	if (bn == NULL)
		return (num);

	/* a is the start of the digits, and it is 'i' long.
	 * We chop it into BN_DEC_NUM digits at a time */
	if (*bn == NULL) {
		if ((ret = BN_new()) == NULL)
			return (0);
	} else {
		ret = *bn;
		BN_zero(ret);
	}

	/* i is the number of digits, a bit of an over expand */
	if (!bn_expand(ret, i * 4))
		goto err;

	j = BN_DEC_NUM - (i % BN_DEC_NUM);
	if (j == BN_DEC_NUM)
		j = 0;
	l = 0;
	while (*a) {
		l *= 10;
		l += *a - '0';
		a++;
		if (++j == BN_DEC_NUM) {
			if (!BN_mul_word(ret, BN_DEC_CONV))
				goto err;
			if (!BN_add_word(ret, l))
				goto err;
			l = 0;
			j = 0;
		}
	}

	bn_correct_top(ret);

	BN_set_negative(ret, neg);

	*bn = ret;
	return (num);

err:
	if (*bn == NULL)
		BN_free(ret);
	return (0);
}

char *
BN_bn2hex(const BIGNUM *bn)
{
	int started = 0;
	uint8_t *s = NULL;
	size_t s_len;
	BN_ULONG v, w;
	int i, j;
	CBB cbb;

	if (!CBB_init(&cbb, 0))
		goto err;

	if (BN_is_negative(bn)) {
		if (!CBB_add_u8(&cbb, '-'))
			goto err;
	}
	if (BN_is_zero(bn)) {
		if (!CBB_add_u8(&cbb, '0'))
			goto err;
	}
	for (i = bn->top - 1; i >= 0; i--) {
		w = bn->d[i];
		for (j = BN_BITS2 - 8; j >= 0; j -= 8) {
			v = (w >> j) & 0xff;
			if (!started && v == 0)
				continue;
			if (!CBB_add_u8(&cbb, hex_digits[v >> 4]))
				goto err;
			if (!CBB_add_u8(&cbb, hex_digits[v & 0xf]))
				goto err;
			started = 1;
		}
	}
	if (!CBB_add_u8(&cbb, '\0'))
		goto err;
	if (!CBB_finish(&cbb, &s, &s_len))
		goto err;

 err:
	CBB_cleanup(&cbb);

	return s;
}

int
BN_hex2bn(BIGNUM **bn, const char *a)
{
	BIGNUM *ret = NULL;
	BN_ULONG l = 0;
	int neg = 0, h, m, i,j, k, c;
	int num;

	if ((a == NULL) || (*a == '\0'))
		return (0);

	if (*a == '-') {
		neg = 1;
		a++;
	}

	for (i = 0; i <= (INT_MAX / 4) && isxdigit((unsigned char)a[i]); i++)
		;
	if (i > INT_MAX / 4)
		return (0);

	num = i + neg;
	if (bn == NULL)
		return (num);

	/* a is the start of the hex digits, and it is 'i' long */
	if (*bn == NULL) {
		if ((ret = BN_new()) == NULL)
			return (0);
	} else {
		ret = *bn;
		BN_zero(ret);
	}

	/* i is the number of hex digits */
	if (!bn_expand(ret, i * 4))
		goto err;

	j = i; /* least significant 'hex' */
	m = 0;
	h = 0;
	while (j > 0) {
		m = ((BN_BYTES * 2) <= j) ? (BN_BYTES * 2) : j;
		l = 0;
		for (;;) {
			c = a[j - m];
			if ((c >= '0') && (c <= '9'))
				k = c - '0';
			else if ((c >= 'a') && (c <= 'f'))
				k = c - 'a' + 10;
			else if ((c >= 'A') && (c <= 'F'))
				k = c - 'A' + 10;
			else
				k = 0; /* paranoia */
			l = (l << 4) | k;

			if (--m <= 0) {
				ret->d[h++] = l;
				break;
			}
		}
		j -= (BN_BYTES * 2);
	}
	ret->top = h;
	bn_correct_top(ret);

	BN_set_negative(ret, neg);

	*bn = ret;
	return (num);

err:
	if (*bn == NULL)
		BN_free(ret);
	return (0);
}

int
BN_bn2mpi(const BIGNUM *a, unsigned char *d)
{
	int bits;
	int num = 0;
	int ext = 0;
	long l;

	bits = BN_num_bits(a);
	num = (bits + 7) / 8;
	if (bits > 0) {
		ext = ((bits & 0x07) == 0);
	}
	if (d == NULL)
		return (num + 4 + ext);

	l = num + ext;
	d[0] = (unsigned char)(l >> 24) & 0xff;
	d[1] = (unsigned char)(l >> 16) & 0xff;
	d[2] = (unsigned char)(l >> 8) & 0xff;
	d[3] = (unsigned char)(l) & 0xff;
	if (ext)
		d[4] = 0;
	num = BN_bn2bin(a, &(d[4 + ext]));
	if (a->neg)
		d[4] |= 0x80;
	return (num + 4 + ext);
}

BIGNUM *
BN_mpi2bn(const unsigned char *d, int n, BIGNUM *ain)
{
	BIGNUM *a = ain;
	long len;
	int neg = 0;

	if (n < 4) {
		BNerror(BN_R_INVALID_LENGTH);
		return (NULL);
	}
	len = ((long)d[0] << 24) | ((long)d[1] << 16) | ((int)d[2] << 8) |
	    (int)d[3];
	if ((len + 4) != n) {
		BNerror(BN_R_ENCODING_ERROR);
		return (NULL);
	}

	if (a == NULL)
		a = BN_new();
	if (a == NULL)
		return (NULL);

	if (len == 0) {
		a->neg = 0;
		a->top = 0;
		return (a);
	}
	d += 4;
	if ((*d) & 0x80)
		neg = 1;
	if (BN_bin2bn(d, (int)len, a) == NULL) {
		if (ain == NULL)
			BN_free(a);
		return (NULL);
	}
	BN_set_negative(a, neg);
	if (neg) {
		BN_clear_bit(a, BN_num_bits(a) - 1);
	}
	return (a);
}

#ifndef OPENSSL_NO_BIO
int
BN_print_fp(FILE *fp, const BIGNUM *a)
{
	BIO *b;
	int ret;

	if ((b = BIO_new(BIO_s_file())) == NULL)
		return (0);
	BIO_set_fp(b, fp, BIO_NOCLOSE);
	ret = BN_print(b, a);
	BIO_free(b);
	return (ret);
}

int
BN_print(BIO *bp, const BIGNUM *a)
{
	int i, j, v, z = 0;
	int ret = 0;

	if ((a->neg) && (BIO_write(bp, "-", 1) != 1))
		goto end;
	if (BN_is_zero(a) && (BIO_write(bp, "0", 1) != 1))
		goto end;
	for (i = a->top - 1; i >= 0; i--) {
		for (j = BN_BITS2 - 4; j >= 0; j -= 4) {
			/* strip leading zeros */
			v = ((int)(a->d[i] >> (long)j)) & 0x0f;
			if (z || (v != 0)) {
				if (BIO_write(bp, &hex_digits[v], 1) != 1)
					goto end;
				z = 1;
			}
		}
	}
	ret = 1;

end:
	return (ret);
}
#endif
