/*
 * Copyright (c) 2003 Can Erkin Acar
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

#ifndef _PRIVSEP_H_
#define _PRIVSEP_H_

enum cmd_types {
	PRIV_BIND		/* bind to a privileged port */
};

/* Privilege separation */
int	isc_priv_init(int);
int	isc_drop_privs(const char *username);

struct sockaddr;
int	isc_priv_bind(int, struct sockaddr *, socklen_t);

/* File descriptor send/recv */
void	send_fd(int, int);
int	receive_fd(int);

/* communications over the channel */
int	may_read(int, void *, size_t);
void	must_read(int, void *, size_t);
void	must_write(int, const void *, size_t);

extern int priv_fd;

#endif
