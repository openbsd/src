/*	$OpenBSD: gmp_util.c,v 1.4 1999/02/26 03:40:05 niklas Exp $	*/
/*	$EOM: gmp_util.c,v 1.2 1999/02/25 11:38:58 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <gmp.h>

#include "sysdep.h"

#include "gmp_util.h"

/* Various utility functions for gmp, used in more than one module */

u_int32_t
mpz_sizeinoctets (mpz_ptr a)
{
  return (7 + mpz_sizeinbase (a, 2)) >> 3;
}

void
mpz_getraw (u_int8_t *raw, mpz_ptr v, u_int32_t len)
{
  mpz_t a, tmp;

  mpz_init_set (a, v);
  mpz_init (tmp);

  while (len-- > 0)
      raw[len] = mpz_fdiv_qr_ui (a, tmp, a, 256);

  mpz_clear (a);
  mpz_clear (tmp);
}

void
mpz_setraw (mpz_ptr d, u_int8_t *s, u_int32_t l)
{
  u_int32_t i;

  mpz_set_ui (d, 0);
  for (i = 0; i < l; i++)
    {
      mpz_mul_ui (d, d, 256);
      mpz_add_ui (d, d, s[i]);
    }
}

