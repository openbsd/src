/*	$OpenBSD: dma.h,v 1.4 1997/07/21 11:26:10 pefo Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

extern vm_map_t phys_map;

/*
 * Little endian mips uses bounce buffer so flush
 * for dma is not requiered.
 */

#ifdef MIPSEL
#define ASC_NOFLUSH
#endif

/*
 *  Structure used to control dma.
 */

typedef struct dma_softc {
	struct device	sc_dev;		/* use as a device */
	struct esp_softc *sc_esp;
	int		dma_ch;
	vm_offset_t	dma_va;		/* Viritual address for transfer */
	vm_offset_t	req_va;		/* Original request va */
	int		req_size;	/* Request size */
	int		mode;		/* Mode register value and direction */
	int		sc_active;	/* Active flag */
	char		**sc_dmaaddr;	/* Pointer to dma address in dev */
	int		*sc_dmalen;	/* Pointer to len counter in dev */
} dma_softc_t;

#define	DMA_TO_DEV	0
#define	DMA_FROM_DEV	1

#define	DMA_CH0		0
#define	DMA_CH1		1

#define	DMA_RESET(r)	
#if 0
#define	DMA_START(a, b, c, d)						\
	{								\
	    int dcmd;							\
	    int xcmd;							\
	    int pa;							\
	    int sz;							\
	    if((vm_offset_t)(b) < VM_MIN_KERNEL_ADDRESS) {		\
		pa = CACHED_TO_PHYS(b);					\
	    }								\
	    else {							\
		pa = pmap_extract(vm_map_pmap(phys_map), (vm_offset_t)(b));\
	    }								\
	    sz = c;							\
	    if(sz + (pa & (NBPG - 1)) > NBPG) {				\
		sz = NBPG - (pa & (NBPG - 1));				\
	    }								\
	    dcmd = ((d) == DMA_FROM_DEV) ? 0x30 : 0x10;			\
	    if((a)->dma_ch == DMA_CH0) {				\
		out32(R3715_DMA_ADR0, pa);				\
		out32(R3715_DMA_CNT0, sz - 1);				\
	        xcmd = ~0x30;						\
	    }								\
	    else {							\
		out32(R3715_DMA_ADR1, pa);				\
		out32(R3715_DMA_CNT1, sz - 1);				\
	        dcmd = dcmd << 6;					\
	        xcmd = ~(0x30 << 6);					\
	    }								\
	    dcmd |= (1 << 26);						\
	    out32(R3715_IO_TIMING, (in32(R3715_IO_TIMING) & xcmd) | dcmd);\
	}
#else
#define	DMA_START(a, b, c, d)						\
	{								\
	    int dcmd;							\
	    int xcmd;							\
	    int pa;							\
	    int sz;							\
	    pa = CACHED_TO_PHYS(dma_buffer);				\
	    (a)->req_va = (vm_offset_t)(b);				\
	    (a)->req_size = c;						\
	    (a)->mode = d;						\
	    sz = c;							\
	    if((d) == DMA_TO_DEV) {					\
		int *_p = (int *)PHYS_TO_UNCACHED(pa);			\
		int *_v = (int *)b;					\
		int _n = sz;						\
		if(_n) {						\
			copynswap(_v, _p, _n);				\
	    	}							\
	    }								\
	    dcmd = ((d) == DMA_FROM_DEV) ? 0x30 : 0x10;			\
	    if((a)->dma_ch == DMA_CH0) {				\
		out32(R3715_DMA_ADR0, pa);				\
		out32(R3715_DMA_CNT0, sz - 1);				\
	        xcmd = ~0x30;						\
	    }								\
	    else {							\
		out32(R3715_DMA_ADR1, pa);				\
		out32(R3715_DMA_CNT1, sz - 1);				\
	        dcmd = dcmd << 6;					\
	        xcmd = ~(0x30 << 6);					\
	    }								\
	    dcmd |= (1 << 26);						\
	    /* Switch direction before enable */			\
	    out32(R3715_IO_TIMING, (in32(R3715_IO_TIMING) & xcmd) |	\
			(dcmd & ~0x410));				\
	    (void)in16(RISC_STATUS);					\
	    out32(R3715_IO_TIMING, (in32(R3715_IO_TIMING) & xcmd) | dcmd);\
	}
#endif
#define	DMA_MAP(a, b, c, d)
#define	DMA_INTR(r)
#define	DMA_DRAIN(r)
#define	DMA_END(c)							\
	{								\
	    int resudial;						\
	    if((c)->dma_ch == DMA_CH0) {				\
	    	out32(R3715_IO_TIMING, in32(R3715_IO_TIMING) & ~0x10);	\
		resudial = in32(R3715_DMA_CNT0);			\
	    }								\
	    else {							\
	    	out32(R3715_IO_TIMING, in32(R3715_IO_TIMING) & ~0x400);	\
		resudial = in32(R3715_DMA_CNT1);			\
	    }								\
	    if(resudial)						\
		resudial++;						\
	    if((c)->mode == DMA_FROM_DEV) {				\
		int *_v = (int *)(c)->req_va;				\
		int *_p = (int *)PHYS_TO_UNCACHED(CACHED_TO_PHYS(dma_buffer)); \
		int _n = (c)->req_size - resudial;			\
		if(_n) {						\
			copynswap(_p, _v, _n);				\
	    	}							\
	    }								\
	}
