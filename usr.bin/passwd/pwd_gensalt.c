/* $OpenBSD: pwd_gensalt.c,v 1.9 1998/07/05 21:08:32 provos Exp $ */
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
#include <util.h>
#include <time.h>

void to64 __P((char *, int32_t, int n));

int
pwd_gensalt(salt, max, pwd, type)
	char   *salt;
	int     max;
	struct passwd *pwd;
	char    type;
{
	char   *bcrypt_gensalt __P((u_int8_t));
	char    option[LINE_MAX];
	char   *next, *now;
	char   *cipher;
	*salt = '\0';

	switch (type) {
	case 'y':
	        cipher = "ypcipher";
		break;
	case 'l':
	default:
	        cipher = "localcipher";
		break;
	}

	pw_getconf(option, LINE_MAX, pwd->pw_name, cipher);

	/* Try to find an entry for the group */
	if (*option == 0) {
	        struct group *grp;
	        char grpkey[LINE_MAX];

	        grp = getgrgid(pwd->pw_gid);
	        if (grp != NULL) {
                        snprintf(grpkey, LINE_MAX-1, ".%s", grp->gr_name);
			grpkey[LINE_MAX-1] = 0;
			pw_getconf(option, LINE_MAX, grpkey, cipher);
		}
		if (*option == 0)
		        pw_getconf(option, LINE_MAX, "default", cipher);
	}

	next = option;
	now = strsep(&next, ",");
	if (!strcmp(now, "old")) {
		if (max < 3)
			return 0;
		to64(&salt[0], arc4random(), 2);
		salt[2] = '\0';
	} else if (!strcmp(now, "newsalt")) {
		u_int32_t rounds = atol(next);
		if (max < 10)
			return 0;
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
		if (max < 13)  /* $1$8salt$\0 */
			return 0;
		strcpy(salt, "$1$");
		to64(&salt[3], arc4random(), 4);
		to64(&salt[7], arc4random(), 4);
		strcpy(&salt[11], "$");
	} else if (!strcmp(now, "blowfish")) {
		int     rounds = atoi(next);
		if (rounds < 4)
			rounds = 4;
		strncpy(salt, bcrypt_gensalt(rounds), max - 1);
		salt[max - 1] = 0;
	} else {
		strcpy(salt, ":");
		warnx("Unkown option %s.", now);
	}
	return 1;
}

static unsigned char itoa64[] =	 /* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
to64(s, v, n)
	register char *s;
	register int32_t v;
	register int n;
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}
