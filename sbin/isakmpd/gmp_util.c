/* $OpenBSD: gmp_util.c,v 1.11 2004/04/15 18:39:25 deraadt Exp $	 */
/* $EOM: gmp_util.c,v 1.7 2000/09/18 00:01:47 ho Exp $	 */

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2000 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2000 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/param.h>

#include "sysdep.h"

#include "gmp_util.h"
#include "math_mp.h"

/* Various utility functions for gmp, used in more than one module */

u_int32_t
mpz_sizeinoctets(math_mp_t a)
{
#if MP_FLAVOUR == MP_FLAVOUR_GMP
	return (7 + mpz_sizeinbase(a, 2)) >> 3;
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	return BN_num_bytes(a);
#endif
}

void
mpz_getraw(u_int8_t *raw, math_mp_t v, u_int32_t len)
{
	math_mp_t       a;

#if MP_FLAVOUR == MP_FLAVOUR_GMP
	math_mp_t       tmp;

	/* XXX  mpz_get_str (raw, BASE, v); ? */
	mpz_init_set(a, v);
	mpz_init(tmp);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	/* XXX bn2bin?  */
	a = BN_dup(v);
#endif

	while (len-- > 0)
#if MP_FLAVOUR == MP_FLAVOUR_GMP
		raw[len] = mpz_fdiv_qr_ui(a, tmp, a, 256);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	raw[len] = BN_div_word(a, 256);
#endif

#if MP_FLAVOUR == MP_FLAVOUR_GMP
	mpz_clear(a);
	mpz_clear(tmp);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	BN_clear_free(a);
#endif
}

void
mpz_setraw(math_mp_t d, u_int8_t *s, u_int32_t l)
{
	u_int32_t       i;

#if MP_FLAVOUR == MP_FLAVOUR_GMP
	/* XXX mpz_set_str (d, s, 0);  */
	mpz_set_si(d, 0);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	/* XXX bin2bn?  */
	BN_set_word(d, 0);
#endif
	for (i = 0; i < l; i++) {
#if MP_FLAVOUR == MP_FLAVOUR_GMP
		mpz_mul_ui(d, d, 256);
		mpz_add_ui(d, d, s[i]);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
		BN_mul_word(d, 256);
		BN_add_word(d, s[i]);
#endif
	}
}
