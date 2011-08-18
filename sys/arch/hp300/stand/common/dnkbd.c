/*	$OpenBSD: dnkbd.c,v 1.5 2011/08/18 19:54:19 miod Exp $	*/
/*	$NetBSD: dnkbd.c,v 1.3 1997/05/12 07:47:03 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Smith and Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Apollo Domain keyboard routines for the standalone ITE.
 */

#if defined(ITECONSOLE) && defined(DOMAIN_KEYBOARD)

#include <sys/param.h>

#include <hp300/dev/frodoreg.h>		/* for apci offsets */
#include <hp300/dev/dcareg.h>		/* for the register bit definitions */
#include <hp300/dev/apcireg.h>		/* for the apci registers */

#include "samachdep.h"
#include "kbdvar.h"

#ifndef SMALL

/*
 * The Apollo keyboard is used in `cooked' mode as configured by the
 * firmware; only one table is required.
 *
 * Note that if an entry in this table is set to 0, the key is passed
 * through untranslated.  If the entry is 0xff, the key is ignored.
 */
u_char dnkbd_keymap[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 07 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 17 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 27 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 2f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 37 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 3f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 47 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 4f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 57 */
	0x00, 0x00, 0x00, '{',  0x00, '}',  0x00, 0x00, /* 5f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 67 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 6f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 77 */
	0x00, 0x00, 0x00, '[',  0x00, ']',  0x00, 0x00, /* 7f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 87 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 8f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 97 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 9f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* a7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* af */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* b7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* bf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* c7 */
	'\\', '|',  0x09, 0x0a, '/',  0xff, 0xff, 0xff, /* cf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* d7 */
	0xff, 0xff, 0xff, 0xff, '?',  0xff, 0x08, 0xff, /* df */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* e7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* ef */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* f7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* ff */
};

int	dnkbd_ignore;		/* for ignoring mouse packets */

int
dnkbd_getc()
{
	struct apciregs *apci =
	    (struct apciregs *)IIOV(FRODO_BASE + FRODO_APCI_OFFSET(0));
	int c;

	/* default to `no key' */
	c = 0;

	/* Is data in the UART? */
	if (apci->ap_lsr & LSR_RXRDY) {
		/* Get the character. */
		c = apci->ap_data;

		/* Ignoring mouse? */
		if (dnkbd_ignore) {
			dnkbd_ignore--;
			return (0);
		}

		/* Is this the start of a mouse packet? */
		if (c == 0xdf) {
			dnkbd_ignore = 3;	/* 3 bytes of junk */
			return (0);
		}

		/* It's a keyboard event. */
		switch (dnkbd_keymap[c]) {
		case 0x00:
			/* passthrough */
			break;

		case 0xff:
			/* ignore */
			c = 0;
			break;

		default:
			c = dnkbd_keymap[c];
			break;
		}
	}

	return (c);
}
#endif /* SMALL */

void
dnkbd_nmi()
{

	/*
	 * XXX Should we do anything?  Can we even generate one?
	 */
}

int
dnkbd_init()
{

	/*
	 * 400, 425, and 433 models can have a Domain keyboard.
	 */
	switch (machineid) {
	case HP_400:
	case HP_425:
	case HP_433:
		break;
	default:
		return (0);
	}

	/*
	 * Look for a Frodo utility chip.  If we find one, assume there
	 * is a Domain keyboard attached.
	 */
	if (badaddr((caddr_t)IIOV(FRODO_BASE + FRODO_APCI_OFFSET(0))))
		return (0);

	/*
	 * XXX Any other initialization?  This appears to work ok.
	 */
	return (1);
}
#endif /* ITECONSOLE && DOMAIN_KEYBOARD */
