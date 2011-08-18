/*	$OpenBSD: itevar.h,v 1.6 2011/08/18 20:02:58 miod Exp $	*/
/*	$NetBSD: itevar.h,v 1.1 1996/03/03 04:23:42 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: itevar.h 1.15 92/12/20$
 *
 *	@(#)itevar.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Standalone version of hp300 ITE.
 */

struct ite_data;

typedef	void (*ite_windowmover)(struct ite_data *, int, int, int, int, int,
	    int, int);

struct ite_data {
	int	alive;
	int	scode;			/* DIO select code or SGC slot # */
	struct  itesw *isw;
	caddr_t regbase, fbbase;
	short	curx, cury;
	short   cursorx, cursory;
	short   cblankx, cblanky;
	short	rows, cols;
	short   cpl;
	short	dheight, dwidth;
	short	fbheight, fbwidth;
	short	ftheight, ftwidth;
	short	fontx, fonty;
	short	planemask;
	ite_windowmover bmv;
};

struct itesw {
	int	ite_hwid;
	int	(*ite_probe)(struct ite_data *);
	void	(*ite_init)(struct ite_data *);
	void	(*ite_clear)(struct ite_data *, int, int, int, int);
	void	(*ite_putc)(struct ite_data *, int, int, int);
	void	(*ite_cursor)(struct ite_data *, int);
	void	(*ite_scroll)(struct ite_data *);
};

/*
 * X and Y location of character 'c' in the framebuffer, in pixels.
 */
#define	charX(ip,c)	\
	(((c) % (ip)->cpl) * (ip)->ftwidth + (ip)->fontx)

#define	charX1bpp(ip,c)	\
	(((c) % (ip)->cpl) * ((((ip)->ftwidth + 7) / 8) * 8) + (ip)->fontx)

#define	charY(ip,c)	\
	(((c) / (ip)->cpl) * (ip)->ftheight + (ip)->fonty)

/* Replacement Rules */
#define RR_CLEAR		0x0
#define RR_COPY			0x3
#define RR_XOR			0x6
#define RR_COPYINVERTED  	0xc

#define DRAW_CURSOR	0x00
#define ERASE_CURSOR    0x01
#define MOVE_CURSOR	0x02

extern	struct ite_data ite_data[];
extern	struct itesw itesw[];
extern	int nitesw;

/*
 * Prototypes.
 */
void	ite_fontinfo(struct ite_data *);
void	ite_fontinit1bpp(struct ite_data *);
void	ite_fontinit8bpp(struct ite_data *);
void	ite_dio_clear(struct ite_data *, int, int, int, int);
void	ite_dio_cursor(struct ite_data *, int);
void	ite_dio_putc1bpp(struct ite_data *, int, int, int);
void	ite_dio_putc8bpp(struct ite_data *, int, int, int);
void	ite_dio_scroll(struct ite_data *);
void	ite_dio_windowmove1bpp(struct ite_data *, int, int, int, int,
	    int, int, int);

/*
 * Framebuffer-specific ITE prototypes.
 */
void	topcat_init(struct ite_data *);
void	gbox_init(struct ite_data *);
void	gbox_scroll(struct ite_data *);
void	rbox_init(struct ite_data *);
void	dvbox_init(struct ite_data *);
void	hyper_init(struct ite_data *);
void	tvrx_init(struct ite_data *);

int	sti_dio_probe(struct ite_data *);
void	sti_iteinit_dio(struct ite_data *);
void	sti_iteinit_sgc(struct ite_data *);
void	sti_clear(struct ite_data *, int, int, int, int);
void	sti_putc(struct ite_data *, int, int, int);
void	sti_cursor(struct ite_data *, int);
void	sti_scroll(struct ite_data *);
