/*	$OpenBSD: i2c_scan.c,v 1.33 2005/12/28 23:05:38 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Theo de Raadt <deraadt@openbsd.org>
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

#define I2C_DEBUG

void	iic_probe(struct device *, struct i2cbus_attach_args *, u_int8_t);

/* addresses at which to probe for sensors */
struct {
	u_int8_t start, end;
} probe_addrs[] = {
	{ 0x18, 0x18 },
	{ 0x1a, 0x1a },
	{ 0x20, 0x2f },
	{ 0x48, 0x4f }
};

#define MAX_IGNORE 8
u_int8_t ignore_addrs[MAX_IGNORE];

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
static u_int16_t probew(u_int8_t);
static int	lm75probe(void);

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
	if (iic_exec(probe_ic, I2C_OP_READ_WITH_STOP,
	    probe_addr, &cmd, 1, &data, 1, I2C_F_POLL) != 0)
		data = 0xff;
	probe_ic->ic_release_bus(probe_ic->ic_cookie, I2C_F_POLL);
	return (data);
}

static u_int16_t
probew(u_int8_t cmd)
{
	u_int16_t data2;

	probe_ic->ic_acquire_bus(probe_ic->ic_cookie, I2C_F_POLL);
	if (iic_exec(probe_ic, I2C_OP_READ_WITH_STOP,
	    probe_addr, &cmd, 1, &data2, 2, I2C_F_POLL) != 0)
		data2 = 0xffff;
	probe_ic->ic_release_bus(probe_ic->ic_cookie, I2C_F_POLL);
	return (data2);
}

static u_int8_t
probe(u_int8_t cmd)
{
	if (probe_val[cmd] != 0xff)
		return probe_val[cmd];
	probe_val[cmd] = probenc(cmd);
	return (probe_val[cmd]);
}

/*
 * 0x06 and 0x07 return whatever value was read before, and the
 * chip loops every 8 registers.
 */
static int
lm75probe(void)
{
	u_int16_t mains[6];
	u_int8_t main;
	int i;

	main = probenc(0x01);
	mains[0] = probew(0x02);
	mains[1] = probew(0x03);

	mains[2] = probew(0x04);	/* read Low Limit */
	if (probew(0x07) != mains[2] || probew(0x07) != mains[2])
		return (0);

	mains[3] = probew(0x05);	/* read High limit */
	mains[4] = probew(0x06);
	mains[5] = probew(0x07);
	if (mains[4] != mains[3] || mains[5] != mains[3])
		return (0);

#ifdef I2C_DEBUG
	printf("lm75probe: %02x %04x %04x %04x %04x %04x %04x\n", main,
	    mains[0], mains[1], mains[2], mains[3], mains[4], mains[5]);
#endif /* I2C_DEBUG */

	/* a real lm75/77 repeats it's registers.... */
	for (i = 0x08; i < 0xff; i += 8) {
		if (main != probenc(0x01 + i) ||
		    mains[0] != probew(0x02 + i) ||
		    mains[1] != probew(0x03 + i) ||
		    mains[2] != probew(0x04 + i) ||
		    mains[3] != probew(0x05 + i) ||
		    mains[4] != probew(0x06 + i) ||
		    mains[5] != probew(0x07 + i))
			return (0);
	}

	/* We hope */
	return (1);
}

#ifdef __i386__
static char 	*xeonprobe(u_int8_t);

static char *
xeonprobe(u_int8_t addr)
{
	if (addr == 0x18 || addr == 0x1a || addr == 0x29 ||
	    addr == 0x2b || addr == 0x4c || addr == 0x4e) {
		u_int8_t reg, val;
		int zero = 0, copy = 0;

		val = probe(0x00);
		for (reg = 0x00; reg < 0x09; reg++) {
			if (probe(reg) == 0xff)
				return (NULL);
			if (probe(reg) == 0x00)
				zero++;
			if (val == probe(reg))
				copy++;
		}
		if (zero > 6 || copy > 6)
			return (NULL);
		val = probe(0x09);
		for (reg = 0x0a; reg < 0xfe; reg++) {
			if (probe(reg) != val)
				return (NULL);
		}
		/* 0xfe may be maxim, or some other vendor */
		if (probe(0xfe) == 0x4d)
			return ("maxim1617");
		return ("xeontemp");
	}
	return (NULL);
}	
#endif

void
iic_ignore_addr(u_int8_t addr)
{
	int i;

	for (i = 0; i < sizeof(ignore_addrs); i++)
		if (ignore_addrs[i] == 0) {
			ignore_addrs[i] = addr;
			return;
		}
}

void
iic_probe(struct device *self, struct i2cbus_attach_args *iba, u_int8_t addr)
{
	struct i2c_attach_args ia;
	char *name = NULL;
	int i;

	for (i = 0; i < sizeof(ignore_addrs); i++)
		if (ignore_addrs[i] == addr)
			return;

	probeinit(iba, addr);

	switch (probe(0x3e)) {
	case 0x41:
		/*
		 * Analog Devices adt/adm product code at 0x3e == 0x41.
		 * Newer chips have a valid 0x3d product number, while
		 * older ones sometimes encoded the product into the
		 * upper half of the "step register" at 0x3f
		 */
		if (probe(0x3d) == 0x76 &&
		    (addr == 0x2c || addr == 0x2d || addr == 0x2e))
			name = "adt7476";
		else if ((addr == 0x2c || addr == 0x2e || addr == 0x2f) &&
		    probe(0x3d) == 0x70)
			name = "adt7470";
		else if (probe(0x3d) == 0x27 &&
		    (probe(0x3f) == 0x60 || probe(0x3f) == 0x6a))
			name = "adm1027";	/* complete check */
		else if (probe(0x3d) == 0x27 &&
		    (probe(0x3f) == 0x62 || probe(0x3f) == 0x6a))
			name = "adt7460";	/* complete check */
		else if (probe(0x3d) == 0x68 && addr == 0x2e &&
		    (probe(0x3f) & 0xf0) == 0x70)
			name = "adt7467";
		else if (probe(0x3d) == 0x33)
			name = "adm1033";
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    probe(0x3d) == 0x30 &&
		    (probe(0x3f) & 0x70) == 0x00 &&
		    (probe(0x01) & 0x4a) == 0x00 &&
		    (probe(0x03) & 0x3f) == 0x00 &&
		    (probe(0x22) & 0xf0) == 0x00 &&
		    (probe(0x0d) & 0x70) == 0x00 &&
		    (probe(0x0e) & 0x70) == 0x00)
			name = "adm1030";	/* complete check */
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    probe(0x3d) == 0x31 &&
		    (probe(0x03) & 0x3f) == 0x00 &&
		    (probe(0x0d) & 0x70) == 0x00 &&
		    (probe(0x0e) & 0x70) == 0x00 &&
		    (probe(0x0f) & 0x70) == 0x00)
			name = "adm1031";	/* complete check */
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    (probe(0x3f) & 0xf0) == 0x20 &&
		    (probe(0x40) & 0x80) == 0x00 &&
		    (probe(0x41) & 0xc0) == 0x00 &&
		    (probe(0x42) & 0xbc) == 0x00)
			name = "adm1025";	/* complete check */
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    (probe(0x3f) & 0xf0) == 0x10 &&
		    (probe(0x40) & 0x80) == 0x00)
			name = "adm1024";	/* complete check */
		else if ((probe(0xff) & 0xf0) == 0x30)
			name = "adm1023";
		else if ((probe(0x3f) & 0xf0) == 0xd0 && addr == 0x2e &&
		    (probe(0x40) & 0x80) == 0x00)
			name = "adm1028";	/* adm1022 clone? */
		else if ((probe(0x3f) & 0xf0) == 0xc0 &&
		    (addr == 0x2c || addr == 0x2e || addr == 0x2f) &&
		    (probe(0x40) & 0x80) == 0x00)
			name = "adm1022";
		break;
	case 0xa1:
		/* Philips vendor code 0xa1 at 0x3e */
		if ((probe(0x3f) & 0xf0) == 0x20 &&
		    (probe(0x40) & 0x80) == 0x00 &&
		    (probe(0x41) & 0xc0) == 0x00 &&
		    (probe(0x42) & 0xbc) == 0x00)
			name = "ne1619";	/* adm1025 compat */
		break;
	case 0x23:	/* 2nd ADM id? */
		if (probe(0x48) == addr &&
		    (probe(0x40) & 0x80) == 0x00 &&
		    (addr & 0x7c) == 0x2c)
			name = "adm9240";	/* lm87 clone */
		break;
	case 0x55:
		if (probe(0x3f) == 0x20 && (probe(0x47) & 0x70) == 0x00 &&
		    (probe(0x49) & 0xfe) == 0x80 &&
		    (addr & 0x7c) == 0x2c)
			name = "47m192";	/* adm1025 compat */
		break;
	case 0x01:
		/*
		 * Some newer National products use a vendor code at
		 * 0x3e of 0x01, and then 0x3f contains a product code
		 * But some older products are missing a product code,
		 * and contain who knows what in that register.  We assume
		 * that some employee was smart enough to keep the numbers
		 * unique.
		 */
		if (probe(0x3f) == 0x52 && probe(0xff) == 0x01 &&
		    (probe(0xfe) == 0x4c || probe(0xfe) == 0x4d))
			name = "lm89";		/* lm89 "alike" */
		else if (probe(0x3f) == 0x33 && probe(0xff) == 0x01 &&
		    probe(0xfe) == 0x21)
			name = "lm90";		/* lm90 "alike" */
		else if (probe(0x3f) == 0x49 && probe(0xff) == 0x01 &&
		    (probe(0xfe) == 0x31 || probe(0xfe) == 0x34))
			name = "lm99";		/* lm99 "alike" */
		else if (probe(0x3f) == 0x73)
			name = "lm93";
		else if (probe(0x3f) == 0x17)
			name = "lm86";
		else if ((probe(0x3f) & 0xf0) == 0x60 &&
		    (addr == 0x2c || addr == 0x2d || addr == 0x2e))
			name = "lm85";		/* adt7460 compat */
		else if (probe(0x3f) == 0x03 && probe(0x48) == addr &&
		    ((probe(0x40) & 0x80) == 0x00) && ((addr & 0x7c) == 0x2c))
			name = "lm81";
		break;
	case 0x49:	/* TI */
		if ((probe(0x3f) & 0xf0) == 0xc0 &&
		    (addr == 0x2c || addr == 0x2e || addr == 0x2f) &&
		    (probe(0x40) & 0x80) == 0x00)
			name = "thmc50";	/* adm1022 clone */
		break;
	case 0x5c:	/* SMSC */
		if ((probe(0x3f) & 0xf0) == 0x60 &&
		    (addr == 0x2c || addr == 0x2d || addr == 0x2e))
			name = "emc6d10x";	/* adt7460 compat */
		break;
	case 0x02:
		if ((probe(0x3f) & 0xfc) == 0x04)
			name = "lm87";		/* complete check */
		break;
	case 0xda:
		if (probe(0x3f) == 0x01 && probe(0x48) == addr &&
		    (probe(0x40) & 0x80) == 0x00)
			name = "ds1780";	/* lm87 clones */
		break;
	}
	switch (probe(0x4e)) {
	case 0x41:
		if ((addr == 0x48 || addr == 0x4a || addr == 0x4b) &&
		    /* addr 0b1001{000, 010, 011} */
		    (probe(0x4d) == 0x03 || probe(0x4d) == 0x08 ||
		    probe(0x4d) == 0x07))
			name = "adt7516";	/* adt7517, adt7519 */
		break;
	}

	if (probe(0xfe) == 0x01) {
		/* Some more National devices ...*/
		if (probe(0xff) == 0x21 && (probe(0x03) & 0x2a) == 0 &&
		    probe(0x04) <= 0x09 && probe(0xff))
			name = "lm90";		/* complete check */
		else if (probe(0xff) == 0x31 && addr == 0x4c &&
		    (probe(0x03) & 0x2a) == 0 && probe(0x04) <= 0x09)
			name = "lm99";
		else if (probe(0xff) == 0x34 && addr == 0x4d &&
		    (probe(0x03) & 0x2a) == 0 && probe(0x04) <= 0x09)
			name = "lm99-1";
		else if (probe(0xff) == 0x11 && 
		    (probe(0x03) & 0x2a) == 0 && probe(0x04) <= 0x09)
			name = "lm86";
	} else if (probe(0xfe) == 0x4d && probe(0xff) == 0x08) {
		name = "maxim6690";	/* somewhat similar to lm90 */
	} else if (probe(0xfe) == 0x41 && (addr == 0x4c || addr == 0x4d) &&
	    (probe(0x03) & 0x2a) == 0 && probe(0x04) <= 0x09) {
		name = "adm1032";
	} else if (probe(0xfe) == 0x41 && probe(0x3c) == 0x00 &&
	    (addr == 0x18 || addr == 0x19 || addr == 0x1a ||
	    addr == 0x29 || addr == 0x2a || addr == 0x2b ||
	    addr == 0x4c || addr == 0x4d || addr == 0x4e)) {
		name = "adm1021";	/* lots of addresses... bleah */
	} else if (probe(0x4f) == 0x5c && (probe(0x4e) & 0x80)) {
		/*
		 * We should toggle 0x4e bit 0x80, then re-read
		 * 0x4f to see if it is 0xa3 (for Winbond)
		 */
		if (probe(0x58) == 0x31)
			name = "as99127f";
	} else if (probe(0x16) == 0x41 && ((probe(0x17) & 0xf0) == 0x40) &&
	    (addr == 0x2c || addr == 0x2d || addr == 0x2e)) {
		name = "adm1026";
	} else if ((addr & 0xfc) == 0x48 && lm75probe()) {
		name = "lm75";
#ifdef __i386__
	} else if (name == NULL) {
		name = xeonprobe(addr);
#endif
	}

#ifdef I2C_DEBUG
	printf("%s: addr 0x%x", self->dv_xname, addr);
//	for (i = 0; i < sizeof(probereg); i++)
//		if (probe(probereg[i]) != 0xff)
//			printf(" %02x=%02x", probereg[i], probe(probereg[i]));
	for (i = 0; i <= 0xff; i++)
		if (probe(i) != 0xff)
			printf(" %02x=%02x", i, probe(i));
	if (name)
		printf(": %s", name);
	printf("\n");
#endif /* I2C_DEBUG */

	if (name) {
		ia.ia_tag = iba->iba_tag;
		ia.ia_addr = addr;
		ia.ia_size = 1;
		ia.ia_name = name;
		config_found(self, &ia, iic_print);
	}
}

void
iic_scan(struct device *self, struct i2cbus_attach_args *iba)
{
	i2c_tag_t ic = iba->iba_tag;
	u_int8_t cmd = 0, addr;
	int i;

	bzero(ignore_addrs, sizeof(ignore_addrs));
	for (i = 0; i < sizeof(probe_addrs)/sizeof(probe_addrs[0]); i++) {
		for (addr = probe_addrs[i].start; addr <= probe_addrs[i].end;
		    addr++) {
			/* Perform RECEIVE BYTE command */
			ic->ic_acquire_bus(ic->ic_cookie, I2C_F_POLL);
			if (iic_exec(ic, I2C_OP_READ_WITH_STOP, addr,
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
