/*	$OpenBSD: extern.h,v 1.12 2004/04/14 19:53:04 deraadt Exp $	*/
/*	$NetBSD: extern.h,v 1.3 1996/05/10 23:16:34 thorpej Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *      @(#)extern.h	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>
#include <fcntl.h>
#include <kvm.h>

extern struct	cmdtab *curcmd;
extern struct	cmdtab cmdtab[];
extern struct	text *xtext;
extern WINDOW	*wnd;
extern char	**dr_name;
extern char	c, *namp, hostname[];
extern double	avenrun[3];
extern float	*dk_mspw;
extern kvm_t	*kd;
extern long	ntext, textp;
extern int	*dk_select;
extern long	CMDLINE;
extern int	dk_ndrive;
extern int	hz, stathz;
extern int	naptime, col;
extern int	nhosts;
extern int	nports;
extern int	protos;
extern int	verbose;

struct inpcb;

int	 checkhost(struct inpcb *);
int	 checkport(struct inpcb *);
void	 closeiostat(WINDOW *);
void	 closekre(WINDOW *);
void	 closembufs(WINDOW *);
void	 closenetstat(WINDOW *);
void	 closepigs(WINDOW *);
void	 closeswap(WINDOW *);
int	 cmdiostat(char *, char *);
int	 cmdkre(char *, char *);
int	 cmdnetstat(char *, char *);
struct	 cmdtab *lookup(char *);
void	 command(char *);
void	 sigdie(int);
void	 sigtstp(int);
void	 die(void);
void	 sigdisplay(int);
void	 display(void);
int	 dkinit(int);
int	 dkcmd(char *, char *);
void	 error(const char *fmt, ...);
void	 fetchiostat(void);
void	 fetchkre(void);
void	 fetchmbufs(void);
void	 fetchnetstat(void);
void	 fetchpigs(void);
void	 fetchswap(void);
int	 initiostat(void);
int	 initkre(void);
int	 initmbufs(void);
int	 initnetstat(void);
int	 initpigs(void);
int	 initswap(void);
void	 keyboard(void);
int	 kvm_ckread(void *, void *, int);
void	 labeliostat(void);
void	 labelkre(void);
void	 labelmbufs(void);
void	 labelnetstat(void);
void	 labelpigs(void);
void	 labels(void);
void	 labelswap(void);
void	 load(void);
int	 netcmd(char *, char *);
void	 nlisterr(struct nlist []);
WINDOW	*openiostat(void);
WINDOW	*openkre(void);
WINDOW	*openmbufs(void);
WINDOW	*opennetstat(void);
WINDOW	*openpigs(void);
WINDOW	*openswap(void);
int	 prefix(char *, char *);
void	 sigwinch(int);
void	 showiostat(void);
void	 showkre(void);
void	 showmbufs(void);
void	 shownetstat(void);
void	 showpigs(void);
void	 showswap(void);
void	 status(void);
void	 gethz(void);

extern volatile sig_atomic_t gotdie;
extern volatile sig_atomic_t gotdisplay;
extern volatile sig_atomic_t gotwinch;
extern volatile sig_atomic_t gottstp;

extern double dellave;
extern WINDOW *wload;
