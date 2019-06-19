/*	$OpenBSD: captiveportal.h,v 1.1 2019/02/03 12:02:30 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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


enum captive_portal_state {
	PORTAL_UNCHECKED,
	PORTAL_UNKNOWN,
	BEHIND,
	NOT_BEHIND
};

static const char * const	captive_portal_state_str[] = {
	"unchecked",
	"unknown",
	"behind",
	"not behind"
};

void	 captiveportal(int, int);
void	 captiveportal_dispatch_main(int, short, void *);
void	 captiveportal_dispatch_resolver(int, short, void *);
void	 captiveportal_dispatch_frontend(int, short, void *);
int	 captiveportal_imsg_compose_main(int, pid_t, void *, uint16_t);
int	 captiveportal_imsg_compose_resolver(int, pid_t, void *, uint16_t);
int	 captiveportal_imsg_compose_frontend(int, pid_t, void *, uint16_t);
