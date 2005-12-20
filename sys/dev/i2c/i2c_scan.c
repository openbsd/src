/*	$OpenBSD: i2c_scan.c,v 1.2 2005/12/20 05:00:47 deraadt Exp $	*/

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
#include <sys/device.h>

#define _I2C_PRIVATE
#include <dev/i2c/i2cvar.h>

/*
some basic devices...

 0x7 == 0x0001			national lm92
 0x3e == 0x01			national product
	0x3f == 0x73		lm93
 0x3e == 0x02			
	0x3f == 0x06		lm87 
 0xfe == 0x01			national product
	0xff == 0x33		lm90
	0xff == 0x49 0x52	lm99 lm89
	0xff == 0x17		lm86 
	0xff == ??		lm83?
 cannot identify		lm81 lm80
 0xfe == 0x4d			maxim (6659/6658/6659) == lm90
 no 0x7 register??		maxim (6633/6634/6635) lm92

XXX remove this block of text later	
0x00	National (on LM84)
0x01	National
0x12C3	Asus (at 0x4F)
0x23	Analog Devices
0x41	Analog Devices (also at 0x16)
0x49	TI
0x4D	Maxim
0x54	On Semi
0x5C	SMSC
0x5D 	SMSC
0x55	SMSC
0x5CA3	Winbond (at 0x4F)
0x90	ITE (at 0x58)
0xA1	Philips (at 0x05 too)
0xA3	Winbond (at 0x4F)
0xAC	Myson (at 0x58)
0xC3	Asus (at 0x4F)
0xDA	Dallas
0x54    Microchip (at 0x7)

 */

struct {
	u_int8_t start, end;
} probe_paddrs[] = {
	{ 0x20, 0x2f },
	{ 0x48, 0x4f },
	{ 0xad, 0xad }
};

u_int8_t probe[] = { 0x3e, 0x3f, 0xfe, 0xff, 0x4f, 0x58, 0x07 };

void
iic_scan(struct device *self, struct i2cbus_attach_args *iba)
{
	i2c_tag_t ic = iba->iba_tag;
	u_int8_t cmd = 0, addr, data;
	int i, j;

	for (j = 0; j < sizeof(probe_paddrs)/sizeof(probe_paddrs[0]); j++) {
		for (addr = probe_paddrs[j].start; addr <= probe_paddrs[j].end;
		    addr++) {
			/* Perform RECEIVE BYTE command */
			ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
			if (ic->ic_exec(ic->ic_cookie, I2C_OP_READ_WITH_STOP, addr,
			    &cmd, 1, NULL, 0, I2C_F_POLL) == 0) {
				printf("addr 0x%x at %s: ", addr, self->dv_xname);
				for (i = 0; i < sizeof(probe); i++) {
					cmd = probe[i];
					if (ic->ic_exec(ic->ic_cookie,
					    I2C_OP_READ_WITH_STOP, addr,
					    &cmd, 1, &data, 1, I2C_F_POLL) == 0 &&
					    data != 0xff)
						printf(" %02x=%02x", cmd, data);
				}
				printf("\n");
			}
			ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);
		}
	}
}
