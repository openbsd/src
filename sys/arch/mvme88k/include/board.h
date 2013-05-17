/*	$OpenBSD: board.h,v 1.22 2013/05/17 22:46:27 miod Exp $	*/
/*
 * Copyright (c) 2013, Miodrag Vallat
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

struct cmmu_p;
struct intrhand;
struct mvmeprom_brdid;
struct pmap_table;

struct board {
	void		(*bootstrap)(void);
	vaddr_t		(*memsize)(void);
	int		(*cpuspeed)(const struct mvmeprom_brdid *);
	void		(*reboot)(int);
	int		(*is_syscon)(void);

	void		(*intr)(struct trapframe *);
	int		(*nmi)(struct trapframe *);		/* 88110 */
	void		(*nmi_wrapup)(struct trapframe *);	/* 88110 */

	u_int		(*getipl)(void);
	u_int		(*setipl)(u_int);
	u_int		(*raiseipl)(u_int);

	int		(*intsrc_available)(u_int, int);
	void		(*intsrc_enable)(u_int, int);
	void		(*intsrc_disable)(u_int, int);
	int		(*intsrc_establish)(u_int, struct intrhand *,
			    const char *);
	void		(*intsrc_disestablish)(u_int, struct intrhand *);

	void		(*init_clocks)(void);
	void		(*delay)(int);

	u_int		(*init_vme)(const char *);

#ifdef	MULTIPROCESSOR
	void		(*send_ipi)(int, cpuid_t);
	void		(*smp_setup)(struct cpu_info *);
#endif

	const struct pmap_table *ptable;
	const struct cmmu_p *cmmu;
};

#define	BOARD_PROTOS(b) \
void	m##b##_bootstrap(void); \
vaddr_t	m##b##_memsize(void); \
int	m##b##_cpuspeed(const struct mvmeprom_brdid *); \
void	m##b##_reboot(int); \
int	m##b##_is_syscon(void); \
void	m##b##_intr(struct trapframe *); \
int	m##b##_nmi(struct trapframe *); \
void	m##b##_nmi_wrapup(struct trapframe *); \
u_int	m##b##_getipl(void); \
u_int	m##b##_setipl(u_int); \
u_int	m##b##_raiseipl(u_int); \
int	m##b##_intsrc_available(u_int, int); \
void	m##b##_intsrc_enable(u_int, int); \
void	m##b##_intsrc_disable(u_int, int); \
int	m##b##_intsrc_establish(u_int, struct intrhand *, const char *); \
void	m##b##_intsrc_disestablish(u_int, struct intrhand *); \
void	m##b##_init_clocks(void); \
u_int	m##b##_init_vme(const char *); \
void	m##b##_delay(int); \
void	m##b##_send_ipi(int, cpuid_t); \
void	m##b##_smp_setup(struct cpu_info *)

BOARD_PROTOS(181);
BOARD_PROTOS(187);
BOARD_PROTOS(188);
BOARD_PROTOS(197);
BOARD_PROTOS(1x7);

extern const struct board board_mvme181;
extern const struct board board_mvme187;
extern const struct board board_mvme188;
extern const struct board board_mvme197le;
extern const struct board board_mvme197spdp;

extern const struct board *platform;

#define	md_interrupt_func(f)	platform->intr(f)
#define	md_nmi_func(f)		platform->nmi(f)
#define	md_nmi_wrapup_func(f)	platform->nmi_wrapup(f)

#endif	/* _LOCORE */
#endif	/* _MACHINE_BOARD_H_ */
