/*	$OpenBSD: awpiovar.h,v 1.1 2013/10/22 13:22:19 jasper Exp $	*/
/*
 * Copyright (c) 2013 Artturi Alm
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

#include <sys/gpio.h>

/*
 * XXX To keep things simple for now, functions below work as if there
 * is 32pins per port, this needs to be taken into account when writing
 * these pin defines.
 */

#define	AWPIO_INPUT		0
#define	AWPIO_OUTPUT		1

#define AWPIO_USB1_PWR		230
#define AWPIO_USB2_PWR		227
#define AWPIO_LED_GREEN		244
#define AWPIO_LED_BLUE		245
#define AWPIO_SATA_PWR		40
#define	AWPIO_EMAC_NPINS	18	/* PORTA 0-17 */

int awpio_getcfg(int);
void awpio_setcfg(int, int);
int awpio_getpin(int);
void awpio_setpin(int);
void awpio_clrpin(int);
int awpio_togglepin(int);
