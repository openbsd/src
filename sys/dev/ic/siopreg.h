/*	$OpenBSD: siopreg.h,v 1.4 2001/06/25 23:14:40 krw Exp $ */
/*	$NetBSD: siopreg.h,v 1.7 2000/10/06 16:35:13 bouyer Exp $	*/

/*
 * Copyright (c) 2000 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer
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
 *
 */

/*
 * Devices definitions for Symbios/NCR M53c8xx PCI-SCSI I/O Processors
 * Docs available from http://www.symbios.com/
 */

#define SIOP_SCNTL0 	0x00 /* SCSI control 0, R/W */
#define SCNTL0_ARB_MASK	0xc0
#define SCNTL0_SARB	0x00
#define SCNTL0_FARB	0xc0
#define SCNTL0_START	0x20
#define SCNTL0_WATM	0x10
#define SCNTL0_EPC	0x08
#define SCNTL0_AAP	0x02
#define SCNTL0_TRG	0x01

#define SIOP_SCNTL1 	0x01 /* SCSI control 1, R/W */
#define SCNTL1_EXC	0x80
#define SCNTL1_ADB	0x40
#define SCNTL1_DHP	0x20
#define SCNTL1_CON	0x10
#define SCNTL1_RST	0x08
#define SCNTL1_AESP	0x04
#define SCNTL1_IARB	0x02
#define SCNTL1_SST	0x01

#define SIOP_SCNTL2 	0x02 /* SCSI control 2, R/W */
#define SCNTL2_SDU	0x80
#define SCNTL2_CHM	0x40	/* 875 only */
#define SCNTL2_SLPMD	0x20	/* 875 only */
#define SCNTL2_SLPHBEN	0x10	/* 875 only */
#define SCNTL2_WSS	0x08	/* 875 only */
#define SCNTL2_VUE0	0x04	/* 875 only */
#define SCNTL2_VUE1	0x02	/* 875 only */
#define SCNTL2_WSR	0x01	/* 875 only */

#define SIOP_SCNTL3 	0x03 /* SCSI control 3, R/W */
#define SCNTL3_ULTRA	0x80	/* 875 only */
#define SCNTL3_SCF_SHIFT 4
#define SCNTL3_SCF_MASK	0x70
#define SCNTL3_EWS	0x08	/* 875 only */
#define SCNTL3_CCF_SHIFT 0
#define SCNTL3_CCF_MASK	0x07

#define SIOP_CLOCKS_SUPPORTED 3 /* 3 supported clocks: 25, 12.5, 6.25 ns */

struct period_factor {
	int factor; /* transfer period factor from sdtr/ppr */
	char *rate; /* string describing transfer rate      */
	struct {
		int st_scf;
		int dt_scf;
	} scf[SIOP_CLOCKS_SUPPORTED];	
		/* scf value to use in SCNTL3[6:4]
		 *   0 == SCLK/3
		 *   1 == SCLK/1
		 *   2 == SCLK/1.5
		 *   3 == SCLK/2
		 *   4 == SCLK/3
		 *   5 == SCLK/4
		 *   6 == SCLK/6
		 *   7 == SCLK/8
		 *
		 * One entry for each different clock
		 * supported, showing appropriate scf
		 * for the period_factor requested. A
		 * value of 0 indicates the rate is
		 * not supported.
		 *
		 * scf[0] == scf for a 25ns cycle
		 * scf[1] == scf for a 12.5 ns cycle
		 * scf[2] == scf for a 6.25 ns cycle
		 *
		 * min sync = first non zero in column
		 * max sync = last  non zero in column	
		 */
};

static const struct period_factor period_factor[] __attribute__((__unused__)) = {
	{0x09, "80",   {{0,0},{0,0},{0,1}}},
	{0x0a, "40",   {{0,0},{0,0},{1,3}}},
	{0x0c, "20",   {{0,0},{1,0},{3,5}}},
	{0x12, "13.3", {{0,0},{2,0},{4,6}}},
	{0x19, "10",   {{1,0},{3,0},{5,7}}},
	{0x25, "6.67", {{2,0},{4,0},{6,0}}},
	{0x32, "5",    {{3,0},{5,0},{7,0}}},
	{0x4b, "3.33", {{4,0},{6,0},{0,0}}},
};

#define SIOP_SCID	0x04 /* SCSI chip ID R/W */
#define SCID_RRE	0x40
#define SCID_SRE	0x20
#define SCID_ENCID_SHIFT 0
#define SCID_ENCID_MASK	0x07

#define SIOP_SXFER	0x05 /* SCSI transfer, R/W */
#define SXFER_TP_SHIFT	 5
#define SXFER_TP_MASK	0xe0
#define SXFER_MO_SHIFT  0
#define SXFER_MO_MASK	0x1f

#define SIOP_SDID	0x06 /* SCSI destination ID, R/W */
#define SDID_ENCID_SHIFT 0
#define SDID_ENCID_MASK	0x07

#define SIOP_GPREG	0x07 /* General purpose, R/W */
#define GPREG_GPIO4	0x10	/* 875 only */
#define GPREG_GPIO3	0x08	/* 875 only */
#define GPREG_GPIO2	0x04	/* 875 only */
#define GPREG_GPIO1	0x02
#define GPREG_GPIO0	0x01

#define SIOP_SFBR	0x08 /* SCSI first byte received, R/W */

#define SIOP_SOCL	0x09 /* SCSI output control latch, RW */

#define SIOP_SSID	0x0A /* SCSI selector ID, RO */
#define SSID_VAL	0x80
#define SSID_ENCID_SHIFT 0
#define SSID_ENCID_MASK 0x0f

#define SIOP_SBCL	0x0B /* SCSI control line, RO */

#define SIOP_DSTAT	0x0C /* DMA status, RO */
#define DSTAT_DFE	0x80
#define DSTAT_MDPE	0x40
#define DSTAT_BF	0x20
#define DSTAT_ABRT	0x10
#define DSTAT_SSI	0x08
#define DSTAT_SIR	0x04
#define DSTAT_IID	0x01

#define SIOP_SSTAT0	0x0D /* STSI status 0, RO */
#define SSTAT0_ILF	0x80
#define SSTAT0_ORF	0x40
#define SSTAT0_OLF	0x20
#define SSTAT0_AIP	0x10
#define SSTAT0_LOA	0x08
#define SSTAT0_WOA	0x04
#define SSTAT0_RST	0x02
#define SSTAT0_SDP	0x01

#define SIOP_SSTAT1	0x0E /* STSI status 1, RO */
#define SSTAT1_FFO_SHIFT 4
#define SSTAT1_FFO_MASK 0x80
#define SSTAT1_SDPL	0x08
#define SSTAT1_MSG	0x04
#define SSTAT1_CD	0x02
#define SSTAT1_IO	0x01
#define SSTAT1_PHASE_MASK (SSTAT1_IO | SSTAT1_CD | SSTAT1_MSG)
#define SSTAT1_PHASE_DATAOUT	0
#define SSTAT1_PHASE_DATAIN	SSTAT1_IO
#define SSTAT1_PHASE_CMD	SSTAT1_CD
#define SSTAT1_PHASE_STATUS	(SSTAT1_CD | SSTAT1_IO)
#define SSTAT1_PHASE_MSGOUT	(SSTAT1_MSG | SSTAT1_CD)
#define SSTAT1_PHASE_MSGIN	(SSTAT1_MSG | SSTAT1_CD | SSTAT1_IO)

#define SIOP_SSTAT2	0x0F /* STSI status 2, RO */
#define SSTAT2_ILF1	0x80	/* 875 only */
#define SSTAT2_ORF1	0x40	/* 875 only */
#define SSTAT2_OLF1	0x20	/* 875 only */
#define SSTAT2_FF4	0x10	/* 875 only */
#define SSTAT2_SPL1	0x08	/* 875 only */
#define SSTAT2_DF	0x04	/* 875 only */
#define SSTAT2_LDSC	0x02
#define SSTAT2_SDP1	0x01	/* 875 only */

#define SIOP_DSA	0x10 /* data struct addr, R/W */

#define SIOP_ISTAT	0x14 /* IRQ status, R/W */
#define ISTAT_ABRT	0x80
#define ISTAT_SRST	0x40
#define ISTAT_SIGP	0x20
#define ISTAT_SEM	0x10
#define ISTAT_CON	0x08
#define ISTAT_INTF	0x04
#define ISTAT_SIP	0x02
#define ISTAT_DIP	0x01

#define SIOP_CTEST0	0x18 /* Chip test 0, R/W */

#define SIOP_CTEST1	0x19 /* Chip test 1, R/W */

#define SIOP_CTEST2	0x1A /* Chip test 2, R/W */
#define CTEST2_SRTCH	0x04	/* 875 only */

#define SIOP_CTEST3	0x1B /* Chip test 3, R/W */
#define CTEST3_FLF	0x08
#define CTEST3_CLF	0x04
#define CTEST3_FM	0x02
#define CTEST3_WRIE	0x01

#define SIOP_TEMP	0x1C /* Temp register (used by CALL/RET), R/W */

#define SIOP_DFIFO	0x20 /* DMA FIFO */

#define SIOP_CTEST4	0x21 /* Chip test 4, R/W */
#define CTEST4_BDIS	0x80
#define CTEST_ZMOD	0x40
#define CTEST_ZSD	0x20
#define CTEST_SRTM	0x10
#define CTEST_MPEE	0x08

#define SIOP_CTEST5	0x22 /* Chip test 5, R/W */
#define CTEST5_ADCK	0x80
#define CTEST5_BBCK	0x40
#define CTEST5_DFS	0x20
#define CTEST5_MASR	0x10
#define CTEST5_DDIR	0x08
#define CTEST5_BOMASK	0x03

#define SIOP_CTEST6	0x23 /* Chip test 6, R/W */

#define SIOP_DBC	0x24 /* DMA byte counter, R/W */

#define SIOP_DCMD	0x27 /* DMA command, R/W */

#define SIOP_DNAD	0x28 /* DMA next addr, R/W */

#define SIOP_DSP	0x2C /* DMA scripts pointer, R/W */

#define SIOP_DSPS	0x30 /* DMA scripts pointer save, R/W */

#define SIOP_SCRATCHA	0x34 /* scratch register A. R/W */

#define SIOP_DMODE	0x38 /* DMA mode, R/W */
#define DMODE_BL_SHIFT   6
#define DMODE_BL_MASK	0xC0
#define DMODE_SIOM	0x20
#define DMODE_DIOM	0x10
#define DMODE_ERL	0x08
#define DMODE_ERMP	0x04
#define DMODE_BOF	0x02
#define DMODE_MAN	0x01

#define SIOP_DIEN	0x39 /* DMA interrupt enable, R/W */
#define DIEN_MDPE	0x40
#define DIEN_BF		0x20
#define DIEN_AVRT	0x10
#define DIEN_SSI	0x08
#define DIEN_SIR	0x04
#define DIEN_IID	0x01

#define SIOP_SBR	0x3A /* scratch byte register, R/W */

#define SIOP_DCNTL	0x3B /* DMA control, R/W */
#define DCNTL_CLSE	0x80
#define DCNTL_PFF	0x40
#define DCNTL_PFEN	0x20
#define DCNTL_SSM	0x10
#define DCNTL_IRQM	0x08
#define DCNTL_STD	0x04
#define DCNTL_IRQD	0x02
#define DCNTL_COM	0x01

#define SIOP_ADDER	0x3C /* adder output sum, RO */

#define SIOP_SIEN0	0x40 /* SCSI interrupt enable 0, R/W */
#define SIEN0_MA	0x80
#define SIEN0_CMP	0x40
#define SIEN0_SEL	0x20
#define SIEN0_RSL	0x10
#define SIEN0_SGE	0x08
#define SIEN0_UDC	0x04
#define SIEN0_SRT	0x02
#define SIEN0_PAR	0x01

#define SIOP_SIEN1	0x41 /* SCSI interrupt enable 1, R/W */
#define SIEN1_SBMC	0x10 /* 895 only */
#define SIEN1_STO	0x04
#define SIEN1_GEN	0x02
#define SIEN1_HTH	0x01

#define SIOP_SIST0	0x42 /* SCSI interrupt status 0, RO */
#define SIST0_MA	0x80
#define SIST0_CMP	0x40
#define SIST0_SEL	0x20
#define SIST0_RSL	0x10
#define SIST0_SGE	0x08
#define SIST0_UDC	0x04
#define SIST0_RST	0x02
#define SIST0_PAR	0x01

#define SIOP_SIST1	0x43 /* SCSI interrut status 1, RO */
#define SIST1_SBMC	0x10 /* 895 only */
#define SIST1_STO	0x04
#define SIST1_GEN	0x02
#define SIST1_HTH	0x01

#define SIOP_SLPAR	0x44 /* scsi longitudinal parity, R/W */

#define SIOP_SWIDE	0x45 /* scsi wide residue, RW, 875 only */

#define SIOP_MACNTL	0x46 /* memory access control, R/W */

#define SIOP_GPCNTL	0x47 /* General Purpose Pin control, R/W */
#define GPCNTL_ME	0x80	/* 875 only */
#define GPCNTL_FE	0x40	/* 875 only */
#define GPCNTL_IN4	0x10	/* 875 only */
#define GPCNTL_IN3	0x08	/* 875 only */
#define GPCNTL_IN2	0x04	/* 875 only */
#define GPCNTL_IN1	0x02
#define GPCNTL_IN0	0x01

#define SIOP_STIME0	0x48 /* SCSI timer 0, R/W */
#define STIME0_HTH_SHIFT 4
#define STIME0_HTH_MASK	0xf0
#define STIME0_SEL_SHIFT 0
#define STIME0_SEL_MASK	0x0f

#define SIOP_STIME1	0x49 /* SCSI timer 1, R/W */
#define STIME1_HTHBA	0x40	/* 875 only */
#define STIME1_GENSF	0x20	/* 875 only */
#define STIME1_HTHSF	0x10	/* 875 only */
#define STIME1_GEN_SHIFT 0
#define STIME1_GEN_MASK	0x0f

#define SIOP_RESPID0	0x4A /* response ID, R/W */

#define SIOP_RESPID1	0x4B /* response ID, R/W, 875-only */

#define SIOP_STEST0	0x4C /* SCSI test 0, RO */

#define SIOP_STEST1	0x4D /* SCSI test 1, RO, RW on 875 */
#define STEST1_DBLEN	0x08	/* 875-only */
#define STEST1_DBLSEL	0x04	/* 875-only */

#define SIOP_STEST2	0x4E /* SCSI test 2, RO, R/W on 875 */
#define STEST2_DIF	0x20	/* 875 only */
#define STEST2_EXT	0x02

#define SIOP_STEST3	0x4F /* SCSI test 3, RO, RW on 875 */
#define STEST3_TE	0x80
#define STEST3_HSC	0x20

#define SIOP_STEST4	0x52 /* SCSI test 4, 895 only */
#define STEST4_MODE_MASK 0xc0
#define STEST4_MODE_DIF	0x40
#define STEST4_MODE_SE	0x80
#define STEST4_MODE_LVD	0xc0
#define STEST4_LOCK	0x20
#define STEST4_

#define SIOP_SIDL	0x50 /* SCSI input data latch, RO */

#define SIOP_SODL	0x54 /* SCSI output data latch, R/W */

#define SIOP_SBDL	0x58 /* SCSI bus data lines, RO */

#define SIOP_SCRATCHB	0x5C /* Scratch register B, R/W */

#define SIOP_SCRATCHC	0x60 /* Scratch register C, R/W, 875 only */

#define SIOP_SCRATCHD	0x64 /* Scratch register D, R/W, 875-only */

#define SIOP_SCRATCHE	0x68 /* Scratch register E, R/W, 875-only */

#define SIOP_SCRATCHF	0x6c /* Scratch register F, R/W, 875-only */

#define SIOP_SCRATCHG	0x70 /* Scratch register G, R/W, 875-only */

#define SIOP_SCRATCHH	0x74 /* Scratch register H, R/W, 875-only */

#define SIOP_SCRATCHI	0x78 /* Scratch register I, R/W, 875-only */

#define SIOP_SCRATCHJ	0x7c /* Scratch register J, R/W, 875-only */

#define SIOP_SCNTL4	0xbc
#define SCNTL4_ULTRA3	0x80
#define SCNTL4_AIP	0x40

#define SIOP_DFBC	0xf0 /* DMA FIFO byte count, RO, C10-only */
