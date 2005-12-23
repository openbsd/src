/*	$OpenBSD: i2c_scan.c,v 1.10 2005/12/23 20:54:24 deraadt Exp $	*/

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

/* registers to load by default */
u_int8_t probereg[] = { 0x3d, 0x3e, 0x3f, 0xfe, 0xff, 0x4e, 0x4f };
#define P_3d	0
#define P_3e	1
#define P_3f	2
#define P_fe	3
#define P_ff	4
#define P_4e	5
#define P_4f	6
u_int8_t probeval[sizeof(probereg)/sizeof(probereg[0])];

/* additional registers to load later, for debugging... */
u_int8_t fprobereg[] = {
	0x00, 0x01, 0x02, 0x03, 0x07, 0x4c, 0x4d, 0x4e, 0x4f, 0x58
};
#define Pf_00	0
#define Pf_01	1
#define Pf_02	2
#define Pf_03	3
#define Pf_07	4
#define Pf_4c	5
#define Pf_4d	6
#define Pf_4e	7
#define Pf_4f	8
#define Pf_58	9
u_int8_t fprobeval[sizeof(fprobereg)/sizeof(fprobereg[0])];

u_int8_t wprobereg[] = { 0x4f };
#define PW_4f	0
u_int16_t wprobeval[sizeof(wprobereg)/sizeof(wprobereg[0])];

void
iic_probe(struct device *self, struct i2cbus_attach_args *iba, u_int8_t addr)
{
	struct i2c_attach_args ia;
	i2c_tag_t ic = iba->iba_tag;
	char *name = NULL;
	u_int8_t data;
	u_int16_t data2;
	int i, widetest = 0;

	/* Load registers used by many vendors as vendor/ID */
	ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
	for (i = 0; i < sizeof(probereg); i++) {
		probeval[i] = 0xff;
		if (ic->ic_exec(ic->ic_cookie,
		    I2C_OP_READ_WITH_STOP, addr,
		    &probereg[i], 1, &data, 1,
		    I2C_F_POLL) == 0)
			probeval[i] = data;
	}
	ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);

	if (probeval[P_3e] == 0x41) {
		/*
		 * Analog Devices adt/adm product code at 0x3e == 0x41.
		 * We probe newer to older.  newer chips have a valid 0x3d
		 * product number, while older ones encoded the product
		 * into the upper half of the step at 0x3f
		 */
		if (probeval[P_3d] == 0x03 || probeval[P_3d] == 0x08 ||
		    probeval[P_3d] == 0x07)
			name = "adt7516";	/* adt7517, adt7519 */
		if (probeval[P_3d] == 0x76)
			name = "adt7476";
		else if (probeval[P_3d] == 0x70)
			name = "adt7470";
		else if (probeval[P_3d] == 0x27)
			name = "adt7460";	/* adt746x */
		else if (probeval[P_3d] == 0x33)
			name = "adm1033";
		else if (probeval[P_3d] == 0x30)
			name = "adm1030";
		else if ((probeval[P_3f] & 0xf0) == 0x20)
			name = "adm1025";
		else if ((probeval[P_ff] & 0xf0) == 0x10)
			name = "adm1024";
		else if ((probeval[P_ff] & 0xf0) == 0x30)
			name = "adm1023";
		else if ((probeval[P_ff] & 0xf0) == 0x90)
			name = "adm1022";
		else
			name = "adm1021";	/* getting desperate.. */
	} else if (probeval[P_3e] == 0xa1) {
		/* Philips vendor code 0xa1 at 0x3e */
		if ((probeval[P_3f] & 0xf0) == 0x20)
			name = "ne1619";	/* adm1025 compat */
	} else if (probeval[P_3e] == 0x55) {
		if (probeval[P_3f] == 0x20)
			name = "47m192";	/* adm1025 compat */
	} else if (probeval[P_3e] == 0x01) {
		/*
		 * Most newer National products use a vendor code at
		 * 0x3e of 0x01, and then 0x3f contains a product code
		 * But some older products are missing a product code,
		 * and contain who knows what in that register.  We assume
		 * that some employee was smart enough to keep the numbers
		 * unique.
		 */
		if (probeval[P_3f] == 0x49)
			name = "lm99";
		else if (probeval[P_3f] == 0x73)
			name = "lm93";
		else if (probeval[P_3f] == 0x33)
			name = "lm90";
		else if (probeval[P_3f] == 0x52)
			name = "lm89";
		else if (probeval[P_3f] == 0x17)
			name = "lm86";
		else if (probeval[P_3f] == 0x03)	/* are there others? */
			name = "lm81";
	} else if (probeval[P_fe] == 0x01) {
		/* Some more National devices ...*/
		if (probeval[P_ff] == 0x33)
			name = "lm90";
	} else if (probeval[P_3e] == 0x02 && probeval[P_3f] == 0x6) {
		name = "lm87";
	} else if (probeval[P_fe] == 0x4d && probeval[P_ff] == 0x08) {
		name = "maxim6690";	/* somewhat similar to lm90 */
	} else if (probeval[P_4f] == 0x5c) {
		widetest = 1;
	} else if ((addr & 0xfc) == 0x48) {
		/* address for lm75/77 ... */
	}

	printf("%s: addr 0x%x", self->dv_xname, addr);
	for (i = 0; i < sizeof(probeval); i++)
		if (probeval[i] != 0xff)
			printf(" %02x=%02x", probereg[i], probeval[i]);

	if (name)
		goto gotname;

	printf(",");

	/* print out some more test register values.... */
	ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
	for (i = 0; i < sizeof(fprobereg); i++) {
		fprobeval[i] = 0xff;
		if (ic->ic_exec(ic->ic_cookie,
		    I2C_OP_READ_WITH_STOP, addr, &fprobereg[i],
		    1, &data, 1, I2C_F_POLL) == 0 &&
		    data != 0xff) {
			fprobeval[i] = data;
			printf(" %02x=%02x", fprobereg[i], data);
		}
	}
	ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);

	if (widetest) {
		printf(";");
		/* Load registers used by many vendors as vendor/ID */
		ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
		for (i = 0; i < sizeof(wprobereg)/sizeof(wprobereg[0]); i++) {
			wprobeval[i] = 0xff;
			if (ic->ic_exec(ic->ic_cookie,
			    I2C_OP_READ_WITH_STOP, addr,
			    &wprobereg[i], 1, &data2, 2,
			    I2C_F_POLL) == 0) {
				wprobeval[i] = data2;
				printf(" %02x=%04x", wprobereg[0], data2);
			}
		}
		ic->ic_release_bus(ic->ic_cookie, I2C_F_POLL);

		if (wprobeval[PW_4f] == 0x5ca3 && (probeval[P_4e] & 0x80)) {
			if (fprobeval[Pf_58] == 0x10)
				name = "w83781d";
			else if (fprobeval[Pf_58] == 0x30)
				name = "w83782d";
			else if (fprobeval[Pf_58] == 0x31)
				name = "as99127f";
		}
	}

gotname:
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
