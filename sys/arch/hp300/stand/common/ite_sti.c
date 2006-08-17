/*	$OpenBSD: ite_sti.c,v 1.1 2006/08/17 06:31:10 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat
 * Copyright (c) 2000-2003 Michael Shalayeff
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef ITECONSOLE
#include <sys/param.h>

#include <lib/libsa/stand.h>

#include "samachdep.h"
#include "consdefs.h"
#include "itevar.h"

#include <hp300/dev/sgcreg.h>
#include <dev/ic/stireg.h>

/*
 * sti-specific data not available in the ite_data structure.
 * Since we will only configure one sti display, it is ok to use a global.
 */
static struct {
	u_int32_t	codeptr[STI_CODECNT];
	u_int8_t	*code;
	u_int32_t	fontbase;
	u_int		firstchar, lastchar;
	struct sti_cfg	cfg;
	struct sti_ecfg	ecfg;
} sti;

#define parseshort1(addr, ofs) \
	(((addr)[(ofs) +  3] << 8) | ((addr)[(ofs) +  7]))
#define parseword1(addr, ofs) \
	(((addr)[(ofs) +  3] << 24) | ((addr)[(ofs) +  7] << 16) | \
	 ((addr)[(ofs) + 11] <<  8) | ((addr)[(ofs) + 15]))

void	sti_do_cursor(struct ite_data *);
void	sti_fontinfo(struct ite_data *);
void	sti_init(int);
void	sti_inqcfg(struct sti_inqconfout *);

/*
 * Initialize the sti device for ite's needs.
 * We don't bother to check for failures since
 * - we are in tight space already
 * - since romputchar() does not work with sti devices, there is no way we
 *   can report errors (although we could switch to serial...)
 */
void
sti_iteinit(struct ite_data *ip)
{
	int slotno, i;
	size_t codesize, memsize;
	u_int8_t *va, *code;
	u_int addr, eaddr, reglist, tmp;
	struct sti_inqconfout cfg;
	struct sti_einqconfout ecfg;

	bzero(&sti, sizeof sti);
	slotno = (int)ip->fbbase;
	ip->fbbase = va = (u_int8_t *)IIOV(SGC_BASE + (slotno * SGC_DEVSIZE));

	/*
	 * Read the microcode.
	 */

	for (i = 0; i < STI_CODECNT; i++)
		sti.codeptr[i] =
		    parseword1(va, (STI_CODEBASE_M68K << 2) + i * 0x10);

	for (i = STI_END; sti.codeptr[i] == 0; i--);
	codesize = sti.codeptr[i] - sti.codeptr[STI_BEGIN];
	codesize = (codesize + 3) / 4;

	sti.code = (u_int8_t *)alloc(codesize);
	code = sti.code;
	addr = (u_int)va + sti.codeptr[STI_BEGIN];
	eaddr = addr + codesize * 4;
	for (; addr < eaddr; addr += 4)
		*code++ = *(u_int8_t *)addr;

	for (i = STI_CODECNT - 1; i != 0; i--)
		if (sti.codeptr[i] != 0) {
			sti.codeptr[i] -= sti.codeptr[0];
			sti.codeptr[i] /= 4;
		}

	sti.codeptr[0] = 0;
	for (i = STI_END; sti.codeptr[i] == 0; i--);
	sti.codeptr[i] = 0;

	/*
	 * Read the regions list.
	 */

	reglist = parseword1(va, 0x60);
	for (i = 0; i < STI_REGION_MAX; i++) {
		tmp = parseword1(va, (reglist & ~3) + i * 0x10);
		sti.cfg.regions[i] = (u_int)va + ((tmp >> 18) << 12);
		if (tmp & 0x4000)
			break;
	}

	/*
	 * Allocate scratch memory for the microcode if it needs it.
	 */

	sti.cfg.ext_cfg = &sti.ecfg;
	memsize = parseword1(va, 0xa0);
	if (memsize != 0)
		sti.ecfg.addr = alloc(memsize);

	/*
	 * Initialize the display, and get geometry information.
	 */

	sti_init(0);

	bzero(&cfg, sizeof cfg);
	bzero(&ecfg, sizeof ecfg);
	cfg.ext = &ecfg;
	sti_inqcfg(&cfg);

	if (cfg.owidth == cfg.width && cfg.oheight == cfg.height) {
		sti.cfg.oscr_width = cfg.owidth = cfg.fbwidth - cfg.width;
		sti.cfg.oscr_height = cfg.oheight = cfg.fbheight - cfg.height;
	}

	ip->dheight = cfg.height;
	ip->dwidth = cfg.width;
	ip->fbheight = cfg.fbheight;
	ip->fbwidth = cfg.fbwidth;

	/*
	 * Get ready for ite operation!
	 */

	sti_init(1);
	sti_fontinfo(ip);
	sti_clear(ip, 0, 0, ip->rows, ip->cols);	/* necessary? */
}

void
sti_putc(struct ite_data *ip, int c, int dy, int dx)
{
	sti_unpmv_t unpmv;
	struct {
		struct sti_unpmvflags flags;
		struct sti_unpmvin in;
		struct sti_unpmvout out;
	} a;

	bzero(&a, sizeof a);
	a.flags.flags = STI_UNPMVF_WAIT;
	a.in.bg_colour = STI_COLOUR_BLACK;
	a.in.fg_colour = STI_COLOUR_WHITE;
	a.in.x = dx * ip->ftwidth;
	a.in.y = dy * ip->ftheight;
	a.in.font_addr = (u_int32_t *)(ip->fbbase + sti.fontbase);
	a.in.index = c;

	unpmv = (sti_unpmv_t)(sti.code + sti.codeptr[STI_FONT_UNPMV]);
	(*unpmv)(&a.flags, &a.in, &a.out, &sti.cfg);
}

void
sti_cursor(struct ite_data *ip, int flag)
{
	switch (flag) {
	case MOVE_CURSOR:
		sti_do_cursor(ip);
		/* FALLTHROUGH */
	case DRAW_CURSOR:
		ip->cursorx = ip->curx;
		ip->cursory = ip->cury;
		/* FALLTHROUGH */
	default:
		sti_do_cursor(ip);
		break;
	}
}

void
sti_do_cursor(struct ite_data *ip)
{
	sti_blkmv_t blkmv;
	struct {
		struct sti_blkmvflags flags;
		struct sti_blkmvin in;
		struct sti_blkmvout out;
	} a;

	bzero(&a, sizeof a);
	a.flags.flags = STI_BLKMVF_WAIT | STI_BLKMVF_COLR;
	a.in.fg_colour = STI_COLOUR_BLACK;
	a.in.bg_colour = STI_COLOUR_WHITE;
	a.in.dstx = a.in.srcx = ip->cursorx * ip->ftwidth;
	a.in.dsty = a.in.srcy = ip->cursory * ip->ftheight;
	a.in.width = ip->ftwidth;
	a.in.height = ip->ftheight;

	blkmv = (sti_blkmv_t)(sti.code + sti.codeptr[STI_BLOCK_MOVE]);
	(*blkmv)(&a.flags, &a.in, &a.out, &sti.cfg);
}

void
sti_clear(struct ite_data *ip, int sy, int sx, int h, int w)
{
	sti_blkmv_t blkmv;
	struct {
		struct sti_blkmvflags flags;
		struct sti_blkmvin in;
		struct sti_blkmvout out;
	} a;

	bzero(&a, sizeof a);
	a.flags.flags = STI_BLKMVF_WAIT | STI_BLKMVF_CLR;
	a.in.bg_colour = STI_COLOUR_BLACK;
	a.in.dstx = a.in.srcx = sx * ip->ftwidth;
	a.in.dsty = a.in.srcy = sy * ip->ftheight;
	a.in.width = w * ip->ftwidth;
	a.in.height = h * ip->ftheight;

	blkmv = (sti_blkmv_t)(sti.code + sti.codeptr[STI_BLOCK_MOVE]);
	(*blkmv)(&a.flags, &a.in, &a.out, &sti.cfg);
}

void
sti_scroll(struct ite_data *ip)
{
	sti_blkmv_t blkmv;
	struct {
		struct sti_blkmvflags flags;
		struct sti_blkmvin in;
		struct sti_blkmvout out;
	} a;

	bzero(&a, sizeof a);
	a.flags.flags = STI_BLKMVF_WAIT;
	a.in.bg_colour = STI_COLOUR_BLACK;
	a.in.fg_colour = STI_COLOUR_WHITE;
	a.in.dstx = a.in.srcx = 0;
	a.in.dsty = 0;
	a.in.srcy = ip->ftheight;
	a.in.width = ip->dwidth;
	a.in.height = (ip->rows - 1) * ip->ftheight;

	blkmv = (sti_blkmv_t)(sti.code + sti.codeptr[STI_BLOCK_MOVE]);
	(*blkmv)(&a.flags, &a.in, &a.out, &sti.cfg);
}

void
sti_fontinfo(struct ite_data *ip)
{
	u_int32_t fontbase;

	fontbase = sti.fontbase = parseword1((u_int8_t *)ip->fbbase, 0x30) & ~3;
	ip->ftwidth = (u_int8_t)ip->fbbase[fontbase + 0x13];
	ip->ftheight = (u_int8_t)ip->fbbase[fontbase + 0x17];
	ip->rows = ip->dheight / ip->ftheight;
	ip->cols = ip->dwidth / ip->ftwidth;
}

void
sti_init(int full)
{
	sti_init_t init;
	struct {
		struct sti_initflags flags;
		struct sti_initin in;
		struct sti_initout out;
	} a;

	bzero(&a, sizeof a);
	a.flags.flags = STI_INITF_WAIT | STI_INITF_CMB | STI_INITF_EBET;
	if (full)
		a.flags.flags |= STI_INITF_TEXT | STI_INITF_PBET |
		    STI_INITF_PBETI | STI_INITF_ICMT;
	a.in.text_planes = 1;

	init = (sti_init_t)(sti.code + sti.codeptr[STI_INIT_GRAPH]);
	(*init)(&a.flags, &a.in, &a.out, &sti.cfg);
}

void
sti_inqcfg(struct sti_inqconfout *ico)
{
	sti_inqconf_t inqconf;
	struct {
		struct sti_inqconfflags flags;
		struct sti_inqconfin in;
	} a;

	bzero(&a, sizeof a);
	a.flags.flags = STI_INQCONFF_WAIT;

	inqconf = (sti_inqconf_t)(sti.code + sti.codeptr[STI_INQ_CONF]);
	(*inqconf)(&a.flags, &a.in, ico, &sti.cfg);
}

#endif
