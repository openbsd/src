/*	$OpenBSD: board.h,v 1.6 2010/04/21 19:33:47 miod Exp $	*/
/*
 * Copyright (c) 2006, 2007, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_BOARD_H_
#define	_MACHINE_BOARD_H_

#if !defined(_LOCORE)

#include <machine/pmap_table.h>

struct vme_range;

struct board {
	const char	*descr;
	void		(*bootstrap)(void);
	vaddr_t		(*memsize)(void);
	void		(*startup)(void);

	void		(*intr)(struct trapframe *);
	void		(*init_clocks)(void);
	u_int		(*getipl)(void);
	u_int		(*setipl)(u_int);
	u_int		(*raiseipl)(u_int);

	u_int64_t	(*intsrc)(int);

	pmap_table_t	ptable;

	const struct vme_range *vme_ranges;
};

#define	md_interrupt_func(f)	platform->intr(f)

#define	DECLARE_BOARD(b) \
extern const struct board board_av##b; \
void	av##b##_bootstrap(void); \
vaddr_t	av##b##_memsize(void); \
void	av##b##_startup(void); \
void	av##b##_intr(struct trapframe *); \
void	av##b##_init_clocks(void); \
u_int	av##b##_getipl(void); \
u_int	av##b##_setipl(u_int); \
u_int	av##b##_raiseipl(u_int); \
u_int64_t av##b##_intsrc(int);

DECLARE_BOARD(400);
DECLARE_BOARD(530);
DECLARE_BOARD(5000);
DECLARE_BOARD(6280);

/*
 * Logical values for interrupt sources.
 * When adding new sources, keep INTSRC_VME as the last item - syscon
 * depends on this.
 */

#define	INTSRC_ABORT		1	/* abort button */
#define	INTSRC_ACFAIL		2	/* AC failure */
#define	INTSRC_SYSFAIL		3	/* system failure */
#define	INTSRC_CLOCK		4	/* clock chip */
#define	INTSRC_DUART1		5	/* console MC68692 */
#define	INTSRC_DUART2		6	/* secondary MC68692 */
#define	INTSRC_ETHERNET1	7	/* first on-board Ethernet */
#define	INTSRC_ETHERNET2	8	/* second on-board Ethernet */
#define	INTSRC_SCSI1		9	/* first on-board SCSI controller */
#define	INTSRC_SCSI2		10	/* second on-board SCSI controller */
#define	NINTSRC_SYSCON		11	/* total number of non-VME sources */
#define	INTSRC_VME(lvl)	(NINTSRC_SYSCON + (lvl) - 1)	/* seven VME levels */

#define	IS_VME_INTSRC(intsrc)		((intsrc) >= NINTSRC_SYSCON)
#define	VME_INTSRC_LEVEL(intsrc)	((intsrc) - NINTSRC_SYSCON + 1)

void	intsrc_enable(u_int, int);
void	intsrc_disable(u_int);

extern int32_t cpuid, sysid;
extern const struct board *platform;/* just to have people confuse both names */

void cio_init_clocks(void);
void rtc_init_clocks(void);

#endif	/* _LOCORE */
#endif	/* _MACHINE_BOARD_H_ */
