/*	$OpenBSD: samachdep.h,v 1.6 2008/02/13 21:20:32 miod Exp $	*/
/*	$NetBSD: samachdep.h,v 1.3 1997/05/12 07:54:45 thorpej Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
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
 *	@(#)samachdep.h	8.1 (Berkeley) 6/10/93
 */

#include <sys/types.h>
#include <machine/hp300spu.h>

#define	NHPIB		4
#define	NSCSI		2
#define NHD		8
#define NCT		8
#define NSD		8

#define NITE		4

/* from cpu.h */
#define	INTIOBASE	(0x00400000)
#undef	IIOV
#define IIOV(x)		(x)
#define DIOBASE		(0x600000)
#define	DIOCSIZE	(0x10000)
#define DIOIIBASE	(0x01000000)
#define DIOIICSIZE	(0x00400000)

#define MHZ_8		1
#define MHZ_16		2
#define MHZ_25		3
#define MHZ_33		4
#define MHZ_50		6

extern	int cpuspeed, machineid, mmuid;
extern	int howto;
extern	u_int opendev;
extern	u_int bootdev;
extern	int userom;

int	badaddr(char *);
void	call_req_reboot(void);
char	*getmachineid(void);
void	hpibinit(void);
void	romout(int, char *);
void	romputchar(int);
void	scsiinit(void);
u_long	sctoaddr(int);
int	tgets(char *);

#define DELAY(n)	{ int N = cpuspeed * (n); while (--N > 0); }

/*
 * Switch we use to set punit in devopen.
 */
struct punitsw {
	int	(*p_punit)(int, int, int *);
};
extern	struct punitsw punitsw[];
extern	int npunit;

extern	struct devsw devsw_net[];
extern	int ndevs_net;

extern	struct devsw devsw_general[];
extern	int ndevs_general;

extern	struct fs_ops file_system_rawfs[];
extern	struct fs_ops file_system_ufs[];
extern	struct fs_ops file_system_nfs[];
extern	struct fs_ops file_system_cd9660[];
