/* $OpenBSD: pcb.h,v 1.3 2017/03/24 19:48:01 kettenis Exp $ */
/*
 * Copyright (c) 2016 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef	_MACHINE_PCB_H_
#define	_MACHINE_PCB_H_

#include <machine/frame.h>

#include <machine/pte.h>
#include <machine/reg.h>

struct trapframe;

/*
 * Warning certain fields must be within 256 bytes of the beginning
 * of this structure.
 */
struct pcb {
	u_int		pcb_flags;
#define	PCB_FPU		0x00000001	/* Process had FPU initialized */
	struct		trapframe *pcb_tf;

	register_t	pcb_sp;		// stack pointer of switchframe

	caddr_t		pcb_onfault;	// On fault handler
	struct fpreg	pcb_fpstate;	// Floating Point state */
	struct cpu_info	*pcb_fpcpu;

	void		*pcb_tcb;
};
#endif	/* _MACHINE_PCB_H_ */
