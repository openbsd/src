/*	$OpenBSD: crypt2.c,v 1.3 2005/08/08 08:05:33 espie Exp $	*/

/*
 * FreeSec: libcrypt
 *
 * Copyright (c) 1994 David Burren
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
 * 4. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * This is an original implementation of the DES and the crypt(3) interfaces
 * by David Burren <davidb@werj.com.au>.
 *
 * An excellent reference on the underlying algorithm (and related
 * algorithms) is:
 *
 *	B. Schneier, Applied Cryptography: protocols, algorithms,
 *	and source code in C, John Wiley & Sons, 1994.
 *
 * Note that in that book's description of DES the lookups for the initial,
 * pbox, and final permutations are inverted (this has been brought to the
 * attention of the author).  A list of errata for this book has been
 * posted to the sci.crypt newsgroup by the author and is available for FTP.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>

#ifdef DEBUG
# include <stdio.h>
#endif

extern const u_char _des_bits8[8];
extern const u_int32_t _des_bits32[32];
extern int	_des_initialised;

int
setkey(const char *key)
{
	int	i, j;
	u_int32_t packed_keys[2];
	u_char	*p;

	p = (u_char *) packed_keys;

	for (i = 0; i < 8; i++) {
		p[i] = 0;
		for (j = 0; j < 8; j++)
			if (*key++ & 1)
				p[i] |= _des_bits8[j];
	}
	return(des_setkey((char *)p));
}

int
encrypt(char *block, int flag)
{
	u_int32_t io[2];
	u_char	*p;
	int	i, j, retval;

	if (!_des_initialised)
		_des_init();

	_des_setup_salt(0);
	p = (u_char *)block;
	for (i = 0; i < 2; i++) {
		io[i] = 0L;
		for (j = 0; j < 32; j++)
			if (*p++ & 1)
				io[i] |= _des_bits32[j];
	}
	retval = _des_do_des(io[0], io[1], io, io + 1, flag ? -1 : 1);
	for (i = 0; i < 2; i++)
		for (j = 0; j < 32; j++)
			block[(i << 5) | j] = (io[i] & _des_bits32[j]) ? 1 : 0;
	return(retval);
}
