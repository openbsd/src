/*	$OpenBSD: pw_dup.c,v 1.5 2003/06/17 21:56:23 millert Exp $	*/

/*
 * Copyright (c) 2000, 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: pw_dup.c,v 1.5 2003/06/17 21:56:23 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct passwd *
pw_dup(const struct passwd *pw)
{
	char		*cp;
	size_t		 nsize, psize, csize, gsize, dsize, ssize, total;
	struct passwd	*newpw;

	/* Allocate in one big chunk for easy freeing */
	total = sizeof(struct passwd);
	if (pw->pw_name) {
		nsize = strlen(pw->pw_name) + 1;
		total += nsize;
	}
	if (pw->pw_passwd) {
		psize = strlen(pw->pw_passwd) + 1;
		total += psize;
	}
	if (pw->pw_class) {
		csize = strlen(pw->pw_class) + 1;
		total += csize;
	}
	if (pw->pw_gecos) {
		gsize = strlen(pw->pw_gecos) + 1;
		total += gsize;
	}
	if (pw->pw_dir) {
		dsize = strlen(pw->pw_dir) + 1;
		total += dsize;
	}
	if (pw->pw_shell) {
		ssize = strlen(pw->pw_shell) + 1;
		total += ssize;
	}
	if ((cp = malloc(total)) == NULL)
		return (NULL);
	newpw = (struct passwd *)cp;

	/*
	 * Copy in passwd contents and make strings relative to space
	 * at the end of the buffer.
	 */
	(void)memcpy(newpw, pw, sizeof(struct passwd));
	cp += sizeof(struct passwd);
	if (pw->pw_name) {
		(void)memcpy(cp, pw->pw_name, nsize);
		newpw->pw_name = cp;
		cp += nsize;
	}
	if (pw->pw_passwd) {
		(void)memcpy(cp, pw->pw_passwd, psize);
		newpw->pw_passwd = cp;
		cp += psize;
	}
	if (pw->pw_class) {
		(void)memcpy(cp, pw->pw_class, csize);
		newpw->pw_class = cp;
		cp += csize;
	}
	if (pw->pw_gecos) {
		(void)memcpy(cp, pw->pw_gecos, gsize);
		newpw->pw_gecos = cp;
		cp += gsize;
	}
	if (pw->pw_dir) {
		(void)memcpy(cp, pw->pw_dir, dsize);
		newpw->pw_dir = cp;
		cp += dsize;
	}
	if (pw->pw_shell) {
		(void)memcpy(cp, pw->pw_shell, ssize);
		newpw->pw_shell = cp;
		cp += ssize;
	}

	return (newpw);
}
