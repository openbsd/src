/*	$OpenBSD: cons.h,v 1.6 1998/06/17 14:58:34 mickey Exp $	*/
/*	$NetBSD: cons.h,v 1.14 1996/03/14 19:08:35 christos Exp $	*/

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
 * from: Utah $Hdr: cons.h 1.6 92/01/21$
 *
 *	@(#)cons.h	8.1 (Berkeley) 6/10/93
 */

struct consdev {
	void	(*cn_probe)	/* probe hardware and fill in consdev info */
		    __P((struct consdev *));
	void	(*cn_init)	/* turn on as console */
		    __P((struct consdev *));
	int	(*cn_getc)	/* kernel getchar interface */
		    __P((dev_t));
	void	(*cn_putc)	/* kernel putchar interface */
		    __P((dev_t, int));
	void	(*cn_pollc)	/* turn on and off polling */
		    __P((dev_t, int));
	dev_t	cn_dev;		/* major/minor of device */
	int	cn_pri;		/* pecking order; the higher the better */
};

/* values for cn_pri - reflect our policy for console selection */
#define	CN_DEAD		0	/* device doesn't exist */
#define CN_NORMAL	1	/* device exists but is nothing special */
#define CN_INTERNAL	2	/* "internal" bit-mapped display */
#define CN_REMOTE	3	/* serial interface with remote bit set */

/* XXX */
#define	CONSMAJOR	0

#ifdef _KERNEL

extern	struct consdev constab[];
extern	struct consdev *cn_tab;

void	cninit __P((void));
int	cnset __P((dev_t));
int	cnopen __P((dev_t, int, int, struct proc *));
int	cnclose __P((dev_t, int, int, struct proc *));
int	cnread __P((dev_t, struct uio *, int));
int	cnwrite __P((dev_t, struct uio *, int));
int	cnioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int	cnselect __P((dev_t, int, struct proc *));
int	cngetc __P((void));
void	cnputc __P((int));
void	cnpollc __P((int));
void	cnrint __P((void));
void	nullcnpollc __P((dev_t, int));

/* console-specific types */
#define	dev_type_cnprobe(n)	void n __P((struct consdev *))
#define	dev_type_cninit(n)	void n __P((struct consdev *))
#define	dev_type_cngetc(n)	int n __P((dev_t))
#define	dev_type_cnputc(n)	void n __P((dev_t, int))
#define	dev_type_cnpollc(n)	void n __P((dev_t, int))

#define	cons_decl(n) \
	dev_decl(n,cnprobe); dev_decl(n,cninit); dev_decl(n,cngetc); \
	dev_decl(n,cnputc); dev_decl(n,cnpollc)

#define	cons_init(n) { \
	dev_init(1,n,cnprobe), dev_init(1,n,cninit), dev_init(1,n,cngetc), \
	dev_init(1,n,cnputc), dev_init(1,n,cnpollc) }

#endif
