/*	$OpenBSD: db_disasm.c,v 1.1.1.1 2006/10/06 21:02:55 miod Exp $	*/
/*	$NetBSD: db_disasm.c,v 1.13 2006/01/21 02:09:06 uwe Exp $	*/

/*
 * Copyright (c) 1998-2000 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Author: Akinori Koketsu
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistribution with functional modification must include
 *    prominent notice stating how and when and by whom it is
 *    modified.
 * 3. Redistributions in binary form have to be along with the source
 *    code or documentation which include above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 4. All commercial advertising materials mentioning features or use
 *    of this software must display the following acknowledgement:
 *      This product includes software developed by Internet
 *      Initiative Japan Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#include <ddb/db_output.h>

static void	get_opcode(uint16_t *, char *, size_t);
static void	get_ascii(unsigned char *, char *);
static void	f_02(uint16_t *, char *, size_t);
static void	f_03(uint16_t *, char *, size_t);
static void	f_04(uint16_t *, char *, size_t);
static void	f_08(uint16_t *, char *, size_t);
static void	f_09(uint16_t *, char *, size_t);
static void	f_0a(uint16_t *, char *, size_t);
static void	f_0b(uint16_t *, char *, size_t);
static void	f_0c(uint16_t *, char *, size_t);
static void	f_10(uint16_t *, char *, size_t);
static void	f_20(uint16_t *, char *, size_t);
static void	f_24(uint16_t *, char *, size_t);
static void	f_28(uint16_t *, char *, size_t);
static void	f_2c(uint16_t *, char *, size_t);
static void	f_30(uint16_t *, char *, size_t);
static void	f_34(uint16_t *, char *, size_t);
static void	f_38(uint16_t *, char *, size_t);
static void	f_3c(uint16_t *, char *, size_t);
static void	f_40(uint16_t *, char *, size_t);
static void	f_41(uint16_t *, char *, size_t);
static void	f_42(uint16_t *, char *, size_t);
static void	f_43(uint16_t *, char *, size_t);
static void	f_44(uint16_t *, char *, size_t);
static void	f_45(uint16_t *, char *, size_t);
static void	f_46(uint16_t *, char *, size_t);
static void	f_47(uint16_t *, char *, size_t);
static void	f_48(uint16_t *, char *, size_t);
static void	f_49(uint16_t *, char *, size_t);
static void	f_4a(uint16_t *, char *, size_t);
static void	f_4b(uint16_t *, char *, size_t);
static void	f_4c(uint16_t *, char *, size_t);
static void	f_4d(uint16_t *, char *, size_t);
static void	f_4e(uint16_t *, char *, size_t);
static void	f_4f(uint16_t *, char *, size_t);
static void	f_50(uint16_t *, char *, size_t);
static void	f_60(uint16_t *, char *, size_t);
static void	f_64(uint16_t *, char *, size_t);
static void	f_68(uint16_t *, char *, size_t);
static void	f_6c(uint16_t *, char *, size_t);
static void	f_70(uint16_t *, char *, size_t);
static void	f_80(uint16_t *, char *, size_t);
static void	f_90(uint16_t *, char *, size_t);
static void	f_a0(uint16_t *, char *, size_t);
static void	f_b0(uint16_t *, char *, size_t);
static void	f_c0(uint16_t *, char *, size_t);
static void	f_d0(uint16_t *, char *, size_t);
static void	f_e0(uint16_t *, char *, size_t);
static void	f_f0(uint16_t *, char *, size_t);
static void	f_f4(uint16_t *, char *, size_t);
static void	f_f8(uint16_t *, char *, size_t);
static void	f_fc(uint16_t *, char *, size_t);
static void	f_fd(uint16_t *, char *, size_t);
static void	f_fe(uint16_t *, char *, size_t);

typedef	void (*rasm_t)(uint16_t *, char *, size_t);
static	rasm_t	f[16][16] = {
	{ /* [0][0-7] */	NULL, NULL, f_02, f_03, f_04, f_04, f_04, f_04,
	  /* [0][8-f] */	f_08, f_09, f_0a, f_0b, f_0c, f_0c, f_0c, f_0c },
	{ /* [1][0-7] */	f_10, f_10, f_10, f_10, f_10, f_10, f_10, f_10,
	  /* [1][8-f] */	f_10, f_10, f_10, f_10, f_10, f_10, f_10, f_10 },
	{ /* [2][0-7] */	f_20, f_20, f_20, f_20, f_24, f_24, f_24, f_24,
	  /* [2][8-f] */	f_28, f_28, f_28, f_28, f_2c, f_2c, f_2c, f_2c },
	{ /* [3][0-7] */	f_30, f_30, f_30, f_30, f_34, f_34, f_34, f_34,
	  /* [3][8-f] */	f_38, f_38, f_38, f_38, f_3c, f_3c, f_3c, f_3c },
	{ /* [4][0-7] */	f_40, f_41, f_42, f_43, f_44, f_45, f_46, f_47,
	  /* [4][8-f] */	f_48, f_49, f_4a, f_4b, f_4c, f_4d, f_4e, f_4f },
	{ /* [5][0-7] */	f_50, f_50, f_50, f_50, f_50, f_50, f_50, f_50,
	  /* [5][8-f] */	f_50, f_50, f_50, f_50, f_50, f_50, f_50, f_50 },
	{ /* [6][0-7] */	f_60, f_60, f_60, f_60, f_64, f_64, f_64, f_64,
	  /* [6][8-f] */	f_68, f_68, f_68, f_68, f_6c, f_6c, f_6c, f_6c },
	{ /* [7][0-7] */	f_70, f_70, f_70, f_70, f_70, f_70, f_70, f_70,
	  /* [7][8-f] */	f_70, f_70, f_70, f_70, f_70, f_70, f_70, f_70 },
	{ /* [8][0-7] */	f_80, f_80, f_80, f_80, f_80, f_80, f_80, f_80,
	  /* [8][8-f] */	f_80, f_80, f_80, f_80, f_80, f_80, f_80, f_80 },
	{ /* [9][0-7] */	f_90, f_90, f_90, f_90, f_90, f_90, f_90, f_90,
	  /* [9][8-f] */	f_90, f_90, f_90, f_90, f_90, f_90, f_90, f_90 },
	{ /* [a][0-7] */	f_a0, f_a0, f_a0, f_a0, f_a0, f_a0, f_a0, f_a0,
	  /* [a][8-f] */	f_a0, f_a0, f_a0, f_a0, f_a0, f_a0, f_a0, f_a0 },
	{ /* [b][0-7] */	f_b0, f_b0, f_b0, f_b0, f_b0, f_b0, f_b0, f_b0,
	  /* [b][8-f] */	f_b0, f_b0, f_b0, f_b0, f_b0, f_b0, f_b0, f_b0 },
	{ /* [c][0-7] */	f_c0, f_c0, f_c0, f_c0, f_c0, f_c0, f_c0, f_c0,
	  /* [c][8-f] */	f_c0, f_c0, f_c0, f_c0, f_c0, f_c0, f_c0, f_c0 },
	{ /* [d][0-7] */	f_d0, f_d0, f_d0, f_d0, f_d0, f_d0, f_d0, f_d0,
	  /* [d][8-f] */	f_d0, f_d0, f_d0, f_d0, f_d0, f_d0, f_d0, f_d0 },
	{ /* [e][0-7] */	f_e0, f_e0, f_e0, f_e0, f_e0, f_e0, f_e0, f_e0,
	  /* [e][8-f] */	f_e0, f_e0, f_e0, f_e0, f_e0, f_e0, f_e0, f_e0 },
	{ /* [f][0-7] */	f_f0, f_f0, f_f0, f_f0, f_f4, f_f4, f_f4, f_f4,
	  /* [f][8-f] */	f_f8, f_f8, f_f8, f_f8, f_fc, f_fd, f_fe, NULL }
};

db_addr_t
db_disasm(db_addr_t loc, boolean_t altfmt)
{
	char line[40], ascii[4];
	void *pc = (void *)loc;

	get_opcode(pc, line, sizeof line);
	if (altfmt) {
		get_ascii(pc, ascii);
		db_printf("%-32s ! %s\n", line, ascii);
	} else
		db_printf("%s\n", line);

	return (loc + 2);
}

static void
get_ascii(unsigned char *cp, char *str)
{

	*str++ = (0x20 <= *cp && *cp < 0x7f) ? *cp : '.';
	cp++;
	*str++ = (0x20 <= *cp && *cp < 0x7f) ? *cp : '.';
	*str = '\0';
}

static void
get_opcode(uint16_t *sp, char *buf, size_t bufsiz)
{
	int	n0, n3;

	strlcpy(buf, "unknown opcode", bufsiz);

	n0 = (*sp & 0xf000) >> 12;
	n3 = (*sp & 0x000f);

	if (f[n0][n3] != NULL) {
		(*f[n0][n3])(sp, buf, bufsiz);
	}
}

static void
f_02(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, type, md;

	rn   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "stc     sr, r%d", rn);
			break;

		case 1:
			snprintf(buf, bufsiz, "stc     gbr, r%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "stc     vbr, r%d", rn);
			break;

		case 3:
			snprintf(buf, bufsiz, "stc     ssr, r%d", rn);
			break;

		}
		break;

	case 1:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "stc     spc, r%d", rn);
			break;
		}
		break;

	case 2:
		snprintf(buf, bufsiz, "stc     r%d_bank, r%d", md, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "stc     r%d_bank, r%d", md+4, rn);
		break;
	} /* end of switch (type) */
}

static void
f_03(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, type, md;

	rn   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "bsrf    r%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "braf    r%d", rn);
			break;
		}
		break;

	case 2:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "pref    @r%d", rn);
			break;
		}
		break;
	} /* end of switch (type) */
}


static void
f_04(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "mov.b   r%d, @(r0, r%d)", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "mov.w   r%d, @(r0, r%d)", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "mov.l   r%d, @(r0, r%d)", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "mul.l   r%d, r%d)", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_08(uint16_t *code, char *buf, size_t bufsiz)
{
	int	n1, type, md;

	n1   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	if (n1 != 0)
		return;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			strlcpy(buf, "clrt", bufsiz);
			break;

		case 1:
			strlcpy(buf, "sett", bufsiz);
			break;

		case 2:
			strlcpy(buf, "clrmac", bufsiz);
			break;

		case 3:
			strlcpy(buf, "ldtlb", bufsiz);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 0:
			strlcpy(buf, "clrs", bufsiz);
			break;

		case 1:
			strlcpy(buf, "sets", bufsiz);
			break;
		}
		break;
	} /* end of switch (type) */
}

static void
f_09(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, fx;

	rn = (*code & 0x0f00) >> 8;
	fx = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		if (rn != 0)
			return;
		strlcpy(buf, "nop", bufsiz);
		break;

	case 1:
		if (rn != 0)
			return;
		strlcpy(buf, "div0u", bufsiz);
		break;

	case 2:
		snprintf(buf, bufsiz, "movt    r%d", rn);
		break;
	} /* end of switch (fx) */
}

static void
f_0a(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, type, md;

	rn   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "sts     mach, r%d", rn);
			break;

		case 1:
			snprintf(buf, bufsiz, "sts     macl, r%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "sts     pr, r%d", rn);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 1:
			snprintf(buf, bufsiz, "sts     fpul, r%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "sts     fpscr, r%d", rn);
			break;
		}
		break;
	} /* end of switch (type) */
}

static void
f_0b(uint16_t *code, char *buf, size_t bufsiz)
{
	int	n1, fx;

	n1 = (*code & 0x0f00) >> 8;
	if (n1 != 0)
		return;

	fx = (*code & 0x00f0) >> 4;
	switch (fx) {
	case 0:
		strlcpy(buf, "rts", bufsiz);
		break;

	case 1:
		strlcpy(buf, "sleep", bufsiz);
		break;

	case 2:
		strlcpy(buf, "rte", bufsiz);
		break;
	} /* end of switch (fx) */
}

static void
f_0c(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "mov.b   @(r0, r%d), r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "mov.w   @(r0, r%d), r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "mov.l   @(r0, r%d), r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "mac.l   @r%d+, r%d+", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_10(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, disp;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	disp = (*code & 0x000f);
	disp *= 4;

	snprintf(buf, bufsiz, "mov.l   r%d, @(%d, r%d)", rm, disp, rn);
}

static void
f_20(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "mov.b   r%d, @r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "mov.w   r%d, @r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "mov.l   r%d, @r%d", rm, rn);
		break;
	} /* end of switch (md) */
}


static void
f_24(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "mov.b   r%d, @-r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "mov.w   r%d, @-r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "mov.l   r%d, @-r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "div0s   r%d, r%d)", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_28(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "tst     r%d, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "and     r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "xor     r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "or      r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}


static void
f_2c(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "cmp/str r%d, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "xtrct   r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "mulu.w  r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "muls.w  r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_30(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "cmp/eq  r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "cmp/hs  r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "cmp/ge  r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}


static void
f_34(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "div1    r%d, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "dmulu.l r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "cmp/hi  r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "cmp/gt  r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_38(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "sub     r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "subc    r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "subv    r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}


static void
f_3c(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "add     r%d, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "dmulu.l r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "addc    r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "addv    r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}


static void
f_40(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, fx;

	rn   = (*code & 0x0f00) >> 8;
	fx   = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		snprintf(buf, bufsiz, "shll    r%d", rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "dt      r%d", rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "shal    r%d", rn);
		break;
	} /* end of switch (fx) */
}

static void
f_41(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, fx;

	rn   = (*code & 0x0f00) >> 8;
	fx   = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		snprintf(buf, bufsiz, "shlr    r%d", rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "cmp/pz  r%d", rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "shar    r%d", rn);
		break;
	} /* end of switch (fx) */
}


static void
f_42(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, type, md;

	rn   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "sts.l   mach, @-r%d", rn);
			break;

		case 1:
			snprintf(buf, bufsiz, "sts.l   macl, @-r%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "sts.l   pr, @-r%d", rn);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 1:
			snprintf(buf, bufsiz, "sts.l   fpul, @-r%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "sts.l   fpscr, @-r%d", rn);
			break;
		}
		break;
	} /* end of switch (type) */
}

static void
f_43(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, type, md;

	rn   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "stc.l   sr, @-r%d", rn);
			break;

		case 1:
			snprintf(buf, bufsiz, "stc.l   gbr, @-r%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "stc.l   vbr, @-r%d", rn);
			break;

		case 3:
			snprintf(buf, bufsiz, "stc.l   ssr, @-r%d", rn);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "stc.l   spc, @-r%d", rn);
			break;
		}
		break;

	case 2:
		snprintf(buf, bufsiz, "stc.l   r%d_bank, @-r%d", md, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "stc.l   r%d_bank, @-r%d", md+4, rn);
		break;
	} /* end of switch (type) */
}

static void
f_44(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, fx;

	rn   = (*code & 0x0f00) >> 8;
	fx   = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		snprintf(buf, bufsiz, "rotl    r%d", rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "rotcl   r%d", rn);
		break;
	} /* end of switch (fx) */
}

static void
f_45(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, fx;

	rn   = (*code & 0x0f00) >> 8;
	fx   = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		snprintf(buf, bufsiz, "rotr    r%d", rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "cmp/pl  r%d", rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "rotcr   r%d", rn);
		break;
	} /* end of switch (fx) */
}

static void
f_46(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rm, type, md;

	rm   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "lds.l   @r%d+, mach", rm);
			break;

		case 1:
			snprintf(buf, bufsiz, "lds.l   @r%d+, macl", rm);
			break;

		case 2:
			snprintf(buf, bufsiz, "lds.l   @r%d+, pr", rm);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 1:
			snprintf(buf, bufsiz, "lds.l   @r%d+, fpul", rm);
			break;

		case 2:
			snprintf(buf, bufsiz, "lds.l   @r%d+, fpscr", rm);
			break;
		}
		break;
	} /* end of switch (type) */
}

static void
f_47(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rm, type, md;

	rm   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "ldc.l   @r%d+, sr", rm);
			break;

		case 1:
			snprintf(buf, bufsiz, "ldc.l   @r%d+, gbr", rm);
			break;

		case 2:
			snprintf(buf, bufsiz, "ldc.l   @r%d+, vbr", rm);
			break;

		case 3:
			snprintf(buf, bufsiz, "ldc.l   @r%d+, ssr", rm);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "ldc.l   @r%d+, spc", rm);
			break;
		}
		break;

	case 2:
		snprintf(buf, bufsiz, "ldc.l   @r%d+, r%d_bank", rm, md);
		break;

	case 3:
		snprintf(buf, bufsiz, "ldc.l   @r%d+, r%d_bank", rm, md+4);
		break;
	} /* end of switch (type) */
}

static void
f_48(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, fx;

	rn   = (*code & 0x0f00) >> 8;
	fx   = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		snprintf(buf, bufsiz, "shll2   r%d", rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "shll8   r%d", rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "shll16  r%d", rn);
		break;
	} /* end of switch (fx) */
}

static void
f_49(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, fx;

	rn   = (*code & 0x0f00) >> 8;
	fx   = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		snprintf(buf, bufsiz, "shlr2   r%d", rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "shlr8   r%d", rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "shlr16  r%d", rn);
		break;
	} /* end of switch (fx) */
}

static void
f_4a(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rm, type, md;

	rm   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "lds     r%d, mach", rm);
			break;

		case 1:
			snprintf(buf, bufsiz, "lds     r%d, macl", rm);
			break;

		case 2:
			snprintf(buf, bufsiz, "lds     r%d, pr", rm);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 1:
			snprintf(buf, bufsiz, "lds     r%d, fpul", rm);
			break;

		case 2:
			snprintf(buf, bufsiz, "lds     r%d, fpscr", rm);
			break;
		}
		break;
	} /* end of switch (type) */
}

static void
f_4b(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rm, fx;

	rm   = (*code & 0x0f00) >> 8;
	fx   = (*code & 0x00f0) >> 4;

	switch (fx) {
	case 0:
		snprintf(buf, bufsiz, "jsr     @r%d", rm);
		break;

	case 1:
		snprintf(buf, bufsiz, "tas.b   @r%d", rm);
		break;

	case 2:
		snprintf(buf, bufsiz, "jmp     @r%d", rm);
		break;
	} /* end of switch (fx) */
}

static void
f_4c(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	snprintf(buf, bufsiz, "shad    r%d, r%d", rm, rn);
}

static void
f_4d(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	snprintf(buf, bufsiz, "shld    r%d, r%d", rm, rn);
}

static void
f_4e(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rm, type, md;

	rm   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "ldc     r%d, sr", rm);
			break;

		case 1:
			snprintf(buf, bufsiz, "ldc     r%d, gbr", rm);
			break;

		case 2:
			snprintf(buf, bufsiz, "ldc     r%d, vbr", rm);
			break;

		case 3:
			snprintf(buf, bufsiz, "ldc     r%d, ssr", rm);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "ldc     r%d, spc", rm);
			break;
		}
		break;

	case 2:
		snprintf(buf, bufsiz, "ldc     r%d, r%d_bank", rm, md);
		break;

	case 3:
		snprintf(buf, bufsiz, "ldc     r%d, r%d_bank", rm, md+4);
		break;
	} /* end of switch (type) */
}

static void
f_4f(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	snprintf(buf, bufsiz, "mac.w   @r%d+, @r%d+", rm, rn);
}

static void
f_50(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, disp;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	disp = (*code & 0x000f);
	disp *= 4;

	snprintf(buf, bufsiz, "mov.l   @(%d, r%d), r%d", disp, rm, rn);
}

static void
f_60(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "mov.b   @r%d, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "mov.w   @r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "mov.l   @r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "mov     r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_64(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "mov.b   @r%d+, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "mov.w   @r%d+, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "mov.l   @r%d+, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "not     r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_68(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "swap.b  r%d, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "swap.w  r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "negc    r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "neg     r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_6c(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "extu.b  r%d, r%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "extu.w  r%d, r%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "exts.b  r%d, r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "exts.w  r%d, r%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_70(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, imm;

	rn   = (*code & 0x0f00) >> 8;
	imm  = (int) ((char) (*code & 0x00ff));

	snprintf(buf, bufsiz, "add     #0x%x, r%d", imm, rn);
}

static void
f_80(uint16_t *code, char *buf, size_t bufsiz)
{
	int	type, md, rn, disp;

	type = (*code & 0x0c00) >> 10;
	md   = (*code & 0x0300) >> 8;

	switch (type) {
	case 0:
		rn   = (*code & 0x00f0) >> 4;
		disp = (*code & 0x000f);

		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "mov.b   r0, @(%d, r%d)",
			    disp, rn);
			break;

		case 1:
			disp *= 2;
			snprintf(buf, bufsiz, "mov.w   r0, @(%d, r%d)",
			    disp, rn);
			break;
		}
		break;

	case 1:
		rn   = (*code & 0x00f0) >> 4;
		disp = (*code & 0x000f);

		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "mov.b   @(%d, r%d), r0",
			    disp, rn);
			break;

		case 1:
			disp *= 2;
			snprintf(buf, bufsiz, "mov.w   @(%d, r%d), r0",
			    disp, rn);
			break;
		}
		break;

	case 2:
		disp = (*code & 0x00ff);

		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "cmp/eq  #%d, r0", disp);
			break;

		case 1:
			disp = (int) ((char) disp);
			disp *= 2;
			snprintf(buf, bufsiz, "bt      0x%x", disp);
			break;

		case 3:
			disp = (int) ((char) disp);
			disp *= 2;
			snprintf(buf, bufsiz, "bf      0x%x", disp);
			break;
		}
		break;

	case 3:
		disp = (int) ((char) (*code & 0x00ff));
		disp *= 2;

		switch (md) {
		case 1:
			snprintf(buf, bufsiz, "bt/s    0x%x", disp);
			break;

		case 3:
			snprintf(buf, bufsiz, "bf/s    0x%x", disp);
			break;
		}
		break;
	} /* end of switch (type) */
}

static void
f_90(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, disp;

	rn   = (*code & 0x0f00) >> 8;
	disp = (*code & 0x00ff);
	disp *= 2;

	snprintf(buf, bufsiz, "mov.w   @(%d, pc), r%d", disp, rn);
}

static void
f_a0(uint16_t *code, char *buf, size_t bufsiz)
{
	int	disp;

	disp = (*code & 0x0fff);
	if (disp & 0x0800)	/* negative displacement? */
		disp |= 0xfffff000; /* sign extend */
	disp *= 2;

	snprintf(buf, bufsiz, "bra     %d(0x%x)", disp, disp);
}

static void
f_b0(uint16_t *code, char *buf, size_t bufsiz)
{
	int	disp;

	disp = (*code & 0x0fff);
	if (disp & 0x0800)	/* negative displacement? */
		disp |= 0xfffff000; /* sign extend */
	disp *= 2;

	snprintf(buf, bufsiz, "bsr     %d(0x%x)", disp, disp);
}

static void
f_c0(uint16_t *code, char *buf, size_t bufsiz)
{
	int	type, md, imm;

	type = (*code & 0x0c00) >> 10;
	md   = (*code & 0x0300) >> 8;
	imm  = (*code & 0x00ff);

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "mov.b   r0, @(%d, gbr)", imm);
			break;

		case 1:
			imm *= 2;
			snprintf(buf, bufsiz, "mov.w   r0, @(%d, gbr)", imm);
			break;

		case 2:
			imm *= 4;
			snprintf(buf, bufsiz, "mov.l   r0, @(%d, gbr)", imm);
			break;

		case 3:
			snprintf(buf, bufsiz, "trapa   #%d", imm);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "mov.b   @(%d, gbr), r0", imm);
			break;

		case 1:
			imm *= 2;
			snprintf(buf, bufsiz, "mov.w   @(%d, gbr), r0", imm);
			break;

		case 2:
			imm *= 4;
			snprintf(buf, bufsiz, "mov.l   @(%d, gbr), r0", imm);
			break;

		case 3:
			imm *= 4;
			snprintf(buf, bufsiz, "mova    @(%d, pc), r0", imm);
			break;
		}
		break;

	case 2:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "tst     #%d, r0", imm);
			break;

		case 1:
			snprintf(buf, bufsiz, "and     #%d, r0", imm);
			break;

		case 2:
			snprintf(buf, bufsiz, "xor     #%d, r0", imm);
			break;

		case 3:
			snprintf(buf, bufsiz, "or      #%d, r0", imm);
			break;
		}
		break;

	case 3:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "tst.b   #%d, @(r0, gbr)", imm);
			break;

		case 1:
			snprintf(buf, bufsiz, "and.b   #%d, @(r0, gbr)", imm);
			break;

		case 2:
			snprintf(buf, bufsiz, "xor.b   #%d, @(r0, gbr)", imm);
			break;

		case 3:
			snprintf(buf, bufsiz, "or.b    #%d, @(r0, gbr)", imm);
			break;
		}
		break;
	} /* end of switch (type) */
}


static void
f_d0(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, disp;

	rn   = (*code & 0x0f00) >> 8;
	disp = (*code & 0x00ff);
	disp *= 4;

	snprintf(buf, bufsiz, "mov.l   @(%d, pc), r%d", disp, rn);
}

static void
f_e0(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, imm;

	rn   = (*code & 0x0f00) >> 8;
	imm  = (int) ((char) (*code & 0x00ff));

	snprintf(buf, bufsiz, "mov     #0x%x, r%d", imm, rn);
}

static void
f_f0(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "fadd    fr%d, fr%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "fsub    fr%d, fr%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "fmul    fr%d, fr%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "fdiv    fr%d, fr%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_f4(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "fcmp/eq fr%d, fr%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "fcmp/gt fr%d, fr%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "fmov.s  @(r0, r%d), fr%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "fmov.s  fr%d, @(r0, r%d)", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_f8(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm, md;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;
	md   = (*code & 0x0003);

	switch (md) {
	case 0:
		snprintf(buf, bufsiz, "fmov.s  @r%d, fr%d", rm, rn);
		break;

	case 1:
		snprintf(buf, bufsiz, "fmov.s  @r%d+, fr%d", rm, rn);
		break;

	case 2:
		snprintf(buf, bufsiz, "fmov.s  fr%d, @r%d", rm, rn);
		break;

	case 3:
		snprintf(buf, bufsiz, "fmov.s  fr%d, @-r%d", rm, rn);
		break;
	} /* end of switch (md) */
}

static void
f_fc(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;

	snprintf(buf, bufsiz, "fmov    fr%d, fr%d", rm, rn);
}

static void
f_fd(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, type, md;

	rn   = (*code & 0x0f00) >> 8;
	type = (*code & 0x00c0) >> 6;
	md   = (*code & 0x0030) >> 4;

	switch (type) {
	case 0:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "fsts    fpul, fr%d", rn);
			break;

		case 1:
			snprintf(buf, bufsiz, "flds    fr%d, fpul", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "float   fpul, fr%d", rn);
			break;

		case 3:
			snprintf(buf, bufsiz, "ftrc    fr%d, fpul", rn);
			break;
		}
		break;

	case 1:
		switch (md) {
		case 0:
			snprintf(buf, bufsiz, "fneg    fr%d", rn);
			break;

		case 1:
			snprintf(buf, bufsiz, "fabs    fr%d", rn);
			break;

		case 2:
			snprintf(buf, bufsiz, "fsqrt   fr%d", rn);
			break;
		}
		break;

	case 2:
		switch (md) {
		case 0:
		case 1:
			snprintf(buf, bufsiz, "fldi%d   fr%d", md, rn);
			break;
		}
		break;
	} /* end of switch (type) */
}

static void
f_fe(uint16_t *code, char *buf, size_t bufsiz)
{
	int	rn, rm;

	rn   = (*code & 0x0f00) >> 8;
	rm   = (*code & 0x00f0) >> 4;

	snprintf(buf, bufsiz, "fmac    fr0, fr%d, fr%d", rm, rn);
}
