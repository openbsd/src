/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#ifndef __EDMAREG_H__
#define __EDMAREG_H__

/*
 * TPCC Registers
 */

#define EDMA_NUM_DMA_CHANS	64
#define EDMA_NUM_QDMA_CHANS	8
#define EDMA_REG_X(x)		(0x1000 + (0x200 * x))

#define EDMA_TPCC_PID		0x0
#define EDMA_TPCC_CCCFG		0x4
#define EDMA_TPCC_SYSCFG	0x10
#define EDMA_TPCC_DHCM(x)	(0x100 + (x * 4))
#define EDMA_TPCC_QCHM(x)	(0x200 + (x * 4))
#define EDMA_TPCC_DMAQNUM(x)	(0x240 + (x * 4))
# define DMAQNUM_SET(c, q)	((0x7 & (q)) << (((c) % 8) * 4))
# define DMAQNUM_CLR(c)		(~(0x7 << (((c) % 8) * 4)))
#define EDMA_TPCC_QDMAQNUM	0x260
# define QDMAQNUM_SET(c,q)	((0x7 & (q)) << ((c) * 4))
# define QDMAQNUM_CLR(c)	(~(0x7 << ((c) * 4)))
#define EDMA_TPCC_QUEPRI	0x284
#define EDMA_TPCC_EMR		0x300
#define EDMA_TPCC_EMRH		0x304
#define EDMA_TPCC_EMCR		0x308
#define EDMA_TPCC_EMCRH		0x30c
#define EDMA_TPCC_QEMR		0x310
#define EDMA_TPCC_QEMCR		0x314
#define EDMA_TPCC_CCERR		0x318
#define EDMA_TPCC_CCERRCLR	0x31c
#define EDMA_TPCC_EEVAL		0x320
#define EDMA_TPCC_DRAE0		0x340
#define EDMA_TPCC_DRAEH0	0x344
#define EDMA_TPCC_DRAE1		0x348
#define EDMA_TPCC_DRAEH1	0x34c
#define EDMA_TPCC_DRAE2		0x350
#define EDMA_TPCC_DRAEH2	0x354
#define EDMA_TPCC_DRAE3		0x358
#define EDMA_TPCC_DRAEH3	0x35c
#define EDMA_TPCC_DRAE4		0x360
#define EDMA_TPCC_DRAEH4	0x364
#define EDMA_TPCC_DRAE5		0x368
#define EDMA_TPCC_DRAEH5	0x36c
#define EDMA_TPCC_DRAE6		0x370
#define EDMA_TPCC_DRAEH6	0x374
#define EDMA_TPCC_DRAE7		0x378
#define EDMA_TPCC_DRAEH7	0x37c
#define EDMA_TPCC_QRAE(x)	(0x380 + (x * 4))
#define EDMA_TPCC_QXEY(x,y)	(0x400 + (x * 0x40) + (y * 4))
#define EDMA_TPCC_QSTAT(x)	(0x600 + (x * 4))
#define EDMA_TPCC_QWAMTHRA	0x620
#define EDMA_TPCC_CCSTAT	0x640
#define EDMA_TPCC_MPFAR		0x800
#define EDMA_TPCC_MPFSR		0x804
#define EDMA_TPCC_MPFCR		0x808
#define EDMA_TPCC_MPPAG		0x80c
#define EDMA_TPCC_MPPA(x)	(0x810 + (x * 4))
#define EDMA_TPCC_ER		0x1000
#define EDMA_TPCC_ERH		0x1004
#define EDMA_TPCC_ECR		0x1008
#define EDMA_TPCC_ECRH		0x100c
#define EDMA_TPCC_ESR		0x1010
#define EDMA_TPCC_ESRH		0x1014
#define EDMA_TPCC_CER		0x1018
#define EDMA_TPCC_CERH		0x101c
#define EDMA_TPCC_EER		0x1020
#define EDMA_TPCC_EERH		0x1024
#define EDMA_TPCC_EECR		0x1028
#define EDMA_TPCC_EECRH		0x102c
#define EDMA_TPCC_EESR		0x1030
#define EDMA_TPCC_EESRH		0x1034
#define EDMA_TPCC_SER		0x1038
#define EDMA_TPCC_SERH		0x103c
#define EDMA_TPCC_SECR		0x1040
#define EDMA_TPCC_SECRH		0x1044
#define EDMA_TPCC_IER		0x1050
#define EDMA_TPCC_IERH		0x1054
#define EDMA_TPCC_IECR		0x1058
#define EDMA_TPCC_IECRH		0x105c
#define EDMA_TPCC_IESR		0x1060
#define EDMA_TPCC_IESRH		0x1064
#define EDMA_TPCC_IPR		0x1068
#define EDMA_TPCC_IPRH		0x106c
#define EDMA_TPCC_ICR		0x1070
#define EDMA_TPCC_ICRH		0x1074
#define EDMA_TPCC_IEVAL		0x1078
#define EDMA_TPCC_QER		0x1080
#define EDMA_TPCC_QEER		0x1084
#define EDMA_TPCC_QEECR		0x1088
#define EDMA_TPCC_QEESR		0x108c
#define EDMA_TPCC_QSER		0x1090
#define EDMA_TPCC_QSECR		0x1094
#define EDMA_TPCC_OPT(x)	(0x4000 + (x * 0x20))

#define TPCC_READ_4(sc, reg)						\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_tpcc, (reg)))
#define TPCC_WRITE_4(sc, reg, val)					\
	(bus_space_write_4((sc)->sc_iot, (sc)->sc_tpcc, (reg), (val)))
#define TPCC_SET(sc, reg, val)						\
	(TPCC_WRITE_4((sc), (reg), (TPCC_READ_4(sc, reg) | (val))))
#define TPCC_FILTSET(sc, reg, val, filt)				\
	(TPCC_WRITE_4((sc), (reg), (TPCC_READ_4(sc, reg) & (filt)) | (val)))

/*
 * TPTC Registers
 */

#define EDMA_TPTC_PID		0x0
#define EDMA_TPTC_TCCFG		0x4
#define EDMA_TPTC_SYSCFG	0x10
#define EDMA_TPTC_TCSTAT	0x100
#define EDMA_TPTC_ERRSTAT	0x120
#define EDMA_TPTC_ERREN		0x124
#define EDMA_TPTC_ERRCLR	0x128
#define EDMA_TPTC_ERRDET	0x12c
#define EDMA_TPTC_ERRCMD	0x130
#define EDMA_TPTC_RDRATE	0x140
#define EDMA_TPTC_SAOPT		0x240
#define EDMA_TPTC_SASRC		0x244
#define EDMA_TPTC_SACNT		0x248
#define EDMA_TPTC_SADST		0x24c
#define EDMA_TPTC_SABIDX	0x250
#define EDMA_TPTC_SAMPPRXY	0x254
#define EDMA_TPTC_SACNTRLD	0x258
#define EDMA_TPTC_SASRCBREF	0x25c
#define EDMA_TPTC_SADSTBREF	0x260
#define EDMA_TPTC_DFCNTRLD	0x280
#define EDMA_TPTC_DFSRCBREF	0x284
#define EDMA_TPTC_DFDSTBREF	0x288
#define EDMA_TPTC_DFOPT0	0x300
#define EDMA_TPTC_DFSRC0	0x304
#define EDMA_TPTC_DFCNT0	0x308
#define EDMA_TPTC_DFDST0	0x30c
#define EDMA_TPTC_DFBIDX0	0x310
#define EDMA_TPTC_DFMPPRXY0	0x314
#define EDMA_TPTC_DFOPT1	0x340
#define EDMA_TPTC_DFSRC1	0x344
#define EDMA_TPTC_DFCNT1	0x348
#define EDMA_TPTC_DFDST1	0x34c
#define EDMA_TPTC_DFBIDX1	0x350
#define EDMA_TPTC_DFMPPRXY1	0x354
#define EDMA_TPTC_DFOPT2	0x380
#define EDMA_TPTC_DFSRC2	0x384
#define EDMA_TPTC_DFCNT2	0x388
#define EDMA_TPTC_DFDST2	0x38c
#define EDMA_TPTC_DFBIDX2	0x390
#define EDMA_TPTC_DFMPPRXY2	0x394
#define EDMA_TPTC_DFOPT3	0x3c0
#define EDMA_TPTC_DFSRC3	0x3c4
#define EDMA_TPTC_DFCNT3	0x3c8
#define EDMA_TPTC_DFDST3	0x3cc
#define EDMA_TPTC_DFBIDX3	0x3d0
#define EDMA_TPTC_DFMPPRXY3	0x3d4

#define TPTC_READ_4(sc, i, reg)						\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_tptc ## i, (reg)))
#define TPTC_WRITE_4(sc, i, reg, val)					\
	(bus_space_write_4((sc)->sc_iot, (sc)->sc_tptc ## i, (reg), (val)))

#endif /* __EDMAREG_H__ */
