/*	$OpenBSD: ncr.c,v 1.4 2000/03/03 00:54:54 todd Exp $ */
/*	$NetBSD: ncr.c,v 1.21 1995/11/30 00:58:47 jtc Exp $ */

/*
 * Copyright (c) 1994 Matthias Pfaller.
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
 *	This product includes software developed by Matthias Pfaller.
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>
#include <machine/stdarg.h>

/*
 * Include the driver definitions
 */
#include "ncr5380reg.h"
#include "ncrreg.h"

/*
 * Set the various driver options
 */
#define	NREQ		18	/* Size of issue queue			*/
#define	AUTO_SENSE	1	/* Automatically issue a request-sense 	*/

#define	DRNAME		ncr	/* used in various prints	*/
#undef	DBG_SEL			/* Show the selection process		*/
#undef	DBG_REQ			/* Show enqueued/ready requests		*/
#undef	DBG_NOWRITE		/* Do not allow writes to the targets	*/
#undef	DBG_PIO			/* Show the polled-I/O process		*/
#undef	DBG_INF			/* Show information transfer process	*/
#undef 	DBG_NOSTATIC		/* No static functions, all in DDB trace*/
#define	DBG_PID			/* Keep track of driver			*/
#undef 	REAL_DMA		/* Use DMA if sensible			*/
#undef 	REAL_DMA_POLL	0	/* 1: Poll for end of DMA-transfer	*/
#define	USE_PDMA		/* Use special pdma-transfer function	*/

/*
 * Softc of currently active controller (a bit of fake; we only have one)
 */
static struct ncr_softc	*cur_softc;

/*
 * Function decls:
 */
static int transfer_pdma __P((u_char *, u_char *, u_long *));
static void ncr_intr __P((void *));
static void ncr_soft_intr __P((void *));
#define scsi_dmaok(x)		0
#define pdma_ready()		0
#define fair_to_keep_dma()	1
#define claimed_dma()		1
#define reconsider_dma()
#define ENABLE_NCR5380(sc) do { \
		scsi_select_ctlr(DP8490); \
		cur_softc = sc; \
	} while (0)

void
delay(timeo)
	int timeo;
{
	int	len;
	for (len=0; len < timeo * 2; len++);
}

static int
machine_match(pdp, cdp, auxp, cfd)
	struct device	*pdp;
	struct cfdata	*cdp;
	void		*auxp;
	struct cfdriver	*cfd;
{
	if(cdp->cf_unit != 0)	/* Only one unit	*/
		return(0);
	return(1);
}

static void
scsi_mach_init(sc)
	struct ncr_softc *sc;
{
	register int i;
	intr_disable(IR_SCSI1);
	i = intr_establish(SOFTINT, ncr_soft_intr, sc, sc->sc_dev.dv_xname,
		IPL_BIO, 0);
	intr_establish(IR_SCSI1, ncr_intr, (void *)i, sc->sc_dev.dv_xname,
		IPL_BIO, RISING_EDGE);
	printf(" addr 0x%x, irq %d", NCR5380, IR_SCSI1);
}

/*
 * 5380 interrupt.
 */
static void
ncr_intr(softint)
	void *softint;
{
	int ctrlr = scsi_select_ctlr(DP8490);
	if (NCR5380->ncr_dmstat & SC_IRQ_SET) {
		intr_disable(IR_SCSI1);
		softintr((int)softint);
	}
	scsi_select_ctlr(ctrlr);
}

static void
ncr_soft_intr(sc)
	void *sc;
{
	int ctrlr = scsi_select_ctlr(DP8490);
	ncr_ctrl_intr(sc);
	intr_enable(IR_SCSI1);
	scsi_select_ctlr(ctrlr);
}

/*
 * PDMA stuff
 */
#define movsd(from, to, n) do { \
		register int r0 __asm ("r0") = n; \
		register u_char *r1 __asm("r1") = from; \
		register u_char *r2 __asm("r2") = to; \
		__asm volatile ("movsd" \
			: "=r" (r1), "=r" (r2) \
			: "0" (r1), "1" (r2), "r" (r0) \
			: "r0", "memory" \
		); \
		from = r1; to = r2; \
	} while (0)

#define movsb(from, to, n) do { \
		register int r0 __asm ("r0") = n; \
		register u_char *r1 __asm("r1") = from; \
		register u_char *r2 __asm("r2") = to; \
		__asm volatile ("movsb" \
			: "=r" (r1), "=r" (r2) \
			: "0" (r1), "1" (r2), "r" (r0) \
			: "r0", "memory" \
		); \
		from = r1; to = r2; \
	} while (0)

#define TIMEOUT	1000000
#define READY(dataout) do { \
		for (i = TIMEOUT; i > 0; i--) { \
			/*if (!(NCR5380->ncr_dmstat & SC_PHS_MTCH)) {*/ \
			if (NCR5380->ncr_dmstat & SC_IRQ_SET) { \
				if (dataout) NCR5380->ncr_icom &= ~SC_ADTB; \
				NCR5380->ncr_mode = IMODE_BASE; \
				*count = len; \
				if ((idstat = NCR5380->ncr_idstat) & SC_S_REQ) \
					*phase = (idstat >> 2) & 7; \
				else \
					*phase = NR_PHASE; \
				return(1); \
			} \
			if (NCR5380->ncr_dmstat & SC_DMA_REQ) break; \
			delay(1); \
		} \
		if (i <= 0) panic("ncr0: pdma timeout"); \
	} while (0)

#define byte_data ((volatile u_char *)pdma)
#define word_data ((volatile u_short *)pdma)
#define long_data ((volatile u_long *)pdma)

#define W1(n)	*byte_data = *(data + n)
#define W2(n)	*word_data = *((u_short *)data + n)
#define W4(n)	*long_data = *((u_long *)data + n)
#define R1(n)	*(data + n) = *byte_data
#define R4(n)	*((u_long *)data + n) = *long_data

static int
transfer_pdma(phase, data, count)
	u_char *phase;
	u_char *data;
	u_long *count;
{
	register volatile u_char *pdma = PDMA_ADDRESS;
	register int len = *count, i, idstat;

	if (len < 256) {
		transfer_pio(phase, data, count, 0);
		return(1);
	}
	NCR5380->ncr_tcom = *phase;
	scsi_clr_ipend();
	if (PH_IN(*phase)) {
		NCR5380->ncr_icom = 0;
		NCR5380->ncr_mode = IMODE_BASE | SC_M_DMA | SC_MON_BSY;
		NCR5380->ncr_ircv = 0;
		while (len >= 256) {
			READY(0);
			di();
			movsd((u_char *)pdma, data, 64);
			len -= 256;
			ei();
		}
		if (len) {
			di();
			while (len) {
				READY(0);
				R1(0);
				data++;
				len--;
			}
			ei();
		}
	} else {
		NCR5380->ncr_mode = IMODE_BASE | SC_M_DMA | SC_MON_BSY;
		NCR5380->ncr_icom = SC_ADTB;
		NCR5380->ncr_dmstat = SC_S_SEND;
		while (len >= 256) {
			/* The second ready is to
			 * compensate for DMA-prefetch.
			 * Since we adjust len only at
			 * the end of the block, there
			 * is no need to correct the
			 * residue.
			 */
			READY(1);
			di();
			W1(0); READY(1); W1(1); W2(1);
			data += 4;
			movsd(data, (u_char *)pdma, 63);
			ei();
			len  -= 256;
		}
		if (len) {
			READY(1);
			di();
			while (len) {
				W1(0);
				READY(1);
				data++;
				len--;
			}
			ei();
		}
		i = TIMEOUT;
		while (((NCR5380->ncr_dmstat & (SC_DMA_REQ|SC_PHS_MTCH))
			== SC_PHS_MTCH) && --i);
		if (!i)
			printf("ncr0: timeout waiting for SC_DMA_REQ.\n");
		*byte_data = 0;
	}

ncr_timeout_error:
	NCR5380->ncr_mode = IMODE_BASE;
	if((idstat = NCR5380->ncr_idstat) & SC_S_REQ)
		*phase = (idstat >> 2) & 7;
	else
		*phase = NR_PHASE;
	*count = len;
	return(1);
}

/*
 * Last but not least... Include the general driver code
 */
#include "ncr5380.c"
