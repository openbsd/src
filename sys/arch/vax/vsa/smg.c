/*	$OpenBSD: smg.c,v 1.1 2000/04/27 02:34:50 bjc Exp $	*/
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
#include <sys/callout.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/cons.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wscons_callbacks.h>

#include <machine/vsbus.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/ka420.h>

#include "lkc.h"

#define SM_COLS		128	/* char width of screen */
#define SM_ROWS		57	/* rows of char on screen */
#define SM_CHEIGHT	15	/* lines a char consists of */
#define SM_NEXTROW	(SM_COLS * SM_CHEIGHT)

static	int smg_match __P((struct device *, struct cfdata *, void *));
static	void smg_attach __P((struct device *, struct device *, void *));

struct	smg_softc {
	struct	device ss_dev;
};

struct cfattach smg_ca = {
	sizeof(struct smg_softc), smg_match, smg_attach,
};

static void	smg_cursor __P((void *, int, int, int));
static int	smg_mapchar __P((void *, int, unsigned int *));
static void	smg_putchar __P((void *, int, int, u_int, long));
static void	smg_copycols __P((void *, int, int, int,int));
static void	smg_erasecols __P((void *, int, int, int, long));
static void	smg_copyrows __P((void *, int, int, int));
static void	smg_eraserows __P((void *, int, int, long));
static int	smg_alloc_attr __P((void *, int, int, int, long *));

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

extern char q_font[];
#define QCHAR(c) (c < 32 ? 32 : (c > 127 ? c - 66 : c - 32))
#define QFONT(c,line)	q_font[QCHAR(c) * 15 + line]
#define	SM_ADDR(row, col, line) \
	sm_addr[col + (row * SM_CHEIGHT * SM_COLS) + line * SM_COLS]


static int	smg_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
static int	smg_mmap __P((void *, off_t, int));
static int	smg_alloc_screen __P((void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *));
static void	smg_free_screen __P((void *, void *));
static int	smg_show_screen __P((void *, void *, int,
				     void (*) (void *, int, int), void *));
static void	smg_crsr_blink __P((void *));

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

static	struct callout smg_cursor_ch = CALLOUT_INITIALIZER;

int
smg_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	volatile short *curcmd;
	volatile short *cfgtst;
	short tmp, tmp2;

	if (vax_boardtype == VAX_BTYP_49)
		return 0;

	curcmd = (short *)va->va_addr;
	cfgtst = (short *)vax_map_physmem(VS_CFGTST, 1);
	/*
	 * Try to find the cursor chip by testing the flip-flop.
	 * If nonexistent, no glass tty.
	 */
	curcmd[0] = 0x7fff;
	DELAY(300000);
	tmp = cfgtst[0];
	curcmd[0] = 0x8000;
	DELAY(300000);
	tmp2 = cfgtst[0];
	vax_unmap_physmem((vaddr_t)cfgtst, 1);

	if (tmp2 != tmp)
		return 20; /* Using periodic interrupt */
	else
		return 0;
}

void
smg_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wsemuldisplaydev_attach_args aa;

	printf("\n");
	sm_addr = (caddr_t)vax_map_physmem(SMADDR, (SMSIZE/VAX_NBPG));
	if (sm_addr == 0) {
		printf("%s: Couldn't alloc graphics memory.\n", self->dv_xname);
		return;
	}
	curscr = &smg_conscreen;
	aa.console = !(vax_confdata & 0x20);
	aa.scrdata = &smg_screenlist;
	aa.accessops = &smg_accessops;
	callout_reset(&smc_cursor_ch, hz / 2, smg_crsr_blink, NULL);

	config_found(self, &aa, wsemuldisplaydevprint);
}

static	u_char *cursor;
static	int cur_on;

static void
smg_crsr_blink(arg)
	void *arg;
{
	if (cur_on)
		*cursor ^= 255;
	callout_reset(&smg_cursor_ch, hz / 2, smg_crsr_blink, NULL);
}

void
smg_cursor(id, on, row, col)
	void *id;
	int on, row, col;
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
smg_mapchar(id, uni, index)
	void *id;
	int uni;
	unsigned int *index;
{
	if (uni < 256) {
		*index = uni;
		return (5);
	}
	*index = ' ';
	return (0);
}

static void
smg_putchar(id, row, col, c, attr)
	void *id;
	int row, col;
	u_int c;
	long attr;
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
smg_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
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
smg_erasecols(id, row, startcol, ncols, fillattr)
	void *id;
	int row, startcol, ncols;
	long fillattr;
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
smg_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
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
smg_eraserows(id, startrow, nrows, fillattr)
	void *id;
	int startrow, nrows;
	long fillattr;
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
smg_alloc_attr(id, fg, bg, flags, attrp)
	void *id;
	int fg, bg;
	int flags;
	long *attrp;
{
	*attrp = flags;
	return 0;
}

int
smg_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct wsdisplay_fbinfo fb;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PM_MONO;
		break;

	case WSDISPLAYIO_GINFO:
		fb.height = 864;
		fb.width = 1024;
		return copyout(&fb, data, sizeof(struct wsdisplay_fbinfo));

	
	default:
		return -1;
	}
	return 0;
}

static int
smg_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	if (offset >= SMSIZE || offset < 0)
		return -1;
	return (SMADDR + offset) >> PGSHIFT;
}

int
smg_alloc_screen(v, type, cookiep, curxp, curyp, defattrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *defattrp;
{
	*cookiep = malloc(sizeof(struct smg_screen), M_DEVBUF, M_WAITOK);
	bzero(*cookiep, sizeof(struct smg_screen));
	*curxp = *curyp = *defattrp = 0;
	return 0;
}

void
smg_free_screen(v, cookie)
	void *v;
	void *cookie;
{
}

int
smg_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb) __P((void *, int, int));
	void *cbarg;
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

#define WSCONSOLEMAJOR 68

void
smgcninit(cndev)
	struct	consdev *cndev;
{
	extern void lkccninit __P((struct consdev *));
	extern int lkccngetc __P((dev_t));
	/* Clear screen */
	memset(sm_addr, 0, 128*864);

	curscr = &smg_conscreen;
	wsdisplay_cnattach(&smg_stdscreen, &smg_conscreen, 0, 0, 0);
	cn_tab->cn_pri = CN_INTERNAL;
#if 0
	lkccninit(cndev);
	wsdisplay_set_cons_kbd(lkccngetc, nullcnpollc);
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
		cndev->cn_dev = makedev(WSCONSOLEMAJOR, 0);
		break;

	default:
		break;
	}
}
