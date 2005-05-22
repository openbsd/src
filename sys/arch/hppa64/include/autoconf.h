/*	$OpenBSD: autoconf.h,v 1.2 2005/05/22 01:38:09 mickey Exp $	*/

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

#include <machine/bus.h>
#include <machine/pdc.h>

struct confargs {
	const char	*ca_name;	/* device name/description */
	struct iodc_data ca_type;	/* iodc-specific type descrition */
	bus_space_tag_t	ca_iot;		/* io tag */
	bus_dma_tag_t	ca_dmatag;	/* DMA tag */
	hppa_hpa_t	ca_hpa;		/* module HPA */
	u_int		ca_hpasz;	/* module HPA size (if avail) */
	int		ca_mod;		/* this module */
}; 

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
void	pdc_scan(struct device *, struct confargs *);
int	mbprint(void *, const char *);

void	dumpconf(void);
