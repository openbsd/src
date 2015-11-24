/*	$OpenBSD: pwcache.c,v 1.12 2015/11/24 22:03:33 millert Exp $ */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>

#define	NCACHE	16			/* power of 2 */
#define	NLINES	4			/* associativity */
#define	MASK	(NCACHE - 1)		/* bits to store with */
#define	IDX(x, i)	((x & MASK) + i * NCACHE)

char *
user_from_uid(uid_t uid, int nouser)
{
	static struct ncache {
		uid_t	uid;
		short	noname;
		char	name[_PW_NAME_LEN + 1];
	} c_uid[NLINES * NCACHE];
	char pwbuf[_PW_BUF_LEN];
	struct passwd pwstore, *pw;
	struct ncache *cp;
	unsigned int i;

	for (i = 0; i < NLINES; i++) {
		cp = &c_uid[IDX(uid, i)];
		if (!*cp->name) {
fillit:
			cp->uid = uid;
			pw = NULL;
			getpwuid_r(uid, &pwstore, pwbuf, sizeof(pwbuf), &pw);
			if (pw == NULL) {
				snprintf(cp->name, sizeof(cp->name), "%u", uid);
				cp->noname = 1;
			} else {
				strlcpy(cp->name, pw->pw_name, sizeof(cp->name));
			}
		}
		if (cp->uid == uid) {
			if (nouser && cp->noname)
				return NULL;
			return cp->name;
		}
	}
	/* move everybody down a slot */
	for (i = 0; i < NLINES - 1; i++) {
		struct ncache *next;

		cp = &c_uid[IDX(uid, i)];
		next = &c_uid[IDX(uid, i + 1)];
		memcpy(next, cp, sizeof(*cp));
	}
	cp = &c_uid[IDX(uid, 0)];
	goto fillit;
}

char *
group_from_gid(gid_t gid, int nogroup)
{
	static struct ncache {
		gid_t	gid;
		short 	noname;
		char	name[_PW_NAME_LEN + 1];
	} c_gid[NLINES * NCACHE];
	char grbuf[_PW_BUF_LEN];
	struct group grstore, *gr;
	struct ncache *cp;
	unsigned int i;

	for (i = 0; i < NLINES; i++) {
		cp = &c_gid[IDX(gid, i)];
		if (!*cp->name) {
fillit:
			cp->gid = gid;
			gr = NULL;
			getgrgid_r(gid, &grstore, grbuf, sizeof(grbuf), &gr);
			if (gr == NULL) {
				snprintf(cp->name, sizeof(cp->name), "%u", gid);
				cp->noname = 1;
			} else {
				strlcpy(cp->name, gr->gr_name, sizeof(cp->name));
			}
		}
		if (cp->gid == gid) {
			if (nogroup && cp->noname)
				return NULL;
			return cp->name;
		}
	}
	/* move everybody down a slot */
	for (i = 0; i < NLINES - 1; i++) {
		struct ncache *next;

		cp = &c_gid[IDX(gid, i)];
		next = &c_gid[IDX(gid, i + 1)];
		memcpy(next, cp, sizeof(*cp));
	}
	cp = &c_gid[IDX(gid, 0)];
	goto fillit;
}
