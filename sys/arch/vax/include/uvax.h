/*	$NetBSD: uvax.h,v 1.1 1996/07/20 17:58:24 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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

/*
 * generic(?) MicroVAX and VAXstation support
 *
 * There are similarities to struct cpu_calls[] in autoconf.c
 */

/* 
 * Prototypes for autoconf.c
 */
struct	device;
void	uvax_conf __P((struct device*, struct device*, void*));
int	uvax_clock __P((void));
void	uvax_memerr __P((void));
int	uvax_mchk __P((caddr_t));
void	uvax_steal_pages __P((void));

int	uvax_setup __P((int mapen));

struct uvax_calls {
	u_long	(*uc_phys2virt) __P((u_long));	/* most often used! */
	u_long	(*uc_memsize) __P((void));
	void	(*uc_steal_pages) __P((void));
	void	(*uc_conf) __P((struct device*, struct device*, void*));
	void	(*uc_memerr) __P((void));
	int	(*uc_mchk) __P((caddr_t));
	int	(*uc_clkread) __P((time_t));
	void	(*uc_clkwrite) __P((void));

	char	*uc_name;
	u_char  *uc_intreq;
	u_char	*uc_intclr;
	u_char  *uc_intmsk;

	void	*le_iomem;		/* base addr of RAM -- CPU's view */
	u_long	*le_ioaddr;		/* base addr of RAM -- LANCE's view */
	int	*le_memsize;		/* size of RAM reserved for LANCE */

	void	*uc_physmap;
	int	uc_busTypes;
	int	uc_vups;		/* used by delay() */

	int	uv_flags;
	int	vs_flags;
};		

extern struct uvax_calls guc;		/* Generic uVAX Calls */
extern struct uvax_calls *ucp;

struct uc_map {
	u_long	um_base;
	u_long	um_end;
	u_long	um_size;
	u_long	um_virt;
};
extern struct uc_map *uc_physmap;

/*
 * Generic definitions common on all MicroVAXen clock chip.
 */
#define	uVAX_CLKVRT	0200
#define	uVAX_CLKUIP	0200
#define	uVAX_CLKRATE	040
#define	uVAX_CLKENABLE	06
#define	uVAX_CLKSET	0206

/* cpmbx bits  */
#define	uVAX_CLKHLTACT	03

/* halt action values */
#define	uVAX_CLKRESTRT	01
#define	uVAX_CLKREBOOT	02
#define	uVAX_CLKHALT	03

/* in progress flags */
#define	uVAX_CLKBOOT	04
#define	uVAX_CLKRSTRT	010
#define	uVAX_CLKLANG	0360

/* Prototypes */
int	uvax_clkread __P((time_t));
void	uvax_clkwrite __P((void));
