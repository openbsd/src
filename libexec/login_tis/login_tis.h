/*	$OpenBSD: login_tis.h,v 1.1 2004/09/28 15:02:01 millert Exp $	*/

/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
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

#define	TIS_BUFSIZ		512	/* max size of authsrv reply */
					/* XXX - only 128 for most */

#define	TIS_PASSWD_TIMEOUT	120	/* password prompt timeout */

/* default values for login.conf variables */
#define	TIS_DEFPORT	"7777"		/* default port to use */
#define	TIS_DEFSERVER	"localhost"	/* default server to contact */
#define	TIS_DEFTIMEOUT	15		/* default communications timeout */

struct tis_connection {
	int fd;
	int timeout;
	char *keyfile;
	char *port;
	char *server;
	char *altserver;
	des_key_schedule keysched;
};
