/* $OpenBSD: gmp_util.c,v 1.12 2005/04/08 19:19:39 hshoexer Exp $	 */
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
	return BN_num_bytes(a);
}

void
mpz_getraw(u_int8_t *raw, math_mp_t v, u_int32_t len)
{
	math_mp_t       a;

	/* XXX bn2bin?  */
	a = BN_dup(v);

	while (len-- > 0)
	raw[len] = BN_div_word(a, 256);

	BN_clear_free(a);
}

void
mpz_setraw(math_mp_t d, u_int8_t *s, u_int32_t l)
{
	u_int32_t       i;

	/* XXX bin2bn?  */
	BN_set_word(d, 0);
	for (i = 0; i < l; i++) {
		BN_mul_word(d, 256);
		BN_add_word(d, s[i]);
	}
}
