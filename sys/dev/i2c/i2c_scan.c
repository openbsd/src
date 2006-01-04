/*	$OpenBSD: i2c_scan.c,v 1.50 2006/01/04 18:36:22 deraadt Exp $	*/

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

#undef I2C_DEBUG
#define I2C_VERBOSE

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

/*
 * Some Maxim 1617 clones MAY NOT even read cmd 0xfc!  When it is
 * read, they will power-on-reset.  Their default condition
 * (control register bit 0x80) therefore will be that they assert
 * /ALERT for the 5 potential errors that may occur.  One of those
 * errors is that the external temperature diode is missing.  This
 * is unfortunately a common choice of system designers, except
 * suddenly now we get a /ALERT, which may on some chipsets cause
 * us to receive an entirely unexpected SMI .. and then an NMI.
 *
 * As we probe each device, if we hit something which looks suspiciously
 * like it may potentially be a 1617 or clone, we immediately set this
 * variable to avoid reading that register offset.
 */
int	skip_fc;

static i2c_tag_t probe_ic;
static u_int8_t probe_addr;
static u_int8_t probe_val[256];

void		iicprobeinit(struct i2cbus_attach_args *, u_int8_t);
u_int8_t	iicprobenc(u_int8_t);
u_int8_t	iicprobe(u_int8_t);
u_int16_t	iicprobew(u_int8_t);
int		lm75probe(void);
char		*amd1032cloneprobe(u_int8_t);
void		iic_dump(struct device *, u_int8_t, char *);

void
iicprobeinit(struct i2cbus_attach_args *iba, u_int8_t addr)
{
	probe_ic = iba->iba_tag;
	probe_addr = addr;
	memset(probe_val, 0xff, sizeof probe_val);
}

u_int8_t
iicprobenc(u_int8_t cmd)
{
	u_int8_t data;

	/*
	 * If we think we are talking to an evil Maxim 1617 or clone,
	 * avoid accessing this register because it is death.
	 */
	if (skip_fc && cmd == 0xfc)
		return (0xff);
	probe_ic->ic_acquire_bus(probe_ic->ic_cookie, 0);
	if (iic_exec(probe_ic, I2C_OP_READ_WITH_STOP,
	    probe_addr, &cmd, 1, &data, 1, 0) != 0)
		data = 0xff;
	probe_ic->ic_release_bus(probe_ic->ic_cookie, 0);
	return (data);
}

u_int16_t
iicprobew(u_int8_t cmd)
{
	u_int16_t data2;

	/*
	 * If we think we are talking to an evil Maxim 1617 or clone,
	 * avoid accessing this register because it is death.
	 */
	if (skip_fc && cmd == 0xfc)
		return (0xffff);
	probe_ic->ic_acquire_bus(probe_ic->ic_cookie, 0);
	if (iic_exec(probe_ic, I2C_OP_READ_WITH_STOP,
	    probe_addr, &cmd, 1, &data2, 2, 0) != 0)
		data2 = 0xffff;
	probe_ic->ic_release_bus(probe_ic->ic_cookie, 0);
	return (data2);
}

u_int8_t
iicprobe(u_int8_t cmd)
{
	if (probe_val[cmd] != 0xff)
		return probe_val[cmd];
	probe_val[cmd] = iicprobenc(cmd);
	return (probe_val[cmd]);
}

/*
 * 0x06 and 0x07 return whatever value was read before, and the
 * chip loops every 8 registers.
 */
int
lm75probe(void)
{
	u_int16_t mains[6];
	u_int8_t main;
	int i;

	main = iicprobenc(0x01);
	mains[0] = iicprobew(0x02);
	mains[1] = iicprobew(0x03);

	if (main == 0xff && mains[0] == 0xffff && mains[1] == 0xffff)
		return (0);
	mains[2] = iicprobew(0x04);	/* read Low Limit */
	if (iicprobew(0x07) != mains[2] || iicprobew(0x07) != mains[2])
		return (0);

	mains[3] = iicprobew(0x05);	/* read High limit */
	mains[4] = iicprobew(0x06);
	mains[5] = iicprobew(0x07);
	if (mains[4] != mains[3] || mains[5] != mains[3])
		return (0);

#if 0
	printf("lm75probe: %02x %04x %04x %04x %04x %04x %04x\n", main,
	    mains[0], mains[1], mains[2], mains[3], mains[4], mains[5]);
#endif

	/* a real lm75/77 repeats it's registers.... */
	for (i = 0x08; i <= 0xf8; i += 8) {
		if (main != iicprobenc(0x01 + i) ||
		    mains[0] != iicprobew(0x02 + i) ||
		    mains[1] != iicprobew(0x03 + i) ||
		    mains[2] != iicprobew(0x04 + i) ||
		    mains[3] != iicprobew(0x05 + i) ||
		    mains[4] != iicprobew(0x06 + i) ||
		    mains[5] != iicprobew(0x07 + i))
			return (0);
	}

	/* We hope */
	return (1);
}

char *
amd1032cloneprobe(u_int8_t addr)
{
	if (addr == 0x18 || addr == 0x1a || addr == 0x29 ||
	    addr == 0x2b || addr == 0x4c || addr == 0x4e) {
		u_int8_t reg, val;
		int zero = 0, copy = 0;

		val = iicprobe(0x00);
		for (reg = 0x00; reg < 0x09; reg++) {
			if (iicprobe(reg) == 0xff)
				return (NULL);
			if (iicprobe(reg) == 0x00)
				zero++;
			if (val == iicprobe(reg))
				copy++;
		}
		if (zero > 6 || copy > 6)
			return (NULL);
		val = iicprobe(0x09);
		for (reg = 0x0a; reg < 0xfc; reg++) {
			if (iicprobe(reg) != val)
				return (NULL);
		}
		/* 0xfe may be Maxim, or some other vendor */
		if (iicprobe(0xfe) == 0x4d)
			return ("max1617");
		/*
		 * "xeontemp" is the name we choose for clone chips
		 * which have all sorts of buggy bus interactions, such
		 * as those we just probed.  Why?
		 * Intel is partly to blame for this situation.
		 */
		return ("xeontemp");
	}
	return (NULL);
}

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

#if defined(I2C_DEBUG) || defined(I2C_VERBOSE)
void
iic_dump(struct device *dv, u_int8_t addr, char *name)
{
	u_int8_t val = iicprobe(0);
	int i, cnt = 0;

	for (i = 1; i <= 0xff; i++) {
		if (val == iicprobe(i))
			cnt++;
	}
	if (cnt <= 254) {
		printf("%s: addr 0x%x", dv->dv_xname, addr);
		for (i = 0; i <= 0xff; i++) {
			if (iicprobe(i) != 0xff)
				printf(" %02x=%02x", i, iicprobe(i));
		}
		if (name)
			printf(": %s", name);
		printf("\n");
	}
}
#endif /* defined(I2C_DEBUG) || defined(I2C_VERBOSE) */

void
iic_probe(struct device *self, struct i2cbus_attach_args *iba, u_int8_t addr)
{
	struct i2c_attach_args ia;
	char *name = NULL;
	int i;

	for (i = 0; i < sizeof(ignore_addrs); i++)
		if (ignore_addrs[i] == addr)
			return;

	iicprobeinit(iba, addr);
	skip_fc = 0;

	switch (iicprobe(0x3e)) {
	case 0x41:
		/*
		 * Analog Devices adt/adm product code at 0x3e == 0x41.
		 * Newer chips have a valid 0x3d product number, while
		 * older ones sometimes encoded the product into the
		 * upper half of the "step register" at 0x3f
		 */
		if (iicprobe(0x3d) == 0x76 &&
		    (addr == 0x2c || addr == 0x2d || addr == 0x2e))
			name = "adt7476";
		else if ((addr == 0x2c || addr == 0x2e || addr == 0x2f) &&
		    iicprobe(0x3d) == 0x70)
			name = "adt7470";
		else if (iicprobe(0x3d) == 0x27 &&
		    (iicprobe(0x3f) == 0x60 || iicprobe(0x3f) == 0x6a))
			name = "adm1027";	/* complete check */
		else if (iicprobe(0x3d) == 0x27 &&
		    (iicprobe(0x3f) == 0x62 || iicprobe(0x3f) == 0x6a))
			name = "adt7460";	/* complete check */
		else if (iicprobe(0x3d) == 0x68 && addr == 0x2e &&
		    (iicprobe(0x3f) & 0xf0) == 0x70)
			name = "adt7467";
		else if (iicprobe(0x3d) == 0x33)
			name = "adm1033";
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    iicprobe(0x3d) == 0x30 &&
		    (iicprobe(0x3f) & 0x70) == 0x00 &&
		    (iicprobe(0x01) & 0x4a) == 0x00 &&
		    (iicprobe(0x03) & 0x3f) == 0x00 &&
		    (iicprobe(0x22) & 0xf0) == 0x00 &&
		    (iicprobe(0x0d) & 0x70) == 0x00 &&
		    (iicprobe(0x0e) & 0x70) == 0x00)
			name = "adm1030";	/* complete check */
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    iicprobe(0x3d) == 0x31 &&
		    (iicprobe(0x03) & 0x3f) == 0x00 &&
		    (iicprobe(0x0d) & 0x70) == 0x00 &&
		    (iicprobe(0x0e) & 0x70) == 0x00 &&
		    (iicprobe(0x0f) & 0x70) == 0x00)
			name = "adm1031";	/* complete check */
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    (iicprobe(0x3f) & 0xf0) == 0x20 &&
		    (iicprobe(0x40) & 0x80) == 0x00 &&
		    (iicprobe(0x41) & 0xc0) == 0x00 &&
		    (iicprobe(0x42) & 0xbc) == 0x00)
			name = "adm1025";	/* complete check */
		else if ((addr & 0x7c) == 0x2c &&	/* addr 0b01011xx */
		    (iicprobe(0x3f) & 0xf0) == 0x10 &&
		    (iicprobe(0x40) & 0x80) == 0x00)
			name = "adm1024";	/* complete check */
		else if ((iicprobe(0xff) & 0xf0) == 0x30)
			name = "adm1023";
		else if ((iicprobe(0x3f) & 0xf0) == 0xd0 && addr == 0x2e &&
		    (iicprobe(0x40) & 0x80) == 0x00)
			name = "adm1028";	/* adm1022 clone? */
		else if ((iicprobe(0x3f) & 0xf0) == 0xc0 &&
		    (addr == 0x2c || addr == 0x2e || addr == 0x2f) &&
		    (iicprobe(0x40) & 0x80) == 0x00)
			name = "adm1022";
		break;
	case 0xa1:
		/* Philips vendor code 0xa1 at 0x3e */
		if ((iicprobe(0x3f) & 0xf0) == 0x20 &&
		    (iicprobe(0x40) & 0x80) == 0x00 &&
		    (iicprobe(0x41) & 0xc0) == 0x00 &&
		    (iicprobe(0x42) & 0xbc) == 0x00)
			name = "ne1619";	/* adm1025 compat */
		break;
	case 0x23:	/* 2nd ADM id? */
		if (iicprobe(0x48) == addr &&
		    (iicprobe(0x40) & 0x80) == 0x00 &&
		    (addr & 0x7c) == 0x2c)
			name = "adm9240";	/* lm87 clone */
		break;
	case 0x55:
		if (iicprobe(0x3f) == 0x20 && (iicprobe(0x47) & 0x70) == 0x00 &&
		    (iicprobe(0x49) & 0xfe) == 0x80 &&
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
		if (iicprobe(0x3f) == 0x52 && iicprobe(0xff) == 0x01 &&
		    (iicprobe(0xfe) == 0x4c || iicprobe(0xfe) == 0x4d))
			name = "lm89";		/* lm89 "alike" */
		else if (iicprobe(0x3f) == 0x33 && iicprobe(0xff) == 0x01 &&
		    iicprobe(0xfe) == 0x21)
			name = "lm90";		/* lm90 "alike" */
		else if (iicprobe(0x3f) == 0x49 && iicprobe(0xff) == 0x01 &&
		    (iicprobe(0xfe) == 0x31 || iicprobe(0xfe) == 0x34))
			name = "lm99";		/* lm99 "alike" */
		else if (iicprobe(0x3f) == 0x73)
			name = "lm93";
		else if (iicprobe(0x3f) == 0x17)
			name = "lm86";
		else if (iicprobe(0x3f) == 0x68 &&
		    (addr == 0x2c || addr == 0x2d || addr == 0x2e))
			name = "lm96000";	/* adt7460 compat? */
		else if ((iicprobe(0x3f) == 0x60 || iicprobe(0x3f) == 0x62) &&
		    (addr == 0x2c || addr == 0x2d || addr == 0x2e))
			name = "lm85";		/* lm85C/B == adt7460 compat */
		else if (iicprobe(0x3f) == 0x03 && iicprobe(0x48) == addr &&
		    ((iicprobe(0x40) & 0x80) == 0x00) && ((addr & 0x7c) == 0x2c))
			name = "lm81";
		break;
	case 0x49:	/* TI */
		if ((iicprobe(0x3f) & 0xf0) == 0xc0 &&
		    (addr == 0x2c || addr == 0x2e || addr == 0x2f) &&
		    (iicprobe(0x40) & 0x80) == 0x00)
			name = "thmc50";	/* adm1022 clone */
		break;
	case 0x5c:	/* SMSC */
		if ((iicprobe(0x3f) & 0xf0) == 0x60 &&
		    (addr == 0x2c || addr == 0x2d || addr == 0x2e))
			name = "emc6d10x";	/* adt7460 compat */
		break;
	case 0x02:
		if ((iicprobe(0x3f) & 0xfc) == 0x04)
			name = "lm87";		/* complete check */
		break;
	case 0xda:
		if (iicprobe(0x3f) == 0x01 && iicprobe(0x48) == addr &&
		    (iicprobe(0x40) & 0x80) == 0x00)
			name = "ds1780";	/* lm87 clones */
		break;
	}
	switch (iicprobe(0x4e)) {
	case 0x41:
		if ((addr == 0x48 || addr == 0x4a || addr == 0x4b) &&
		    /* addr 0b1001{000, 010, 011} */
		    (iicprobe(0x4d) == 0x03 || iicprobe(0x4d) == 0x08 ||
		    iicprobe(0x4d) == 0x07))
			name = "adt7516";	/* adt7517, adt7519 */
		break;
	}

	if (iicprobe(0xfe) == 0x01) {
		/* Some more National devices ...*/
		if (iicprobe(0xff) == 0x21 && (iicprobe(0x03) & 0x2a) == 0 &&
		    iicprobe(0x04) <= 0x09 && iicprobe(0xff))
			name = "lm90";		/* complete check */
		else if (iicprobe(0xff) == 0x31 && addr == 0x4c &&
		    (iicprobe(0x03) & 0x2a) == 0 && iicprobe(0x04) <= 0x09)
			name = "lm99";
		else if (iicprobe(0xff) == 0x34 && addr == 0x4d &&
		    (iicprobe(0x03) & 0x2a) == 0 && iicprobe(0x04) <= 0x09)
			name = "lm99-1";
		else if (iicprobe(0xff) == 0x11 &&
		    (iicprobe(0x03) & 0x2a) == 0 && iicprobe(0x04) <= 0x09)
			name = "lm86";
	} else if (iicprobe(0xfe) == 0x4d && iicprobe(0xff) == 0x08) {
		name = "max6690";	/* somewhat similar to lm90 */
	} else if (iicprobe(0xfe) == 0x41 && (addr == 0x4c || addr == 0x4d) &&
	    (iicprobe(0x03) & 0x2a) == 0 && iicprobe(0x04) <= 0x09) {
		name = "adm1032";
		skip_fc = 1;
	} else if (iicprobe(0xfe) == 0x41 && iicprobe(0x3c) == 0x00 &&
	    (addr == 0x18 || addr == 0x19 || addr == 0x1a ||
	    addr == 0x29 || addr == 0x2a || addr == 0x2b ||
	    addr == 0x4c || addr == 0x4d || addr == 0x4e)) {
		name = "adm1021";	/* lots of addresses... bleah */
		skip_fc = 1;
	} else if ((iicprobe(0x4f) == 0x5c && (iicprobe(0x4e) & 0x80)) ||
	    (iicprobe(0x4f) == 0xa3 && !(iicprobe(0x4e) & 0x80))) {
		/*
		 * We could toggle 0x4e bit 0x80, then re-read 0x4f to
		 * see if the value changes to 0xa3 (indicating Winbond).
		 * But we are trying to avoid writes.
		 */
		switch (iicprobe(0x58)) {
		case 0x10:
		case 0x11:			/* rev 2? */
			name = "w83781d";
			break;
		case 0x21:
			name = "w83627hf";
			break;
		case 0x30:
			name = "w83782d";
			break;
		case 0x31:
			name = "as99127f";	/* rev 2 */
			break;
		case 0x40:
			name = "w83783s";
			break;
		case 0x71:
		case 0x72:			/* rev 2? */
			name = "w83791d";
			break;
		case 0x7a:
			name = "w83792d";
			break;
		}
	} else if (iicprobe(0x4f) == 0x12 && (iicprobe(0x4e) & 0x80)) {
		/*
		 * We could toggle 0x4e bit 0x80, then re-read 0x4f to
		 * see if the value changes to 0xc3 (indicating ASUS).
		 * But we are trying to avoid writes.
		 */
		if (iicprobe(0x58) == 0x31)
			name = "as99127f";	/* rev 1 */
	} else if (addr == 0x2d &&
	    ((iicprobe(0x4f) == 0x06 && (iicprobe(0x4e) & 0x80)) ||
	    (iicprobe(0x4f) == 0x94 && !(iicprobe(0x4e) & 0x80)))) {
		/*
		 * We could toggle 0x4e bit 0x80, then re-read 0x4f to
		 * see if the value changes to 0x94 (indicating ASUS).
		 * But we are trying to avoid writes.
		 *
		 * NB. we won't match if the BIOS has selected a non-zero
		 * register bank (set via 0x4e). We could select bank 0 so
		 * we see the right registers, but that would require a
		 * write.  In general though, we bet no BIOS would leave us
		 * in the wrong state.
		 */
		if ((iicprobe(0x58) & 0x7f) == 0x31 &&
		    (iicprobe(0x4e) & 0xf) == 0x00)
			name = "asb100";
	} else if (iicprobe(0x16) == 0x41 && ((iicprobe(0x17) & 0xf0) == 0x40) &&
	    (addr == 0x2c || addr == 0x2d || addr == 0x2e)) {
		name = "adm1026";
	} else if ((addr & 0xfc) == 0x48 && lm75probe()) {
		name = "lm75";
	} else if (name == NULL) {
		name = amd1032cloneprobe(addr);
		if (name)
			skip_fc = 1;
	}

#ifdef I2C_DEBUG
	iic_dump(self, addr, name);
#endif /* I2C_DEBUG */

	if (name) {
		ia.ia_tag = iba->iba_tag;
		ia.ia_addr = addr;
		ia.ia_size = 1;
		ia.ia_name = name;
		if (config_found(self, &ia, iic_print))
			return;
	}

#ifdef I2C_VERBOSE
	iic_dump(self, addr, name);
#endif /* I2C_DEBUG */

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
			ic->ic_acquire_bus(ic->ic_cookie, 0);
			if (iic_exec(ic, I2C_OP_READ_WITH_STOP, addr,
			    &cmd, 1, NULL, 0, 0) == 0) {
				ic->ic_release_bus(ic->ic_cookie, 0);

				/* Some device exists, so go scope it out */
				iic_probe(self, iba, addr);

				ic->ic_acquire_bus(ic->ic_cookie, 0);

			}
			ic->ic_release_bus(ic->ic_cookie, 0);
		}
	}
}
