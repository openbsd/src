/*	$OpenBSD: board.h,v 1.2 2006/05/21 12:22:03 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat
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

struct board {
	const char *descr;
	void (*bootstrap)(void);
	vaddr_t (*memsize)(void);
	void (*startup)(void);

	void (*intr)(u_int, struct trapframe *);
	void (*init_clocks)(void);
	u_int (*getipl)(void);
	u_int (*setipl)(u_int);
	u_int (*raiseipl)(u_int);

	pmap_table_t ptable;
};

#define	md_interrupt_func(t, f)	platform->intr(t, f)

#define	DECLARE_BOARD(b) \
extern const struct board board_av##b; \
void	av##b##_bootstrap(void); \
vaddr_t	av##b##_memsize(void); \
void	av##b##_startup(void); \
void	av##b##_intr(u_int, struct trapframe *); \
void	av##b##_init_clocks(void); \
u_int	av##b##_getipl(void); \
u_int	av##b##_setipl(u_int); \
u_int	av##b##_raiseipl(u_int);

DECLARE_BOARD(400);
DECLARE_BOARD(530);
DECLARE_BOARD(5000);
DECLARE_BOARD(6280);

extern const struct board *platform;/* just to have people confuse both names */

#endif	/* _LOCORE */
#endif	/* _MACHINE_BOARD_H_ */
