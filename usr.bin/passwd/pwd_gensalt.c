/*	$OpenBSD: pwd_gensalt.c,v 1.22 2004/12/20 15:05:59 moritz Exp $ */

/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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

#include <sys/syslimits.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <login_cap.h>

void	to64(char *, int32_t, int n);
char	*bcrypt_gensalt(u_int8_t);
int	pwd_gensalt(char *, int, login_cap_t *, char);

#define	YPCIPHER_DEF		"old"
#define	LOCALCIPHER_DEF		"blowfish,6"

int
pwd_gensalt(char *salt, int saltlen, login_cap_t *lc, char type)
{
	char *next, *now, *oldnext;

	*salt = '\0';

	switch (type) {
	case 'y':
		next = login_getcapstr(lc, "ypcipher", NULL, NULL);
		if (next == NULL && (next = strdup(YPCIPHER_DEF)) == NULL) {
			warn(NULL);
			return 0;
		}
		break;
	case 'l':
	default:
		next = login_getcapstr(lc, "localcipher", NULL, NULL);
		if (next == NULL && (next = strdup(LOCALCIPHER_DEF)) == NULL) {
			warn(NULL);
			return 0;
		}
		break;
	}

	oldnext = next;
	now = strsep(&next, ",");
	if (!strcmp(now, "old")) {
		if (saltlen < 3) {
			free(oldnext);
			return 0;
		}
		to64(&salt[0], arc4random(), 2);
		salt[2] = '\0';
	} else if (!strcmp(now, "newsalt")) {
		u_int32_t rounds = 7250;

		if (next)
			rounds = atol(next);
		if (saltlen < 10) {
			free(oldnext);
			return 0;
		}
		/* Check rounds, 24 bit is max */
		if (rounds < 7250)
			rounds = 7250;
		else if (rounds > 0xffffff)
			rounds = 0xffffff;
		salt[0] = _PASSWORD_EFMT1;
		to64(&salt[1], (u_int32_t) rounds, 4);
		to64(&salt[5], arc4random(), 4);
		salt[9] = '\0';
	} else if (!strcmp(now, "md5")) {
		if (saltlen < 13) {	/* $1$8salt$\0 */
			free(oldnext);
			return 0;
		}

		strlcpy(salt, "$1$", saltlen);
		to64(&salt[3], arc4random(), 4);
		to64(&salt[7], arc4random(), 4);
		strlcpy(&salt[11], "$", saltlen - 11);
	} else if (!strcmp(now, "blowfish")) {
		int rounds = 6;

		if (next)
			rounds = atoi(next);
		if (rounds < 4)
			rounds = 4;
		if (rounds > 31)
			rounds = 31;
		strlcpy(salt, bcrypt_gensalt(rounds), saltlen);
	} else {
		warnx("Unknown option %s.", now);
		free(oldnext);
		return 0;
	}
	free(oldnext);
	return 1;
}

static unsigned char itoa64[] =	 /* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
to64(char *s, int32_t v, int n)
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}
