/*	$NetBSD: fbreg.h,v 1.6 1996/09/21 03:25:20 jonathan Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)fbreg.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Data for fb.c generic frame buffer routines that are called by the
 * various frame buffer drivers.
 */
struct fbuaccess {
	PM_Info		scrInfo;
	pmEvent		events[PM_MAXEVQ];	
	pmTimeCoord	tcs[MOTION_BUFFER_SIZE];
};

struct pmax_fbtty {
	int	row, col;		/* Screen pos for glass tty */
	void	(*KBDPutc) __P ((dev_t dev, int c));
					/* Send char to keyboard func */
	dev_t	kbddev;			/* Device for KBDPutc */
};

/*
 * Mouse report structure definition
 */
typedef struct {
	char state;			/* buttons and sign bits	*/
	short dx;			/* delta X since last change	*/
	short dy;			/* delta Y since last change	*/
	char byteCount;			/* mouse report byte count	*/
} MouseReport;

/*
 * Macro to translate from a time struct to milliseconds.
 */
#define TO_MS(tv) ((tv.tv_sec * 1000) + (tv.tv_usec / 1000))

extern void fbKbdEvent __P((int, struct fbinfo *));
extern void fbMouseEvent __P((MouseReport *, struct fbinfo *));
extern void fbMouseButtons __P((MouseReport *, struct fbinfo *));
extern void fbScroll __P((struct fbinfo *));
extern void fbPutc __P((dev_t, int));
extern void fbBlitc __P((int, struct fbinfo *));
extern int tb_kbdmouseconfig __P((struct fbinfo *fi));
extern int fbmmap_fb __P((struct fbinfo *, dev_t, caddr_t, struct proc *));

extern void init_pmaxfbu __P((struct fbinfo *fi));
