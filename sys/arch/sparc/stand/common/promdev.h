/*	$OpenBSD: promdev.h,v 1.5 2011/04/14 18:27:49 miod Exp $	*/
/*	$NetBSD: promdev.h,v 1.3 1995/09/18 21:31:50 pk Exp $ */

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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

#include <machine/bsd_openprom.h>

struct promdata {
	int	fd;			/* Openboot descriptor */
	struct	saioreq *si;		/* Oldmon IO request */
	int	devtype;		/* Kind of device we're booting from */
#define DT_BLOCK	1
#define DT_NET		2
#define DT_BYTE		3
	/* Hooks for netif.c */
	ssize_t	(*xmit)(struct promdata *, void *, size_t);
	ssize_t	(*recv)(struct promdata *, void *, size_t);
};

#define PROM_LOADADDR	0x4000
#define DDB_MAGIC0	( ('D'<<24) | ('D'<<16) | ('B'<<8) | ('0') )
#define DDB_MAGIC1	( ('D'<<24) | ('D'<<16) | ('B'<<8) | ('1') )
#define DDB_MAGIC	DDB_MAGIC0

extern struct promvec	*promvec;
extern char	*prom_bootdevice;
extern char	*prom_bootfile;
extern int	prom_boothow;
extern int	hz;
extern int	cputyp, nbpg, pgofset, pgshift;
extern int	debug;

void	prom_init(void);
void	prom_getether(int, u_char *);
struct idprom *prom_getidprom(void);
void	prom_interpret(char *);
int	prom_makememarr(struct memarr *, u_int, int);

/* Note: dvma_*() routines are for "oldmon" machines only */
void	dvma_init(void);	
char	*dvma_mapin(char *, size_t);
char	*dvma_mapout(char *, size_t);
char	*dvma_alloc(int);
void	dvma_free(char *, int);

int	getticks(void);

int	mmu_init(void);
extern int	(*pmap_map)(vaddr_t, paddr_t, psize_t);
extern int	(*pmap_extract)(vaddr_t, paddr_t *);

int	net_open(struct promdata *);
void	net_close(struct promdata *);
int	net_mountroot(void);

/*
 * duplicates from pmap.c for mapping device on "oldmon" machines.
 */
#include <sparc/sparc/asm.h>

#define getcontext()		lduba(AC_CONTEXT, ASI_CONTROL)
#define setcontext(c)		stba(AC_CONTEXT, ASI_CONTROL, c)
#define getsegmap(va)		(cputyp == CPU_SUN4C \
					? lduba(va, ASI_SEGMAP) \
					: lduha(va, ASI_SEGMAP))
#define setsegmap(va, pmeg)	(cputyp == CPU_SUN4C \
					? stba(va, ASI_SEGMAP, pmeg) \
					: stha(va, ASI_SEGMAP, pmeg))
#define getregmap(va)		((unsigned)lduha((va) + 2, ASI_REGMAP) >> 8)
#define setregmap(va, smeg)	stha((va) + 2, ASI_REGMAP, (smeg << 8))

#define getpte(va)		lda(va, ASI_PTE)
#define setpte(va, pte)		sta(va, ASI_PTE, pte)
