/*	$OpenBSD: board.h,v 1.7 2010/04/24 18:44:27 miod Exp $	*/
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

/*
 * cpuid values
 */

#define	AVIION_300_310		0x7904	/* mono Maverick */
#define	AVIION_5100_6100	0x7906	/* 20MHz Topgun */
#define	AVIION_400_4000		0x7908	/* 16MHz Mav+ */
#define	AVIION_410_4100		0x790c	/* 20MHz Mav+ */
#define	AVIION_300C_310C	0x7910	/* color Maverick */
#define	AVIION_5200_6200	0x7912	/* 25MHz Topgun */
#define	AVIION_5240_6240	0x7918	/* 25MHz Shotgun */
#define	AVIION_300CD_310CD	0x7920	/* dual duart color Maverick */
#define	AVIION_300D_310D	0x7924	/* dual duart mono Maverick */
#define	AVIION_4600_530		0x7930	/* Rolling Rock */
#define	AVIION_4300_25		0x7932	/* 25MHz Terra */
#define	AVIION_4300_20		0x7934	/* 20MHz Terra */
#define	AVIION_4300_16		0x7936	/* 16MHz Terra */
#define	AVIION_5255_6255	0x7942	/* 25MHz Tophat */
#define	AVIION_350		0x7944	/* KME */
#define	AVIION_6280		0x7946	/* High Noon */
#define	AVIION_8500_9500	0x794a	/* Odyssey */
#define	AVIION_9500_HA		0x794c	/* Oz */
#define	AVIION_500		0x794e	/* Robin Hood */
#define	AVIION_5500		0x7950	/* Schooner */
#define	AVIION_450		0x7958	/* Inner Tube */
#define	AVIION_8500_9500_45_1MB	0x795a	/* 45MHz Iliad (1MB L2) */
#define	AVIION_10000		0x7960	/* Sierra */
#define	AVIION_10000_QT		0x7962	/* Sierra QT */
#define	AVIION_5500PLUS		0x7964	/* Schooner+ */
#define	AVIION_450PLUS		0x7966	/* Inner Tube+ */
#define	AVIION_8500_9500_50_1MB	0x7968	/* 50MHz Iliad (1MB L2) */
#define	AVIION_8500_9500_50_2MB	0x796a	/* 50MHz Iliad (2MB L2) */

/* did the following ever hit the market? */
#define	AVIION_UNKNOWN1		0x7926	/* mono Montezuma */
#define	AVIION_UNKNOWN2		0x7928	/* color Montezuma */
#define	AVIION_UNKNOWN3		0x7956	/* Flintstone */
#define	AVIION_UNKNOWN1_DIS	0xfff0	/* mono disabled Montezuma */
#define	AVIION_UNKNOWN2_DIS	0xfff1	/* color disabled Montezuma */

extern int32_t cpuid;

#include <machine/pmap_table.h>
struct vme_range;

struct board {
	void		(*bootstrap)(void);
	vaddr_t		(*memsize)(void);
	void		(*startup)(void);

	void		(*intr)(struct trapframe *);
	void		(*init_clocks)(void);
	u_int		(*getipl)(void);
	u_int		(*setipl)(u_int);
	u_int		(*raiseipl)(u_int);

	u_int64_t	(*intsrc)(int);
	const struct vme_range *(*get_vme_ranges)(void);

	pmap_table_t	ptable;
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
u_int64_t av##b##_intsrc(int); \
const struct vme_range *av##b##_get_vme_ranges(void);

DECLARE_BOARD(400);
DECLARE_BOARD(530);
DECLARE_BOARD(5000);
DECLARE_BOARD(6280);

extern const struct board *platform;/* just to have people confuse both names */

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

void cio_init_clocks(void);
void rtc_init_clocks(void);

#endif	/* _LOCORE */
#endif	/* _MACHINE_BOARD_H_ */
