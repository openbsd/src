/* $OpenBSD: exchange.h,v 1.5 2002/06/09 08:13:08 todd Exp $ */
/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
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
 *      This product includes software developed by Niels Provos.
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
 * exchange.h:
 * exchange generation header file
 */

#ifndef _EXCHANGE_H_
#define _EXCHANGE_H_

#undef EXTERN

#ifdef _EXCHANGE_C_
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN u_int8_t *varpre_get_number_bits(size_t *, u_int8_t *);
EXTERN u_int8_t *BN_varpre2bn(u_int8_t *, size_t, BIGNUM *);
EXTERN int BN_bn2varpre(BIGNUM *, u_int8_t *, size_t *);

EXTERN int exchange_set_generator(BIGNUM *, u_int8_t *, u_int8_t *);
EXTERN int exchange_check_value(BIGNUM *, BIGNUM *, BIGNUM *);
EXTERN int exchange_make_values(struct stateob *, BIGNUM *, BIGNUM *);
EXTERN int exchange_value_generate(struct stateob *, u_int8_t *, u_int16_t *);

#endif
