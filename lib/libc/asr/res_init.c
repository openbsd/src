/*	$OpenBSD: res_init.c,v 1.1 2012/09/08 11:08:21 eric Exp $	*/
/*
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#include <sys/types.h>
#include <netinet/in.h>

#include <resolv.h>

#include "asr.h"

/*
 * XXX these function is actually internal to asr, but we use it here to force
 * the creation a default resolver context in res_init().
 */
struct asr_ctx *asr_use_resolver(struct asr *);
void asr_ctx_unref(struct asr_ctx *);

struct __res_state _res;
struct __res_state_ext _res_ext;

int h_errno;

int
res_init(void)
{
	async_resolver_done(NULL);
	asr_ctx_unref(asr_use_resolver(NULL));

	return (0);
}
