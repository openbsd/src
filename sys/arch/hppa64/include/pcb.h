/*	$OpenBSD: pcb.h,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

#include <machine/reg.h>

struct pcb {
	u_int64_t pcb_fpregs[HPPA_NFPREGS+1];	/* not in the trapframe */
	u_int64_t pcb_onfault;		/* SW copy fault handler */
	vaddr_t pcb_uva;		/* KVA for U-area */
	u_int64_t pcb_ksp;		/* kernel sp for ctxsw */
	pa_space_t pcb_space;		/* copy pmap_space, for asm's sake */

#if 0	/* imaginary part that is after user but in the same page */
	u_int32_t pcb_pad[53+768];
	u_int64_t pcb_frame[64];	/* the very end */
#endif
};

struct md_coredump {
	struct reg md_reg;
	struct fpreg md_fpreg;
}; 


#endif /* _MACHINE_PCB_H_ */
