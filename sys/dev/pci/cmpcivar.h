/*	$OpenBSD: cmpcivar.h,v 1.3 2002/03/14 01:26:58 millert Exp $	*/

/*
 * Copyright (c) 2000 Takuya SHIOZAKI
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* C-Media CMI8x38 Audio Chip Support */

/*
 * DMA pool
 */
struct cmpci_dmanode {
	bus_dma_tag_t         cd_tag;
	int                   cd_nsegs;
	bus_dma_segment_t     cd_segs[1];
	bus_dmamap_t          cd_map;
	caddr_t               cd_addr;
	size_t                cd_size;
	struct cmpci_dmanode *cd_next;
};

typedef struct cmpci_dmanode *cmpci_dmapool_t;
#define KVADDR(dma)  ((void *)(dma)->cd_addr)
#define DMAADDR(dma) ((dma)->cd_map->dm_segs[0].ds_addr)


/*
 * Mixer - SoundBlaster16 Compatible
 */
#define CMPCI_MASTER_VOL		0
#define CMPCI_FM_VOL			1
#define CMPCI_CD_VOL			2
#define CMPCI_VOICE_VOL			3
#define CMPCI_OUTPUT_CLASS		4

#define CMPCI_MIC_VOL			5
#define CMPCI_LINE_IN_VOL		6
#define CMPCI_RECORD_SOURCE		7
#define CMPCI_TREBLE			8
#define CMPCI_BASS			9
#define CMPCI_RECORD_CLASS		10
#define CMPCI_INPUT_CLASS		11

#define CMPCI_PCSPEAKER			12
#define CMPCI_INPUT_GAIN		13
#define CMPCI_OUTPUT_GAIN		14
#define CMPCI_AGC			15
#define CMPCI_EQUALIZATION_CLASS	16

#define CMPCI_CD_IN_MUTE		17
#define CMPCI_MIC_IN_MUTE		18
#define CMPCI_LINE_IN_MUTE		19
#define CMPCI_FM_IN_MUTE		20

#define CMPCI_CD_SWAP			21
#define CMPCI_MIC_SWAP			22
#define CMPCI_LINE_SWAP			23
#define CMPCI_FM_SWAP			24

#define CMPCI_CD_OUT_MUTE		25
#define CMPCI_MIC_OUT_MUTE		26
#define CMPCI_LINE_OUT_MUTE		27

#ifdef CMPCI_SPDIF_SUPPORT
#define CMPCI_SPDIF_IN			28
#define CMPCI_SPDIF_IN_MUTE		29
#define CmpciNspdif			"spdif"
#define CMPCI_NDEVS			30
#else
#define CMPCI_NDEVS			28
#endif

#define CMPCI_IS_IN_MUTE(x) ((x) < CMPCI_CD_SWAP)


/*
 * softc
 */
struct cmpci_softc {
	struct device sc_dev;

	/* I/O Base device */
	bus_space_tag_t     sc_iot;
	bus_space_handle_t  sc_ioh;

	/* intr handle */
	pci_intr_handle_t * sc_ih;

	/* DMA */
	bus_dma_tag_t       sc_dmat;
	cmpci_dmapool_t     sc_dmap;

	/* each channel */
	struct {
		void (*intr)(void *);
		void *intr_arg;
	} sc_play, sc_rec;

	/* mixer */
	uint8_t gain[CMPCI_NDEVS][2];
#define CMPCI_LEFT 0
#define CMPCI_RIGHT 1
#define CMPCI_LR 0
	uint16_t in_mask;
};
