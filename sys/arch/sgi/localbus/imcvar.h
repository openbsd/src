/*	$OpenBSD: imcvar.h,v 1.1 2012/03/28 20:44:23 miod Exp $	*/
/*	$NetBSD: imcvar.h,v 1.1 2006/08/30 23:44:52 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

struct imc_attach_args {
	const char	*iaa_name;
	bus_space_tag_t	 iaa_st;
	bus_dma_tag_t	 iaa_dmat;
};

int	imc_gio64_arb_config(int, uint32_t);
void	imc_disable_sysad_parity(void);
void	imc_enable_sysad_parity(void);
int	imc_is_sysad_parity_enabled(void);

#define	imc_read(o) \
	*(volatile uint32_t *)PHYS_TO_XKPHYS(IMC_BASE + (o), CCA_NC)
#define	imc_write(o,v) \
	*(volatile uint32_t *)PHYS_TO_XKPHYS(IMC_BASE + (o), CCA_NC) = (v)
