/*	$OpenBSD: pw_dup.c,v 1.1 2000/11/21 00:49:58 millert Exp $	*/

/*
 * Copyright (c) 2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: pw_dup.c,v 1.1 2000/11/21 00:49:58 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>

struct passwd *
pw_dup(pw)
	const struct passwd *pw;
{
	struct passwd *newpw;
	char *cp;
	size_t siz;

	/* Allocate in one big chunk for easy freeing */
	siz = sizeof(struct passwd);
	if (pw->pw_name)
		siz += strlen(pw->pw_name) + 1;
	if (pw->pw_passwd)
		siz += strlen(pw->pw_passwd) + 1;
	if (pw->pw_class)
		siz += strlen(pw->pw_class) + 1;
	if (pw->pw_gecos)
		siz += strlen(pw->pw_gecos) + 1;
	if (pw->pw_dir)
		siz += strlen(pw->pw_dir) + 1;
	if (pw->pw_shell)
		siz += strlen(pw->pw_shell) + 1;
	if ((cp = malloc(siz)) == NULL)
		return(NULL);
	newpw = (struct passwd *)cp;

	/*
	 * Copy in passwd contents and make strings relative to space
	 * at the end of the buffer.
	 */
	memcpy(newpw, pw, sizeof(struct passwd));
	cp += sizeof(struct passwd);
	if (pw->pw_name) {
		siz = strlen(pw->pw_name) + 1;
		memcpy(cp, pw->pw_name, siz);
		newpw->pw_name = cp;
		cp += siz;
	}
	if (pw->pw_passwd) {
		siz = strlen(pw->pw_passwd) + 1;
		memcpy(cp, pw->pw_passwd, siz);
		newpw->pw_passwd = cp;
		cp += siz;
	}
	if (pw->pw_class) {
		siz = strlen(pw->pw_class) + 1;
		memcpy(cp, pw->pw_class, siz);
		newpw->pw_class = cp;
		cp += siz;
	}
	if (pw->pw_gecos) {
		siz = strlen(pw->pw_gecos) + 1;
		memcpy(cp, pw->pw_gecos, siz);
		newpw->pw_gecos = cp;
		cp += siz;
	}
	if (pw->pw_dir) {
		siz = strlen(pw->pw_dir) + 1;
		memcpy(cp, pw->pw_dir, siz);
		newpw->pw_dir = cp;
		cp += siz;
	}
	if (pw->pw_shell) {
		siz = strlen(pw->pw_shell) + 1;
		memcpy(cp, pw->pw_shell, siz);
		newpw->pw_shell = cp;
		cp += siz;
	}

	return(newpw);
}
