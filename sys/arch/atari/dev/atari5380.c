/*	$NetBSD: atari5380.c,v 1.5 1996/01/14 13:06:37 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

/*
 * Include the driver definitions
 */
#include <atari/dev/ncr5380reg.h>

#include <machine/stdarg.h>
#include <machine/iomap.h>
#include <machine/mfp.h>

#if defined(FALCON_SCSI)
#include <machine/dma.h>
#endif

/*
 * This is crap, but because the interrupts now run at MFP spl-level (6),
 * splbio() is not enough at some places. The code should be checked to
 * find out where splhigh() is needed and where splbio() should be used.
 * Now that I use this interrupt sceme, the spl values are fake!
 */
#undef splbio()
#define splbio()	splhigh()

/*
 * Set the various driver options
 */
#define	NREQ		18	/* Size of issue queue			*/
#define	AUTO_SENSE	1	/* Automatically issue a request-sense 	*/

#define	DRNAME		ncrscsi	/* used in various prints		*/
#undef	DBG_SEL			/* Show the selection process		*/
#undef	DBG_REQ			/* Show enqueued/ready requests		*/
#undef	DBG_ERR_RET		/* Show requests with != 0 return code	*/
#undef	DBG_NOWRITE		/* Do not allow writes to the targets	*/
#undef	DBG_PIO			/* Show the polled-I/O process		*/
#undef	DBG_INF			/* Show information transfer process	*/
#define	DBG_NOSTATIC		/* No static functions, all in DDB trace*/
#define	DBG_PID		2	/* Keep track of driver			*/
#define	REAL_DMA		/* Use DMA if sensible			*/
#if defined(FALCON_SCSI)
#define	REAL_DMA_POLL	1	/* 1: Poll for end of DMA-transfer	*/
#else
#define	REAL_DMA_POLL	0	/* 1: Poll for end of DMA-transfer	*/
#endif
#undef	USE_PDMA		/* Use special pdma-transfer function	*/
#define MIN_PHYS	65536	/*BARF!!!!*/

/*
 * The atari specific driver options
 */
#undef	NO_TTRAM_DMA		/* Do not use DMA to TT-ram. This	*/
				/*    fails on older atari's		*/
#define	ENABLE_NCR5380(sc)	cur_softc = sc;

/*
 * Functions that do nothing on the atari
 */
#define	pdma_ready()		0

#if defined(TT_SCSI)
/*
 * Define all the things we need of the DMA-controller
 */
#define	SCSI_DMA	((struct scsi_dma *)AD_SCSI_DMA)
#define	SCSI_5380	((struct scsi_5380 *)AD_NCR5380)

struct scsi_dma {
	volatile u_char		s_dma_ptr[8];	/* use only the odd bytes */
	volatile u_char		s_dma_cnt[8];	/* use only the odd bytes */
	volatile u_char		s_dma_res[4];	/* data residue register  */
	volatile u_char		s_dma_gap;	/* not used		  */
	volatile u_char		s_dma_ctrl;	/* control register	  */
};

#define	set_scsi_dma(addr, val)	(void)(					\
	{								\
	u_char	*address = (u_char*)addr+1;				\
	u_long	nval	 = (u_long)val;					\
	__asm("movepl	%0, %1@(0)": :"d" (nval), "a" (address));	\
	})

#define	get_scsi_dma(addr, res)	(					\
	{								\
	u_char	*address = (u_char*)addr+1;				\
	u_long	nval;							\
	__asm("movepl	%1@(0), %0": "=d" (nval) : "a" (address));	\
	res = (u_long)nval;						\
	})

/*
 * Defines for TT-DMA control register
 */
#define	SD_BUSERR	0x80		/* 1 = transfer caused bus error*/
#define	SD_ZERO		0x40		/* 1 = byte counter is zero	*/
#define	SD_ENABLE	0x02		/* 1 = Enable DMA		*/
#define	SD_OUT		0x01		/* Direction: memory to SCSI	*/
#define	SD_IN		0x00		/* Direction: SCSI to memory	*/

/*
 * Define the 5380 register set
 */
struct scsi_5380 {
	volatile u_char	scsi_5380[16];	/* use only the odd bytes	*/
};
#endif /* TT_SCSI */

/**********************************************
 * Variables present for both TT and Falcon.  *
 **********************************************/

/*
 * Softc of currently active controller (a bit of fake; we only have one)
 */
static struct ncr_softc	*cur_softc;

#if defined(TT_SCSI) && !defined(FALCON_SCSI)
/*
 * We can be more efficient for some functions when only TT_SCSI is selected
 */
#define	GET_5380_REG(rnum)	SCSI_5380->scsi_5380[(rnum << 1) | 1]
#define	SET_5380_REG(rnum,val)	(SCSI_5380->scsi_5380[(rnum << 1) | 1] = val)

#define scsi_mach_init(sc)	scsi_tt_init(sc)
#define scsi_ienable()		scsi_tt_ienable()
#define scsi_idisable()		scsi_tt_idisable()
#define	scsi_clr_ipend()	scsi_tt_clr_ipend()
#define scsi_dma_setup(r,p,m)	scsi_tt_dmasetup(r, p, m)
#define wrong_dma_range(r,d)	tt_wrong_dma_range(r, d)
#define poll_edma(reqp)		tt_poll_edma(reqp)
#define get_dma_result(r, b)	tt_get_dma_result(r, b)

#define fair_to_keep_dma()	1
#define claimed_dma()		1
#define reconsider_dma()

#endif /* defined(TT_SCSI) && !defined(FALCON_SCSI) */

#if defined(TT_SCSI)

/*
 * Define these too, so we can use them locally...
 */
#define	GET_TT_REG(rnum)	SCSI_5380->scsi_5380[(rnum << 1) | 1]
#define	SET_TT_REG(rnum,val)	(SCSI_5380->scsi_5380[(rnum << 1) | 1] = val)

#ifdef NO_TTRAM_DMA
static int tt_wrong_dma_range(reqp, dm)
SC_REQ			*reqp;
struct dma_chain	*dm;
{
	if (dm->dm_addr & 0xff000000) {
		reqp->dr_flag |= DRIVER_BOUNCING;
		return(1);
	}
	return(0);
}
#else
#define	tt_wrong_dma_range(reqp, dm)	0
#endif

static void scsi_tt_init(sc)
struct ncr_softc	*sc;
{
	/*
	 * Enable SCSI-related interrupts
	 */
	MFP2->mf_aer  |= 0x80;		/* SCSI IRQ goes HIGH!!!!!	*/

	MFP2->mf_ierb |= IB_SCDM;	/* SCSI-dma interrupts		*/
	MFP2->mf_iprb &= ~IB_SCDM;
	MFP2->mf_imrb |= IB_SCDM;

	MFP2->mf_iera |= IA_SCSI;	/* SCSI-5380 interrupts		*/
	MFP2->mf_ipra &= ~IA_SCSI;
	MFP2->mf_imra |= IA_SCSI;

	/*
	 * LWP: DMA transfers to TT-ram causes data to be garbeled
	 * without notice on some revisons of the TT-mainboard.
	 * When program's generate misterious Segmentations faults,
	 * try turning on NO_TTRAM_DMA.
	 */
#ifdef NO_TTRAM_DMA
	printf(": DMA to TT-RAM is disabled!");
#endif
}

static u_char get_tt_5380_reg(rnum)
u_short	rnum;
{
	return(SCSI_5380->scsi_5380[(rnum << 1) | 1]);
}

static void set_tt_5380_reg(rnum, val)
u_short	rnum, val;
{
	SCSI_5380->scsi_5380[(rnum << 1) | 1] = val;
}

extern __inline__ void scsi_tt_ienable()
{
	int	sps = splbio();
	MFP2->mf_ierb |= IB_SCDM;
	MFP2->mf_iera |= IA_SCSI;
	splx(sps);
}

extern __inline__ scsi_tt_idisable()
{
	int	sps = splbio();
	MFP2->mf_ierb &= ~IB_SCDM;
	MFP2->mf_iera &= ~IA_SCSI;
	splx(sps);
}

extern __inline__ scsi_tt_clr_ipend()
{
	int	tmp;

	SCSI_DMA->s_dma_ctrl = 0;
	tmp = GET_TT_REG(NCR5380_IRCV);
}

static void scsi_tt_dmasetup(reqp, phase, mode)
SC_REQ	*reqp;
u_int	phase;
u_char	mode;
{
	if (PH_IN(phase)) {
		SCSI_DMA->s_dma_ctrl = SD_IN;
		set_scsi_dma(&(SCSI_DMA->s_dma_ptr), reqp->dm_cur->dm_addr);
		set_scsi_dma(&(SCSI_DMA->s_dma_cnt), reqp->dm_cur->dm_count);
		SET_TT_REG(NCR5380_ICOM, 0);
		SET_TT_REG(NCR5380_MODE, mode);
		SCSI_DMA->s_dma_ctrl = SD_ENABLE;
		SET_TT_REG(NCR5380_IRCV, 0);
	}
	else {
		SCSI_DMA->s_dma_ctrl = SD_OUT;
		set_scsi_dma(&(SCSI_DMA->s_dma_ptr), reqp->dm_cur->dm_addr);
		set_scsi_dma(&(SCSI_DMA->s_dma_cnt), reqp->dm_cur->dm_count);
		SET_TT_REG(NCR5380_MODE, mode);
		SET_TT_REG(NCR5380_ICOM, SC_ADTB);
		SET_TT_REG(NCR5380_DMSTAT, 0);
		SCSI_DMA->s_dma_ctrl = SD_ENABLE|SD_OUT;
	}
}

static int
tt_poll_edma(reqp)
SC_REQ	*reqp;
{
	u_char	dmstat, dmastat;
	int	timeout = 9000; /* XXX */

	/*
	 * We wait here until the DMA has finished. This can be
	 * achieved by checking the following conditions:
	 *   - 5380:
	 *	- End of DMA flag is set
	 *	- We lost BSY (error!!)
	 *	- A phase mismatch has occured (partial transfer)
	 *   - DMA-controller:
	 *	- A bus error occurred (Kernel error!!)
	 *	- All bytes are transferred
	 * If one of the terminating conditions was met, we call
	 * 'dma_ready' to check errors and perform the bookkeeping.
	 */

	for (;;) {
		delay(20);
		if (--timeout <= 0) {
			ncr_tprint(reqp, "timeout on polled transfer\n");
			reqp->xs->error = XS_DRIVER_STUFFUP;
			return(0);
		}
		dmstat  = GET_TT_REG(NCR5380_DMSTAT);
		dmastat = SCSI_DMA->s_dma_ctrl;
		if (dmstat & (SC_END_DMA|SC_BSY_ERR|SC_IRQ_SET))
			break;
		if (!(dmstat & SC_PHS_MTCH))
			break;
		if (dmastat & (SD_BUSERR|SD_ZERO))
			break;
	}
	return(1);
}

/*
 * Convert physical DMA address to a virtual address.
 */
static u_char *
ptov(reqp, phaddr)
SC_REQ	*reqp;
u_long	*phaddr;
{
	struct dma_chain	*dm;
	u_char			*vaddr;

	dm = reqp->dm_chain;
	vaddr = reqp->xdata_ptr;
	for(; dm < reqp->dm_cur; dm++)
		vaddr += dm->dm_count;
	vaddr += (u_long)phaddr - dm->dm_addr;
	return(vaddr);
}

static int
tt_get_dma_result(reqp, bytes_left)
SC_REQ	*reqp;
u_long	*bytes_left;
{
	int	dmastat, dmstat;
	u_char	*byte_p;
	u_long	leftover;

	dmastat = SCSI_DMA->s_dma_ctrl;
	dmstat  = GET_TT_REG(NCR5380_DMSTAT);
	get_scsi_dma(SCSI_DMA->s_dma_cnt, leftover);
	get_scsi_dma(SCSI_DMA->s_dma_ptr, (u_long)byte_p);

	if (dmastat & SD_BUSERR) {
		/*
		 * The DMA-controller seems to access 8 bytes beyond
		 * it's limits on output. Therefore check also the byte
		 * count. If it's zero, ignore the bus error.
		 */
		if (leftover != 0) {
			ncr_tprint(reqp,
				"SCSI-DMA buserror - accessing 0x%x\n", byte_p);
			reqp->xs->error = XS_DRIVER_STUFFUP;
		}
	}

	/*
	 * We handle the following special condition below:
	 *  -- The device disconnects in the middle of a write operation --
	 * In this case, the 5380 has already pre-fetched the next byte from
	 * the DMA-controller before the phase mismatch occurs. Therefore,
	 * leftover is 1 too low.
	 * This does not always happen! Therefore, we only do this when
	 * leftover is odd. This assumes that DMA transfers are _even_! This
	 * is normally the case on disks and types but might not always be.
	 * XXX: Check if ACK is consistently high on these occasions LWP
	 */
	if ((leftover & 1) && !(dmstat & SC_PHS_MTCH) && PH_OUT(reqp->phase))
		leftover++;

	/*
	 * Check if there are some 'restbytes' left in the DMA-controller.
	 */
	if (((u_long)byte_p & 3) && PH_IN(reqp->phase)) {
		u_char	*p, *q;

		p = ptov(reqp, (u_long)byte_p & ~3);
		q = (u_char*)&(SCSI_DMA->s_dma_res);
		switch ((u_long)byte_p & 3) {
			case 3: *p++ = *q++;
			case 2: *p++ = *q++;
			case 1: *p++ = *q++;
		}
	}
	*bytes_left = leftover;
	return ((dmastat & (SD_BUSERR|SD_ZERO)) ? 1 : 0);
}

#endif /* defined(TT_SCSI) */

#if defined(FALCON_SCSI) && !defined(TT_SCSI)

#define	GET_5380_REG(rnum)	get_falcon_5380_reg(rnum)
#define	SET_5380_REG(rnum,val)	set_falcon_5380_reg(rnum, val)
#define scsi_mach_init(sc)	scsi_falcon_init(sc)
#define scsi_ienable()		scsi_falcon_ienable()
#define scsi_idisable()		scsi_falcon_idisable()
#define	scsi_clr_ipend()	scsi_falcon_clr_ipend()
#define scsi_dma_setup(r,p,m)	scsi_falcon_dmasetup(r, p, m)
#define wrong_dma_range(r,d)	falcon_wrong_dma_range(r, d)
#define poll_edma(reqp)		falcon_poll_edma(reqp)
#define get_dma_result(r, b)	falcon_get_dma_result(r, b)

#define fair_to_keep_dma()	(!st_dmawanted())
#define claimed_dma()		falcon_claimed_dma()
#define reconsider_dma()	falcon_reconsider_dma()

#endif /* defined(FALCON_SCSI) && !defined(TT_SCSI) */

#if defined(FALCON_SCSI)

static void fscsi_int __P((void));

static void scsi_falcon_init(sc)
struct ncr_softc	*sc;
{
	/*
	 * Enable disk related interrupts
	 */
	MFP->mf_ierb  |= IB_DINT;
	MFP->mf_iprb  &= ~IB_DINT;
	MFP->mf_imrb  |= IB_DINT;
}

static u_char get_falcon_5380_reg(rnum)
u_short	rnum;
{
	DMA->dma_mode = DMA_SCSI + rnum;
	return(DMA->dma_data);
}

static void set_falcon_5380_reg(rnum, val)
u_short	rnum, val;
{
	DMA->dma_mode = DMA_SCSI + rnum;
	DMA->dma_data = val;
}

extern __inline__ void scsi_falcon_ienable()
{
	MFP->mf_ierb  |= IB_DINT;
}

extern __inline__ scsi_falcon_idisable()
{
	MFP->mf_ierb  &= ~IB_DINT;
}

extern __inline__ scsi_falcon_clr_ipend()
{
	int	tmp;

	tmp = get_falcon_5380_reg(NCR5380_IRCV);
}

static int falcon_wrong_dma_range(reqp, dm)
SC_REQ			*reqp;
struct dma_chain	*dm;
{
	/*
	 * Do not allow chains yet! See also comment with
	 * falcon_poll_edma() !!!
	 */
	if (((dm - reqp->dm_chain) > 0) || (dm->dm_addr & 0xff000000)) {
		reqp->dr_flag |= DRIVER_BOUNCING;
		return(1);
	}
	/*
	 * Never allow DMA to happen on a Falcon when the transfer
	 * size is no multiple of 512. This is the transfer unit of the
	 * ST DMA-controller.
	 */
	if(dm->dm_count & 511)
		return(1);
	return(0);
}

static	int falcon_lock = 0;

extern __inline__ falcon_claimed_dma()
{
	if (!(falcon_lock & DMA_LOCK_GRANT)) {
		if (falcon_lock) {
			/*
			 * DMA access is being claimed.
			 */
			return(0);
		}
		if (!st_dmagrab(fscsi_int, run_main, &connected,&falcon_lock,1))
			return(0);
	}
	return(1);
}

extern __inline__ void falcon_reconsider_dma()
{
	if (falcon_lock && (connected == NULL) && (discon_q == NULL)) {
		/*
		 * No need to keep DMA locked by us as we are not currently
		 * connected and no disconnected jobs are pending.
		 */
		st_dmafree(&connected, &falcon_lock);
	}

	if (!falcon_lock && (issue_q != NULL)) {
		/*
		 * We must (re)claim DMA access as there are jobs
		 * waiting in the issue queue.
		 */
		st_dmagrab(fscsi_int, run_main, &connected, &falcon_lock, 0);
	}
}

static void fal1_dma(dir, nsects, reqp)
u_int	dir, nsects;
SC_REQ	*reqp;
{
	dir <<= 8;
	st_dmaaddr_set((caddr_t)reqp->dm_cur->dm_addr);
	DMA->dma_mode = 0x90 | dir;
	DMA->dma_mode = 0x90 | (dir ^ DMA_WRBIT);
	DMA->dma_mode = 0x90 | dir;
	delay(40);	/* XXX: LWP - is this really needed ? */
	DMA->dma_data = nsects;
	delay(40);	/* XXX: LWP - is this really needed ? */
	DMA->dma_mode = 0x10 | dir;
	delay(40);	/* XXX: LWP - is this really needed ? */
}

static void scsi_falcon_dmasetup(reqp, phase, mode)
SC_REQ	*reqp;
u_int	phase;
u_char	mode;
{
	int	nsects = reqp->dm_cur->dm_count / 512; /* XXX */

	/*
	 * XXX: We should probably clear the fifo before putting the
	 *      5380 into DMA-mode.
	 */
	if (PH_IN(phase)) {
		set_falcon_5380_reg(NCR5380_ICOM, 0);
		set_falcon_5380_reg(NCR5380_MODE, mode);
		set_falcon_5380_reg(NCR5380_IRCV, 0);
		fal1_dma(0, nsects, reqp);
	}
	else {
		set_falcon_5380_reg(NCR5380_MODE, mode);
		set_falcon_5380_reg(NCR5380_ICOM, SC_ADTB);
		set_falcon_5380_reg(NCR5380_DMSTAT, 0);
		fal1_dma(1, nsects, reqp);
	}
}

/*
 * Falcon SCSI interrupt. _Always_ called at spl1!
 */
static void fscsi_int()
{
	int	itype;
	int	dma_done;

	if (get_falcon_5380_reg(NCR5380_DMSTAT) & SC_IRQ_SET) {
		scsi_falcon_idisable();
		ncr_ctrl_intr(cur_softc);
	}
}

static int
falcon_poll_edma(reqp)
SC_REQ	*reqp;
{
	int	timeout = 9000; /* XXX */

	/*
	 * Because of the Falcon hardware, it is impossible to reach
	 * the 5380 while doing DMA-transfers. So we have to rely on
	 * the interrupt line to determine if DMA-has finished. the
	 * DMA-controller itself will never fire an interrupt. This means
	 * that 'broken-up' DMA transfers are not (yet) possible on the
	 * Falcon.
	 */
	for (;;) {
		delay(20);
		if (--timeout <= 0) {
			ncr_tprint(reqp, "Timeout on polled transfer\n");
			reqp->xs->error = XS_DRIVER_STUFFUP;
			return(0);
		}
		if (!(MFP->mf_gpip & IO_DINT))
			break;
	}
	return(1);
}

static int
falcon_get_dma_result(reqp, bytes_left)
SC_REQ	*reqp;
u_long	*bytes_left;
{
	int	rv = 0;
	int	st_dmastat;
	u_long	bytes_done;

	/*
	 * Select sector counter register first (See Atari docu.)
	 */
	DMA->dma_mode = 0x90;
	if (!(st_dmastat = DMA->dma_stat) & 0x01) {
		/*
		 * Misc. DMA-error according to Atari...
		 */
		ncr_tprint(reqp, "Unknow ST-SCSI error near 0x%x\n",
							st_dmaaddr_get());
		reqp->xs->error = XS_DRIVER_STUFFUP;
		rv = 1;
	}
	if (st_dmastat & 0x02) {
		/*
		 * Bytecount not zero.... As the fifo loads in 16 byte
		 * chunks, check if bytes are stuck in fifo.
		 * As we don't use DMA on chunks less than 512 bytes
		 * on the Falcon, report any residual not a multiple of
		 * 512 as an error...
		 */
		bytes_done = st_dmaaddr_get() - reqp->dm_cur->dm_addr;
		if (bytes_done & 511) {
			ncr_tprint(reqp, "Some bytes stuck in fifo\n");
			bytes_done &= ~511;
			reqp->xs->error = XS_DRIVER_STUFFUP;
		}
		*bytes_left = reqp->dm_cur->dm_count - bytes_done; 
	}
	else {
		*bytes_left = 0;
		rv = 1;
	}
	return(rv);
}

#endif /* defined(FALCON_SCSI) */

#if defined(TT_SCSI) && defined(FALCON_SCSI)
/*
 * Define some functions to support _both_ TT and Falcon SCSI
 */

/*
 * Register access will be done through the following 2 function pointers.
 */
static u_char	(*get_5380_reg)();
static void	(*set_5380_reg)();

#define	GET_5380_REG	(*get_5380_reg)
#define	SET_5380_REG	(*set_5380_reg)

static void scsi_mach_init(sc)
struct ncr_softc	*sc;
{
	if (machineid & ATARI_FALCON) {
		get_5380_reg = get_falcon_5380_reg;
		set_5380_reg = set_falcon_5380_reg;
		scsi_falcon_init(sc);
	}
	else {
		get_5380_reg = get_tt_5380_reg;
		set_5380_reg = set_tt_5380_reg;
		scsi_tt_init(sc);
	}
}

extern __inline__ void scsi_ienable()
{
	if (machineid & ATARI_FALCON)
		scsi_falcon_ienable();
	else scsi_tt_ienable();
}

extern __inline__ void scsi_idisable()
{
	if (machineid & ATARI_FALCON)
		scsi_falcon_idisable();
	else scsi_tt_idisable();
}

extern __inline__ scsi_clr_ipend()
{
	if (machineid & ATARI_FALCON)
		scsi_falcon_clr_ipend();
	else scsi_tt_clr_ipend();
}

extern __inline__ scsi_dma_setup(reqp, phase, mbase)
SC_REQ	*reqp;
u_int	phase;
u_char	mbase;
{
	if (machineid & ATARI_FALCON)
		scsi_falcon_dmasetup(reqp, phase, mbase);
	else scsi_tt_dmasetup(reqp, phase, mbase);
}

extern __inline__ int wrong_dma_range(reqp, dm)
SC_REQ			*reqp;
struct dma_chain	*dm;
{
	if (machineid & ATARI_FALCON)
		return(falcon_wrong_dma_range(reqp, dm));
	else return(tt_wrong_dma_range(reqp, dm));
}

extern __inline__ int poll_edma(reqp)
SC_REQ	*reqp;
{
	if (machineid & ATARI_FALCON)
		return(falcon_poll_edma(reqp));
	else return(tt_poll_edma(reqp));
}

extern __inline__ int get_dma_result(reqp, bytes_left)
SC_REQ	*reqp;
u_long	*bytes_left;
{
	if (machineid & ATARI_FALCON)
		return(falcon_get_dma_result(reqp, bytes_left));
	else return(tt_get_dma_result(reqp, bytes_left));
}

/*
 * Locking stuff. All turns into NOP's on the TT.
 */
#define	fair_to_keep_dma()	((machineid & ATARI_FALCON) ?		\
						!st_dmawanted() : 1)
#define	claimed_dma()		((machineid & ATARI_FALCON) ?		\
						falcon_claimed_dma() : 1)
#define reconsider_dma()	{					\
					if(machineid & ATARI_FALCON)	\
						falcon_reconsider_dma();\
				}
#endif /* defined(TT_SCSI) && defined(FALCON_SCSI) */

/**********************************************
 * Functions present for both TT and Falcon.  *
 **********************************************/
/*
 * Our autoconfig matching function
 */
static int
machine_match(pdp, cdp, auxp, cd)
struct device	*pdp;
struct cfdata	*cdp;
void		*auxp;
struct cfdriver *cd;
{
	if (strcmp(auxp, cd->cd_name))
		return(0);
	if (cdp->cf_unit != 0)	/* Only one unit	*/
		return(0);
	return(1);
}

/*
 * Bounce buffer (de)allocation. Those buffers are gotten from the ST-mem
 * pool. Allocation here is both contiguous and in the lower 16Mb of
 * the address space. Thus being DMA-able for all controllers.
 */
static u_char *
alloc_bounceb(len)
u_long len;
{
	u_long	tmp;

	return((u_char *)alloc_stmem(len, &tmp));
}

static void
free_bounceb(bounceb)
u_char	*bounceb;
{
	free_stmem(bounceb);
}

/*
 * 5380 interrupt.
 */
scsi_ctrl(sr)
int	sr;	/* sr at time of interrupt */
{
	if (GET_5380_REG(NCR5380_DMSTAT) & SC_IRQ_SET) {
		scsi_idisable();
		if (!BASEPRI(sr))
			add_sicallback(ncr_ctrl_intr, cur_softc, 0);
		else {
			spl1();
			ncr_ctrl_intr(cur_softc);
		}
	}
}

/*
 * DMA controller interrupt
 */
scsi_dma(sr)
int	sr;	/* sr at time of interrupt */
{
	SC_REQ	*reqp;

	if ((reqp = connected) && (reqp->dr_flag & DRIVER_IN_DMA)) {
		scsi_idisable();
		if (!BASEPRI(sr))
			add_sicallback(ncr_dma_intr, cur_softc, 0);
		else {
			spl1();
			ncr_dma_intr(cur_softc);
		}
	}
}

/*
 * Last but not least... Include the general driver code
 */
#include "atari/dev/ncr5380.c"
