/*	$OpenBSD: autoconf.h,v 1.17 2002/12/18 23:52:45 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
 * All rights reserved.
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <machine/bus.h>
#include <machine/pdc.h>

struct confargs {
	const char	*ca_name;	/* device name/description */
	bus_space_tag_t	ca_iot;		/* io tag */
	struct device_path ca_dp;	/* device_path as found by pdc_scan */
	struct iodc_data ca_type;	/* iodc-specific type descrition */
	hppa_hpa_t	ca_hpa;		/* module HPA */
	hppa_hpa_t	ca_hpamask;	/* mask for modules on the bus */
	bus_dma_tag_t	ca_dmatag;	/* DMA tag */
	int		ca_irq;		/* module IRQ */
	struct pdc_iodc_read *ca_pdc_iodc_read;
}; 

#define	hppacf_off	cf_loc[0]
#define	hppacf_irq	cf_loc[1]

/* this is used for hppa_knownmodules table
 * describing known to this port modules,
 * system boards, cpus, fpus and busses
 */
struct hppa_mod_info {
	int	mi_type;
	int	mi_sv;
	const char *mi_name;
};

extern void (*cold_hook)(int);
#define	HPPA_COLD_COLD	0
#define	HPPA_COLD_HOT	1   
#define	HPPA_COLD_OFF	2

struct device;

const char *hppa_mod_info(int, int);
void	pdc_scanbus(struct device *, struct confargs *, int);
int	mbprint(void *, const char *);
int	mbsubmatch(struct device *, void *, void *);
void	*cpu_intr_map(void *v, int pri, int irq, int (*handler)(void *),
	    void *arg, struct device *dv);
void	*cpu_intr_establish(int pri, int irq, int (*handler)(void *),
	    void *arg, struct device *dv);
int	clock_intr(void *);

void	dumpconf(void);
