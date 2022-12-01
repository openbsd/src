/*	$OpenBSD: bn_mod_exp2_mont.c,v 1.1 2022/12/01 20:50:10 tb Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>

#include <openssl/bn.h>

/*
 * Small test for a crash reported by Guido Vranken, fixed in bn_exp2.c r1.13.
 * https://github.com/openssl/openssl/issues/17648
 */

int
main(void)
{
	BIGNUM *m;

	if ((m = BN_new()) == NULL)
		errx(1, "BN_new");

	BN_zero_ex(m);

	if (BN_mod_exp2_mont(NULL, NULL, NULL, NULL, NULL, m, NULL, NULL))
		errx(1, "BN_mod_exp2_mont succeeded");

	BN_free(m);

	return 0;
}
