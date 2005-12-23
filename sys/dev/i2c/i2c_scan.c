/*	$OpenBSD: i2c_scan.c,v 1.12 2005/12/23 22:56:44 deraadt Exp $	*/

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
 * I2C bus scanning.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#define _I2C_PRIVATE
#include <dev/i2c/i2cvar.h>

void	iic_probe(struct device *, struct i2cbus_attach_args *, u_int8_t);

/*
 * some basic rules for finding devices...
 * 
 * 0x7 == 0x0001			national lm92
 * 0xfe == 0x4d			maxim (6659/6658/6659) == lm90
 * no 0x7 register??		maxim (6633/6634/6635) lm92
 * 
 * XXX remove this block of text later	
 * 0x00	National (on LM84)
 * 0x01	National
 * 0x12C3	Asus (at 0x4F)
 * 0x23	Analog Devices
 * 0x41	Analog Devices (also at 0x16)
 * 0x49	TI
 * 0x4D	Maxim
 * 0x54	On Semi
 * 0x5C	SMSC
 * 0x5D 	SMSC
 * 0x55	SMSC
 * 0x5CA3	Winbond (at 0x4F)
 * 0x90	ITE (at 0x58)
 * 0xA1	Philips (at 0x05 too)
 * 0xA3	Winbond (at 0x4F)
 * 0xAC	Myson (at 0x58)
 * 0xC3	Asus (at 0x4F)
 * 0xDA	Dallas
 * 0x54    Microchip (at 0x7)
 */

/* addresses at which to probe for sensors */
struct {
	u_int8_t start, end;
} probe_addrs[] = {
	{ 0x20, 0x2f },
	{ 0x48, 0x4f }
};

/* registers to print if we fail to probe */
u_int8_t probereg[] = {
	0x00, 0x01, 0x02, 0x03, 0x07,
	0x3d, 0x3e, 0x3f,
	0x4c, 0x4d, 0x4e, 0x4f,
	0x58, 0xfe, 0xff
};

static i2c_tag_t probe_ic;
static u_int8_t probe_addr;
static u_int8_t probe_val[256];

static void	probeinit(struct i2cbus_attach_args *, u_int8_t);
static u_int8_t	probenc(u_int8_t);
static u_int8_t	probe(u_int8_t);

static void
probeinit(struct i2cbus_attach_args *iba, u_int8_t addr)
{
	probe_ic = iba->iba_tag;
	probe_addr = addr;
	memset(probe_val, 0xff, sizeof probe_val);
}

static u_int8_t
probenc(u_int8_t cmd)
{
	u_int8_t data;

	probe_ic->ic_acquire_bus(probe_ic->ic_cookie, I2C_F_POLL);
	if (probe_ic->ic_exec(probe_ic->ic_cookie, I2C_OP_READ_WITH_STOP,
	    probe_addr, &cmd, 1, &data, 1, I2C_F_POLL) != 0)
		data = 0xff;
	probe_ic->ic_release_bus(probe_ic->ic_cookie, I2C_F_POLL);
	return (data);
}

static u_int8_t
probe(u_int8_t cmd)
{
	if (probe_val[cmd] != 0xff)
		return probe_val[cmd];
	probe_val[cmd] = probenc(cmd);
	return (probe_val[cmd]);
}

void
iic_probe(struct device *self, struct i2cbus_attach_args *iba, u_int8_t addr)
{
	struct i2c_attach_args ia;
	char *name = NULL;
	int i;

	probeinit(iba, addr);

	if (probe(0x3e) == 0x41) {
		/*
		 * Analog Devices adt/adm product code at 0x3e == 0x41.
		 * We probe newer to older.  newer chips have a valid 0x3d
		 * product number, while older ones encoded the product
		 * into the upper half of the step at 0x3f
		 */
		if (probe(0x3d) == 0x03 || probe(0x3d) == 0x08 ||
		    probe(0x3d) == 0x07)
			name = "adt7516";	/* adt7517, adt7519 */
		if (probe(0x3d) == 0x76)
			name = "adt7476";
		else if (probe(0x3d) == 0x70)
			name = "adt7470";
		else if (probe(0x3d) == 0x27)
			name = "adt7460";	/* adt746x */
		else if (probe(0x3d) == 0x33)
			name = "adm1033";
		else if (probe(0x3d) == 0x30)
			name = "adm1030";
		else if ((probe(0x3f) & 0xf0) == 0x20)
			name = "adm1025";
		else if ((probe(0xff) & 0xf0) == 0x10)
			name = "adm1024";
		else if ((probe(0xff) & 0xf0) == 0x30)
			name = "adm1023";
		else if ((probe(0xff) & 0xf0) == 0x90)
			name = "adm1022";
		else
			name = "adm1021";	/* getting desperate.. */
	} else if (probe(0x3e) == 0xa1) {
		/* Philips vendor code 0xa1 at 0x3e */
		if ((probe(0x3f) & 0xf0) == 0x20)
			name = "ne1619";	/* adm1025 compat */
	} else if (probe(0x3e) == 0x55) {
		if (probe(0x3f) == 0x20)
			name = "47m192";	/* adm1025 compat */
	} else if (probe(0x3e) == 0x01) {
		/*
		 * Most newer National products use a vendor code at
		 * 0x3e of 0x01, and then 0x3f contains a product code
		 * But some older products are missing a product code,
		 * and contain who knows what in that register.  We assume
		 * that some employee was smart enough to keep the numbers
		 * unique.
		 */
		if (probe(0x3f) == 0x49)
			name = "lm99";
		else if (probe(0x3f) == 0x73)
			name = "lm93";
		else if (probe(0x3f) == 0x33)
			name = "lm90";
		else if (probe(0x3f) == 0x52)
			name = "lm89";
		else if (probe(0x3f) == 0x17)
			name = "lm86";
		else if (probe(0x3f) == 0x03)	/* are there others? */
			name = "lm81";
	} else if (probe(0xfe) == 0x01) {
		/* Some more National devices ...*/
		if (probe(0xff) == 0x33)
			name = "lm90";
	} else if (probe(0x3e) == 0x02 && probe(0x3f) == 0x6) {
		name = "lm87";
	} else if (probe(0xfe) == 0x4d && probe(0xff) == 0x08) {
		name = "maxim6690";	/* somewhat similar to lm90 */
	} else if ((addr & 0xfc) == 0x48) {
		/* address for lm75/77 ... */
	} else if (probe(0x4f) == 0x5c && (probe(0x4e) & 0x80)) {
		/*
		 * We should toggle 0x4e bit 0x80, then re-read
		 * 0x4f to see if it is 0xa3 (for Winbond)
		 */
		if (probe(0x58) == 0x31)
			name = "as99127f";
	}

	printf("%s: addr 0x%x", self->dv_xname, addr);
	for (i = 0; i < sizeof(probereg); i++)
		if (probe(probereg[i]) != 0xff)
			printf(" %02x=%02x", probereg[i], probe(probereg[i]));
	if (name)
		printf(": %s", name);
	printf("\n");

	if (name) {
		ia.ia_tag = iba->iba_tag;
		ia.ia_addr = addr;
		ia.ia_size = 1;
		ia.ia_name = name;
		ia.ia_compat = name;
		config_found(self, &ia, iic_print);
	}
}

void
iic_scan(struct device *self, struct i2cbus_attach_args *iba)
{
	i2c_tag_t ic = iba->iba_tag;
	u_int8_t cmd = 0, addr;
	int i;

	for (i = 0; i < sizeof(probe_addrs)/sizeof(probe_addrs[0]); i++) {
		for (addr = probe_addrs[i].start; addr <= probe_addrs[i].end;
		    addr++) {
			/* Perform RECEIVE BYTE command */
			ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
			if (ic->ic_exec(ic->ic_cookie, I2C_OP_READ_WITH_STOP, addr,
			    &cmd, 1, NULL, 0, I2C_F_POLL) == 0) {
				ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);

				/* Some device exists, so go scope it out */
				iic_probe(self, iba, addr);

				ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);

			}
			ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);
		}
	}
}
