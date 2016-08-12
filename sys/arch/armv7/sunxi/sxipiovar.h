/*	$OpenBSD: sxipiovar.h,v 1.4 2016/08/12 16:02:31 kettenis Exp $	*/
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

struct sxipio_func {
	const char *name;
	int mux;
};

struct sxipio_pin {
	const char *name;
	int port, pin;
	struct sxipio_func funcs[8];
};

#define SXIPIO_PORT_A	0
#define SXIPIO_PORT_B	1
#define SXIPIO_PORT_C	2
#define SXIPIO_PORT_D	3
#define SXIPIO_PORT_E	4
#define SXIPIO_PORT_F	5
#define SXIPIO_PORT_G	6
#define SXIPIO_PORT_H	7
#define SXIPIO_PORT_I	8

#define SXIPIO_PIN(port, pin) \
	"P" #port #pin,  SXIPIO_PORT_ ## port, pin

/*
 * XXX To keep things simple for now, functions below work as if there
 * is 32pins per port, this needs to be taken into account when writing
 * these pin defines.
 */

#define	SXIPIO_INPUT		0
#define	SXIPIO_OUTPUT		1

#define SXIPIO_USB1_PWR		230
#define SXIPIO_USB2_PWR		227
#define SXIPIO_SATA_PWR		40
#define	SXIPIO_EMAC_NPINS	18	/* PORTA 0-17 */

int sxipio_getcfg(int);
void sxipio_setcfg(int, int);
int sxipio_getpin(int);
void sxipio_setpin(int);
void sxipio_clrpin(int);
int sxipio_togglepin(int);

int sxipio_pinctrlbyid(int node, int id);
int sxipio_pinctrlbyname(int node, const char *);
