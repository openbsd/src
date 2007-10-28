/*	$OpenBSD: envyvar.h,v 1.2 2007/10/28 18:25:21 fgsch Exp $	*/
/*
 * Copyright (c) 2007 Alexandre Ratchov <alex@caoua.org>
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

#ifndef SYS_DEV_PCI_ENVYVAR_H
#define SYS_DEV_PCI_ENVYVAR_H

#include <sys/types.h>
#include <sys/device.h>
#include <dev/audio_if.h>

struct envy_buf {
	bus_dma_segment_t	seg;
	bus_dmamap_t		map;
	caddr_t			addr;
	size_t			size;
};

/*
 * ak4524 codecs
 */
struct envy_ak {
	unsigned char reg[8];	/* shadow for ak4524 registers */
};

/*
 * the envy24 have multiple components with their respective mixers
 * depending on the configuration of the sound card, so we use
 * "submixers" for them
 */
struct envy_submix {
	int ndevs;
	void (*devinfo)(struct mixer_devinfo *, int);
	int  (*set)(struct mixer_devinfo *, int);
	int  (*get)(struct mixer_devinfo *, int);
};

#define ENVY_SUBMIX_NMAX	0x10

struct envy_softc {
	struct device		dev;
	struct device	       *audio;
	struct envy_buf		ibuf, obuf;
	pcitag_t		pci_tag;
	pci_chipset_tag_t	pci_pc;
	pci_intr_handle_t      *pci_ih;
	bus_dma_tag_t		pci_dmat;
	bus_space_tag_t		ccs_iot;
	bus_space_handle_t      ccs_ioh;
	bus_size_t		ccs_iosz;
	bus_space_tag_t		mt_iot;
	bus_space_handle_t      mt_ioh;
	bus_size_t		mt_iosz;
	struct envy_ak		ak[4];
	struct envy_submix	submix[ENVY_SUBMIX_NMAX];
	int			nsubmix;
	void (*iintr)(void *);
	void *iarg;
	void (*ointr)(void *);
	void *oarg;
};

#define ENVY_MIX_CLASSIN	0
#define ENVY_MIX_CLASSOUT	1
#define ENVY_MIX_CLASSMIX	2
#define ENVY_MIX_CLASSMON	3
#define ENVY_MIX_OUTSRC		4
#define ENVY_MIX_MONITOR	14
#define ENVY_MIX_INVAL		34

#define ENVY_MIX_OUTSRC_LINEIN	0
#define ENVY_MIX_OUTSRC_SPDIN	8
#define ENVY_MIX_OUTSRC_DMA	10
#define ENVY_MIX_OUTSRC_MON	11

#endif /* !defined(SYS_DEV_PCI_ENVYVAR_H) */
