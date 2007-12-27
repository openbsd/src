/*	$OpenBSD: sysconvar.h,v 1.1 2007/12/27 23:17:53 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Logical values for non-VME interrupt sources on 188.
 */

#define	INTSRC_ABORT		1	/* abort button */
#define	INTSRC_ACFAIL		2	/* AC failure */
#define	INTSRC_SYSFAIL		3	/* system failure */
#define	INTSRC_CIO		4	/* Z8536 */
#define	INTSRC_DTIMER		5	/* MC68692 timer interrupt */
#define	INTSRC_DUART		6	/* MC68692 serial interrupt */
#define	INTSRC_VME		7	/* seven VME interrupt levels */

extern intrhand_t sysconintr_handlers[INTSRC_VME];

int	sysconintr_establish(u_int, struct intrhand *, const char *);
void	sysconintr_disestablish(u_int, struct intrhand *);

void	syscon_intsrc_enable(u_int, int);
void	syscon_intsrc_disable(u_int);
