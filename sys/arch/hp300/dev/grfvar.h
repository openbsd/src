/*	$OpenBSD: grfvar.h,v 1.7 2002/03/14 01:26:30 millert Exp $	*/
/*	$NetBSD: grfvar.h,v 1.10 1997/03/31 07:34:19 scottr Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: grfvar.h 1.11 93/08/13$
 *
 *	@(#)grfvar.h	8.2 (Berkeley) 9/9/93
 */

/* internal structure of lock page */
#define GRFMAXLCK	256
struct	grf_lockpage {
	u_char	gl_locks[GRFMAXLCK];
};
#define gl_lockslot gl_locks[0]

/* per display info */
struct	grf_data {
	int	g_flags;		/* software flags */
	struct  grfsw *g_sw;		/* static configuration info */
	caddr_t	g_regkva;		/* KVA of registers */
	caddr_t	g_fbkva;		/* KVA of framebuffer */
	struct	grfinfo g_display;	/* hardware description (for ioctl) */
	struct	grf_lockpage *g_lock;	/* lock page associated with device */
	struct	proc *g_lockp;		/* process holding lock */
	short	*g_pid;			/* array of pids with device open */
	int	g_lockpslot;		/* g_pid entry of g_lockp */
	caddr_t	g_data;			/* device dependent data */
};

/*
 * Static configuration info for display types
 */
struct	grfsw {
	int	gd_hwid;	/* id returned by hardware */
	int	gd_swid;	/* id to be returned by software */
	char	*gd_desc;	/* description printed at config time */
				/* boot time init routine */
	int	(*gd_init)(struct grf_data *, int, caddr_t);
				/* misc function routine */
	int	(*gd_mode)(struct grf_data *, int, caddr_t);
};

struct	grf_softc {
	struct	device sc_dev;		/* generic device info */
	int	sc_scode;		/* select code; for grfdevno() */
	struct	grf_data *sc_data;	/* display state information */
	struct	ite_softc *sc_ite;	/* pointer to ite; may be NULL */
};

struct	grfdev_softc {
	struct	device sc_dev;		/* generic device info */
	struct	grf_data *sc_data;	/* generic grf data */
	int	sc_scode;		/* select code, -1 for intio */
};

/*
 * Set up by the hardware driver, and passed all the way down to
 * the ITE, if appropriate.
 */
struct	grfdev_attach_args {
	int	ga_scode;		/* XXX select code, -1 for intio */
	int	ga_isconsole;		/* from hardware; is console? */
	void	*ga_data;		/* hardware-dependent data */
	void	*ga_ite;		/* ITE switch table */
};

/* flags */
#define	GF_ALIVE	0x01
#define GF_OPEN		0x02
#define GF_EXCLUDE	0x04
#define GF_WANTED	0x08
#define GF_BSDOPEN	0x10
#define GF_HPUXOPEN	0x20

/* requests to mode routine */
#define GM_GRFON	1
#define GM_GRFOFF	2
#define GM_GRFOVON	3
#define GM_GRFOVOFF	4
#define GM_DESCRIBE	5
#define GM_MAP		6
#define GM_UNMAP	7

/* minor device interpretation */
#define GRFOVDEV	0x10	/* overlay planes */
#define GRFIMDEV	0x20	/* images planes */
#define GRFUNIT(d)	((d) & 0x7)

#ifdef _KERNEL
extern	struct grf_data grf_cn;		/* grf_data for console device */

/* grf.c prototypes */
int	grfmap(dev_t, caddr_t *, struct proc *);
int	grfunmap(dev_t, caddr_t, struct proc *);
int	grfon(dev_t);
int	grfoff(dev_t);
int	grfaddr(struct grf_softc *, int);

#ifdef COMPAT_HPUX
int	hpuxgrfioctl(dev_t, int, caddr_t, int, struct proc *);

int	grflock(struct grf_data *, int);
int	grfunlock(struct grf_data *);
int	grfdevno(dev_t);

int	iommap(dev_t, caddr_t *);
int	iounmmap(dev_t, caddr_t);

int	grffindpid(struct grf_data *);
void	grfrmpid(struct grf_data *);
int	grflckmmap(dev_t, caddr_t *);
int	grflckunmmap(dev_t, caddr_t);
#endif /* COMPAT_HPUX */

/* grf_subr.c prototypes */
struct itesw;
void	grfdev_attach __P((struct grfdev_softc *,
	    int (*init)(struct grf_data *, int, caddr_t),
	    caddr_t, struct grfsw *, struct itesw *itesw));
#endif /* _KERNEL */
