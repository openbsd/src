/*	$OpenBSD: smg.c,v 1.9 2006/01/17 20:26:16 miod Exp $	*/
/*	$NetBSD: smg.c,v 1.21 2000/03/23 06:46:44 thorpej Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/timeout.h>

#include <machine/vsbus.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/ka420.h>

#include <dev/cons.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>
#include <vax/dec/dzkbdvar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_callbacks.h>

#include "dzkbd.h"

/* Safety guard */
#ifndef FONT_QVSS8x15
#include <dev/wsfont/qvss8x15.h>
#endif

/* Screen hardware defs */
#define SM_COLS		128	/* char width of screen */
#define SM_ROWS		57	/* rows of char on screen */
#define SM_CHEIGHT	15	/* lines a char consists of */
#define SM_NEXTROW	(SM_COLS * SM_CHEIGHT)
#define SM_YWIDTH	864
#define SM_XWIDTH	1024

/* Cursor register definitions */
#define CUR_CMD		0
#define CUR_XPOS	4
#define CUR_YPOS	8
#define CUR_XMIN_1	12
#define CUR_XMAX_1	16
#define CUR_YMIN_1	20
#define CUR_YMAX_1	24
#define CUR_XMIN_2	28
#define CUR_XMAX_2	32
#define CUR_YMIN_2	36
#define CUR_YMAX_2	40
#define CUR_LOAD	44

#define CUR_CMD_TEST	0x8000
#define CUR_CMD_HSHI	0x4000
#define CUR_CMD_VBHI	0x2000
#define CUR_CMD_LODSA	0x1000
#define CUR_CMD_FORG2	0x0800
#define CUR_CMD_ENRG2	0x0400
#define CUR_CMD_FORG1	0x0200
#define CUR_CMD_ENRG1	0x0100
#define CUR_CMD_XHWID	0x0080
#define CUR_CMD_XHCL1	0x0040
#define CUR_CMD_XHCLP	0x0020
#define CUR_CMD_XHAIR	0x0010
#define CUR_CMD_FOPB	0x0008
#define CUR_CMD_ENPB	0x0004
#define CUR_CMD_FOPA	0x0002
#define CUR_CMD_ENPA	0x0001

#define CUR_XBIAS	216	/* Add to cursor position */
#define CUR_YBIAS	33

#define	WRITECUR(addr, val)	*(volatile short *)(curaddr + (addr)) = (val)
static	caddr_t	curaddr;
static	short curcmd, curx, cury, hotX, hotY;
static	int bgmask, fgmask; 

static	int smg_match(struct device *, struct cfdata *, void *);
static	void smg_attach(struct device *, struct device *, void *);

struct	smg_softc {
	struct	device ss_dev;
};

struct cfattach smg_ca = {
	sizeof(struct smg_softc), (cfmatch_t)smg_match, smg_attach,
};

struct	cfdriver smg_cd = {
	NULL, "smg", DV_DULL
};

static void	smg_cursor(void *, int, int, int);
static int	smg_mapchar(void *, int, unsigned int *);
static void	smg_putchar(void *, int, int, u_int, long);
static void	smg_copycols(void *, int, int, int,int);
static void	smg_erasecols(void *, int, int, int, long);
static void	smg_copyrows(void *, int, int, int);
static void	smg_eraserows(void *, int, int, long);
static int	smg_alloc_attr(void *, int, int, int, long *);

const struct wsdisplay_emulops smg_emulops = {
	smg_cursor,
	smg_mapchar,
	smg_putchar,
	smg_copycols,
	smg_erasecols,
	smg_copyrows,
	smg_eraserows,
	smg_alloc_attr
};

const struct wsscreen_descr smg_stdscreen = {
	"128x57", SM_COLS, SM_ROWS,
	&smg_emulops,
	8, SM_CHEIGHT,
	WSSCREEN_UNDERLINE|WSSCREEN_REVERSE,
};

const struct wsscreen_descr *_smg_scrlist[] = {
	&smg_stdscreen,
};

const struct wsscreen_list smg_screenlist = {
	sizeof(_smg_scrlist) / sizeof(struct wsscreen_descr *),
	_smg_scrlist,
};

static	caddr_t	sm_addr;

extern struct wsdisplay_font qvss8x15;
static u_char *qf;

#define QCHAR(c) (c < 32 ? 32 : (c > 127 ? c - 66 : c - 32))
#define QFONT(c,line)	qf[QCHAR(c) * 15 + line]
#define	SM_ADDR(row, col, line) \
	sm_addr[col + (row * SM_CHEIGHT * SM_COLS) + line * SM_COLS]


static int	smg_ioctl(void *, u_long, caddr_t, int, struct proc *);
static paddr_t	smg_mmap(void *, off_t, int);
static int	smg_alloc_screen(void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *);
static void	smg_free_screen(void *, void *);
static int	smg_show_screen(void *, void *, int,
				     void (*) (void *, int, int), void *);
static void	smg_crsr_blink(void *);

const struct wsdisplay_accessops smg_accessops = {
	smg_ioctl,
	smg_mmap,
	smg_alloc_screen,
	smg_free_screen,
	smg_show_screen,
	0 /* load_font */
};

struct	smg_screen {
	int	ss_curx;
	int	ss_cury;
	u_char	ss_image[SM_ROWS][SM_COLS];	/* Image of current screen */
	u_char	ss_attr[SM_ROWS][SM_COLS];	/* Reversed etc... */
};

static	struct smg_screen smg_conscreen;
static	struct smg_screen *curscr;

static	struct timeout smg_blink_timeout;

extern int getmajor(void *);	/* conf.c */

int
smg_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct vsbus_attach_args *va = aux;
	volatile short *curcmd;
	volatile short *cfgtst;
	short tmp, tmp2;

	if (vax_boardtype == VAX_BTYP_49 || vax_boardtype == VAX_BTYP_1303)
		return 0;

	curcmd = (short *)va->va_addr;
	cfgtst = (short *)vax_map_physmem(VS_CFGTST, 1);
	/*
	 * Try to find the cursor chip by testing the flip-flop.
	 * If nonexistent, no glass tty.
	 */
	curcmd[0] = CUR_CMD_HSHI|CUR_CMD_FOPB;
	DELAY(300000);
	tmp = cfgtst[0];
	curcmd[0] = CUR_CMD_TEST|CUR_CMD_HSHI;
	DELAY(300000);
	tmp2 = cfgtst[0];
	vax_unmap_physmem((vaddr_t)cfgtst, 1);

	if (tmp2 != tmp)
		return 20; /* Using periodic interrupt */
	else
		return 0;
}

void
smg_attach(struct device *parent, struct device *self, void *aux)
{
	struct wsemuldisplaydev_attach_args aa;

	printf("\n");
	sm_addr = (caddr_t)vax_map_physmem(SMADDR, (SMSIZE/VAX_NBPG));
	curaddr = (caddr_t)vax_map_physmem(KA420_CUR_BASE, 1);
	if (sm_addr == 0) {
		printf("%s: Couldn't alloc graphics memory.\n", self->dv_xname);
		return;
	}
	curscr = &smg_conscreen;
	aa.console = !(vax_confdata & 0x20);
	aa.scrdata = &smg_screenlist;
	aa.accessops = &smg_accessops;
	timeout_set(&smg_blink_timeout, smg_crsr_blink, NULL);
	timeout_add(&smg_blink_timeout, hz / 2);
	curcmd = CUR_CMD_HSHI;
	WRITECUR(CUR_CMD, curcmd);
	qf = qvss8x15.data;

	config_found(self, &aa, wsemuldisplaydevprint);
}

static	u_char *cursor;
static	int cur_on;

static void
smg_crsr_blink(void *arg)
{
	if (cur_on)
		*cursor ^= 255;
	timeout_add(&smg_blink_timeout, hz / 2);
}

void
smg_cursor(void *id, int on, int row, int col)
{
	struct smg_screen *ss = id;

	if (ss == curscr) {
		SM_ADDR(ss->ss_cury, ss->ss_curx, 14) =
		    QFONT(ss->ss_image[ss->ss_cury][ss->ss_curx], 14);
		cursor = &SM_ADDR(row, col, 14);
		if ((cur_on = on))
			*cursor ^= 255;
	}
	ss->ss_curx = col;
	ss->ss_cury = row;
}

int
smg_mapchar(void *id, int uni, unsigned int *index)
{
	if (uni < 256) {
		*index = uni;
		return (5);
	}
	*index = ' ';
	return (0);
}

static void
smg_putchar(void *id, int row, int col, u_int c, long attr)
{
	struct smg_screen *ss = id;
	int i;

	c &= 0xff;

	ss->ss_image[row][col] = c;
	ss->ss_attr[row][col] = attr;
	if (ss != curscr)
		return;
	for (i = 0; i < 15; i++) {
		unsigned char ch = QFONT(c, i);

		SM_ADDR(row, col, i) = (attr & WSATTR_REVERSE ? ~ch : ch);
		
	}
	if (attr & WSATTR_UNDERLINE)
		SM_ADDR(row, col, 14) ^= SM_ADDR(row, col, 14);
}

/*
 * copies columns inside a row.
 */
static void
smg_copycols(void *id, int row, int srccol, int dstcol, int ncols)
{
	struct smg_screen *ss = id;
	int i;

	bcopy(&ss->ss_image[row][srccol], &ss->ss_image[row][dstcol], ncols);
	bcopy(&ss->ss_attr[row][srccol], &ss->ss_attr[row][dstcol], ncols);
	if (ss != curscr)
		return;
	for (i = 0; i < SM_CHEIGHT; i++)
		bcopy(&SM_ADDR(row,srccol, i), &SM_ADDR(row, dstcol, i),ncols);
}

/*
 * Erases a bunch of chars inside one row.
 */
static void
smg_erasecols(void *id, int row, int startcol, int ncols, long fillattr)
{
	struct smg_screen *ss = id;
	int i;

	bzero(&ss->ss_image[row][startcol], ncols);
	bzero(&ss->ss_attr[row][startcol], ncols);
	if (ss != curscr)
		return;
	for (i = 0; i < SM_CHEIGHT; i++)
		bzero(&SM_ADDR(row, startcol, i), ncols);
}

static void
smg_copyrows(void *id, int srcrow, int dstrow, int nrows)
{
	struct smg_screen *ss = id;
	int frows;

	bcopy(&ss->ss_image[srcrow][0], &ss->ss_image[dstrow][0],
	    nrows * SM_COLS);
	bcopy(&ss->ss_attr[srcrow][0], &ss->ss_attr[dstrow][0],
	    nrows * SM_COLS);
	if (ss != curscr)
		return;
	if (nrows > 25) {
		frows = nrows >> 1;
		if (srcrow > dstrow) {
			bcopy(&sm_addr[(srcrow * SM_NEXTROW)],
			    &sm_addr[(dstrow * SM_NEXTROW)],
			    frows * SM_NEXTROW);
			bcopy(&sm_addr[((srcrow + frows) * SM_NEXTROW)],
			    &sm_addr[((dstrow + frows) * SM_NEXTROW)],
			    (nrows - frows) * SM_NEXTROW);
		} else {
			bcopy(&sm_addr[((srcrow + frows) * SM_NEXTROW)],
			    &sm_addr[((dstrow + frows) * SM_NEXTROW)],
			    (nrows - frows) * SM_NEXTROW);
			bcopy(&sm_addr[(srcrow * SM_NEXTROW)],
			    &sm_addr[(dstrow * SM_NEXTROW)],
			    frows * SM_NEXTROW);
		}
	} else
		bcopy(&sm_addr[(srcrow * SM_NEXTROW)],
		    &sm_addr[(dstrow * SM_NEXTROW)], nrows * SM_NEXTROW);
}

static void
smg_eraserows(void *id, int startrow, int nrows, long fillattr)
{
	struct smg_screen *ss = id;
	int frows;

	bzero(&ss->ss_image[startrow][0], nrows * SM_COLS);
	bzero(&ss->ss_attr[startrow][0], nrows * SM_COLS);
	if (ss != curscr)
		return;
	if (nrows > 25) {
		frows = nrows >> 1;
		bzero(&sm_addr[(startrow * SM_NEXTROW)], frows * SM_NEXTROW);
		bzero(&sm_addr[((startrow + frows) * SM_NEXTROW)],
		    (nrows - frows) * SM_NEXTROW);
	} else
		bzero(&sm_addr[(startrow * SM_NEXTROW)], nrows * SM_NEXTROW);
}

static int
smg_alloc_attr(void *id, int fg, int bg, int flags, long *attrp)
{
	*attrp = flags;
	return 0;
}

static void
setcursor(struct wsdisplay_cursor *v)
{
	u_short red, green, blue, curfg[16], curmask[16];
	int i;

	/* Enable cursor */
	if (v->which & WSDISPLAY_CURSOR_DOCUR) {
		if (v->enable)
			curcmd |= CUR_CMD_ENPB|CUR_CMD_ENPA;
		else
			curcmd &= ~(CUR_CMD_ENPB|CUR_CMD_ENPA);
		WRITECUR(CUR_CMD, curcmd);
	}
	if (v->which & WSDISPLAY_CURSOR_DOHOT) {
		hotX = v->hot.x;
		hotY = v->hot.y;
	}
	if (v->which & WSDISPLAY_CURSOR_DOCMAP) {
		/* First background */
		copyin(v->cmap.red, &red, sizeof(u_short));
		copyin(v->cmap.green, &green, sizeof(u_short));
		copyin(v->cmap.blue, &blue, sizeof(u_short));
		bgmask = (((30L * red + 59L * green + 11L * blue) >> 8) >=
		    (((1<<8)-1)*50)) ? ~0 : 0;
		copyin(v->cmap.red + 2, &red, sizeof(u_short));
		copyin(v->cmap.green + 2, &green, sizeof(u_short));
		copyin(v->cmap.blue + 2, &blue, sizeof(u_short));
		fgmask = (((30L * red + 59L * green + 11L * blue) >> 8) >=
		    (((1<<8)-1)*50)) ? ~0 : 0;
	}
	if (v->which & WSDISPLAY_CURSOR_DOSHAPE) {
		WRITECUR(CUR_CMD, curcmd | CUR_CMD_LODSA);
		copyin(v->image, curfg, sizeof(curfg));
		copyin(v->mask, curmask, sizeof(curmask));
		for (i = 0; i < sizeof(curfg)/2; i++) {
			WRITECUR(CUR_LOAD, (curfg[i] & fgmask) |
			    ((curmask[i] & ~curfg[i]) & bgmask));
		}
		for (i = 0; i < sizeof(curmask)/2; i++) {
			WRITECUR(CUR_LOAD, curmask[i]);
		}
		WRITECUR(CUR_CMD, curcmd);
	}
}

int
smg_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct wsdisplay_fbinfo *fb = (void *)data;
	static short curc;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_VAX_MONO;
		break;

	case WSDISPLAYIO_GINFO:
		fb->height = SM_YWIDTH;
		fb->width = SM_XWIDTH;
		fb->depth = 1;
		fb->cmsize = 2;
		break;

	case WSDISPLAYIO_SVIDEO:
		if (*(u_int *)data == WSDISPLAYIO_VIDEO_ON) {
			curcmd = curc;
		} else {
			curc = curcmd;
			curcmd &= ~(CUR_CMD_FOPA|CUR_CMD_ENPA);
			curcmd |= CUR_CMD_FOPB;
		}
		WRITECUR(CUR_CMD, curcmd);
		break;

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = (curcmd & CUR_CMD_FOPB ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON);
		break;

	case WSDISPLAYIO_SCURSOR:
		setcursor((struct wsdisplay_cursor *)data);
		break;

	case WSDISPLAYIO_SCURPOS:
		curx = ((struct wsdisplay_curpos *)data)->x;
		cury = ((struct wsdisplay_curpos *)data)->y;
		WRITECUR(CUR_XPOS, curx + CUR_XBIAS);
		WRITECUR(CUR_YPOS, cury + CUR_YBIAS);
		break;

	case WSDISPLAYIO_GCURPOS:
		((struct wsdisplay_curpos *)data)->x = curx;
		((struct wsdisplay_curpos *)data)->y = cury;
		break;

	default:
		return ENOTTY;
	}
	return 0;
}

static paddr_t
smg_mmap(void *v, off_t offset, int prot)
{
	if (offset >= SMSIZE || offset < 0)
		return -1;
	return (SMADDR + offset) >> PGSHIFT;
}

int
smg_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	*cookiep = malloc(sizeof(struct smg_screen), M_DEVBUF, M_WAITOK);
	bzero(*cookiep, sizeof(struct smg_screen));
	*curxp = *curyp = *defattrp = 0;
	return 0;
}

void
smg_free_screen(void *v, void *cookie)
{
}

int
smg_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct smg_screen *ss = cookie;
	int row, col, line;

	if (ss == curscr)
		return (0);

	for (row = 0; row < SM_ROWS; row++)
		for (line = 0; line < SM_CHEIGHT; line++) {
			for (col = 0; col < SM_COLS; col++) {
				u_char s, c = ss->ss_image[row][col];

				if (c < 32)
					c = 32;
				s = QFONT(c, line);
				if (ss->ss_attr[row][col] & WSATTR_REVERSE)
					s ^= 255;
				SM_ADDR(row, col, line) = s;
			}
			if (ss->ss_attr[row][col] & WSATTR_UNDERLINE)
				SM_ADDR(row, col, line) = 255;
		}
	cursor = &sm_addr[(ss->ss_cury * SM_CHEIGHT * SM_COLS) + ss->ss_curx +
	    ((SM_CHEIGHT - 1) * SM_COLS)];
	curscr = ss;
	return (0);
}

cons_decl(smg);

void
smgcninit(cndev)
	struct	consdev *cndev;
{
	extern void lkccninit(struct consdev *);
	extern int lkccngetc(dev_t);
	extern int dz_vsbus_lk201_cnattach(int);
	/* Clear screen */
	memset(sm_addr, 0, 128*864);

	curscr = &smg_conscreen;
	wsdisplay_cnattach(&smg_stdscreen, &smg_conscreen, 0, 0, 0);
	cn_tab->cn_pri = CN_INTERNAL;
	qf = qvss8x15.data;

#if NDZKBD > 0
	dzkbd_cnattach(0); /* Connect keyboard and screen together */
#endif
}

/*
 * Called very early to setup the glass tty as console.
 * Because it's called before the VM system is inited, virtual memory
 * for the framebuffer can be stolen directly without disturbing anything.
 */
void
smgcnprobe(cndev)
	struct  consdev *cndev;
{
	extern vaddr_t virtual_avail;

	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if ((vax_confdata & KA420_CFG_L3CON) ||
		    (vax_confdata & KA420_CFG_MULTU))
			break; /* doesn't use graphics console */
		sm_addr = (caddr_t)virtual_avail;
		virtual_avail += SMSIZE;
		ioaccess((vaddr_t)sm_addr, SMADDR, (SMSIZE/VAX_NBPG));
		cndev->cn_pri = CN_INTERNAL;
		cndev->cn_dev = makedev(getmajor(wsdisplayopen), 0);
		break;

	default:
		break;
	}
}
