/*	$OpenBSD: i2c_scan.c,v 1.1 2005/12/19 19:36:46 grange Exp $	*/

/*
 * Copyright (c) 2005 Alexander Yurchenko <grange@openbsd.org>
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

/*
 * I2C bus scannig.
 */

#include <sys/param.h>
#include <sys/systm.h>

#define _I2C_PRIVATE
#include <dev/i2c/i2cvar.h>

/* Device signatures */
static const struct {
	const char *	name;
	i2c_addr_t	addr;
	u_int8_t	offset;
	u_int8_t	value;
} iicsig[] = {
	{ "foo",	0xff, 0xff, 0xff }
};

void
iic_scan(struct device *self, struct i2cbus_attach_args *iba)
{
	i2c_tag_t ic = iba->iba_tag;
	u_int8_t cmd = 0, addr, data;
	int i;

	/* Do full scan */
	printf("%s: full scan:", self->dv_xname);
	for (addr = 0; addr < 0x80; addr++) {
		ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
		if (ic->ic_exec(ic->ic_cookie, I2C_OP_READ_WITH_STOP, addr,
		    &cmd, 1, NULL, 0, I2C_F_POLL) == 0)
			printf(" 0x%x", addr);
		ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);
	}
	printf("\n");

	/* Scan only for know signatures */
	printf("%s: sign scan:", self->dv_xname);
	for (i = 0; i < sizeof(iicsig) / sizeof(iicsig[0]); i++) {
		addr = iicsig[i].addr;
		cmd = iicsig[i].offset;

		ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
		if (ic->ic_exec(ic->ic_cookie, I2C_OP_READ_WITH_STOP, addr,
		    &cmd, 1, &data, 1, I2C_F_POLL) == 0) {
			if (iicsig[i].value == 0xff ||
			    iicsig[i].value == data)
				printf(" %s", iicsig[i].name);
		}
		ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);
	}
	printf("\n");
}
