/* $NetBSD: pxa2x0var.h,v 1.2 2003/06/05 13:48:28 scw Exp $ */

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
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
 *	This product includes software developed for the NetBSD Project by
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _ARM_XSCALE_PXA2X0VAR_H_
#define _ARM_XSCALE_PXA2X0VAR_H_

#include <arm/sa11x0/sa11x0_var.h>

/* PXA2X0's integrated peripheral bus. */

typedef int (* pxa2x0_irq_handler_t)(void *);

struct pxaip_attach_args {
	struct sa11x0_attach_args  pxa_sa;
	bus_dma_tag_t pxa_dmat;
	int pxa_index;			/* to specify device by index number */

#define pxa_iot 	pxa_sa.sa_iot
#define pxa_addr	pxa_sa.sa_addr
#define pxa_size	pxa_sa.sa_size
#define pxa_intr	pxa_sa.sa_intr
};

#define	cf_addr		cf_loc[0]
#define	cf_size		cf_loc[1]
#define	cf_intr		cf_loc[2]
#define	cf_index	cf_loc[3]


extern struct bus_space pxa2x0_bs_tag;
extern struct arm32_bus_dma_tag pxa2x0_bus_dma_tag;
extern struct bus_space pxa2x0_a4x_bs_tag;

/* misc. */
extern void pxa2x0_fcs_init(void);
extern void pxa2x0_freq_change(int);
extern void pxa2x0_turbo_mode(int);
extern int pxa2x0_i2c_master_tx( int, uint8_t *, int );

/*
 * Probe the memory controller to deterimine which SDRAM are
 * populated, and what size of SDRAM is present in each bank.
 *
 * This routine should be called from a port's initarm()
 * function, with the first parameter set to the address
 * of the memory controller's registers.
 */
extern void pxa2x0_probe_sdram(vaddr_t, paddr_t *, psize_t *);

/*
 * Configure one or more clock enables in the Clock Manager's
 * CKEN register.
 */
extern void pxa2x0_clkman_config(u_int, int);

#endif /* _ARM_XSCALE_PXA2X0VAR_H_ */
