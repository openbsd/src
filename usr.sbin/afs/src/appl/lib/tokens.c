/*
 * Copyright (c) 2001, 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "appl_locl.h"

RCSID("$arla: tokens.c,v 1.9 2003/02/01 17:01:11 lha Exp $");

/*
 * Iterate over all tokens and apply `func' on all of them, passing
 * the user argument `arg' to func. If `cell' is used, only the token
 * of cell is passed to func.
 */

int
arlalib_token_iter (const char *cell, arlalib_token_iter_func func, void *arg)
{
    uint32_t i;
    unsigned char data[4256];
    struct ViceIoctl parms;
    char token_cell[128];
    unsigned char secret[4000];
    size_t secret_sz;
    struct ClearToken ct;
    int ret;

    if (!k_hasafs())
	return ENOSYS;

    for (i = 0;; i++) {
	uint32_t sz;
	unsigned char *t = data;

	parms.in = (void *)&i;
	parms.in_size = sizeof(i);
	parms.out = (void *)data;
	parms.out_size = sizeof(data); 
	
	if(k_pioctl(NULL, VIOCGETTOK, &parms, 0) != 0) {
	    if(errno == EDOM)
		break;
	    return EINVAL;
	}

	/* secret token, v4 or v5 ticket */
	memcpy (&sz, t, sizeof(sz));
	t += sizeof(sz);
	if (parms.out_size < t - data + sz)
	    continue;
	if (sz > sizeof(secret))
	    continue;
	memcpy (secret, t, sz);
	secret_sz = sz;
	t += sz;

	/* clear token */
	memcpy (&sz, t, sizeof(sz));
	t += sizeof(sz);
	if (parms.out_size < t - data + sz)
	    continue;
	if (sz != sizeof(ct))
	    continue;
	memcpy (&ct, t, sz);
	t += sz;

	t += 4; /* skip primary cell flag */
	
	/* cell name */
	data[sizeof(data) - 1] = '\0'; /* it should be zero terminated, 
					  but make sure it is */
	strlcpy(token_cell, t, sizeof(token_cell));
	
	if (cell == NULL || strcasecmp(token_cell, cell) == 0) {
	    ret = (*func) (secret, secret_sz, &ct, token_cell, arg);
	    memset (data, 0, sizeof(data));
	    if (cell != NULL)
		return ret;
	}
    }
    memset (data, 0, sizeof(data));
    if (cell)
	return ENOENT;
    return 0;
}
