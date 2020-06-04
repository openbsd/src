/*	$OpenBSD: opal.h,v 1.2 2020/05/27 22:22:04 gkoehler Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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

#ifndef _MACHINE_OPAL_H_
#define _MACHINE_OPAL_H_

#define OPAL_TEST		0
#define OPAL_CONSOLE_WRITE	1
#define OPAL_CONSOLE_READ	2
#define OPAL_CEC_POWER_DOWN	5
#define OPAL_CEC_REBOOT		6
#define OPAL_POLL_EVENTS	10

#ifndef _LOCORE
int64_t	opal_test(uint64_t);
int64_t	opal_console_write(int64_t, int64_t *, const uint8_t *);
int64_t	opal_console_read(int64_t, int64_t *, uint8_t *);
int64_t	opal_cec_power_down(uint64_t);
int64_t	opal_cec_reboot(void);
int64_t	opal_poll_events(uint64_t *);

void	opal_printf(const char *fmt, ...);
#endif

#endif /* _MACHINE_OPAL_H_ */
