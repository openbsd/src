/*	$OpenBSD: ises.c,v 1.8 2001/06/23 18:30:37 deraadt Exp $	*/

/*
 * Copyright (c) 2000, 2001 Håkan Olsson (ho@crt.se)
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCC-ISES hardware crypto accelerator
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/timeout.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>
#include <dev/rndvar.h>
#include <sys/md5k.h>
#include <crypto/sha1.h>
#include <crypto/rmd160.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/isesreg.h>
#include <dev/pci/isesvar.h>

/*
 * Prototypes and count for the pci_device structure
 */
int	ises_match __P((struct device *, void *, void *));
void	ises_attach __P((struct device *, struct device *, void *));

void	ises_initstate __P((void *));
void	ises_hrng_init __P((struct ises_softc *));
void	ises_hrng __P((void *));
void	ises_process_oqueue __P((struct ises_softc *));
int	ises_queue_cmd __P((struct ises_softc *, u_int32_t, u_int32_t *, 
			    u_int32_t (*)(struct ises_softc *, 
					  struct ises_cmd *)));
u_int32_t ises_get_fwversion __P((struct ises_softc *));
int	ises_assert_cmd_mode __P((struct ises_softc *));

int	ises_intr __P((void *));
int	ises_newsession __P((u_int32_t *, struct cryptoini *));
int	ises_freesession __P((u_int64_t));
int	ises_process __P((struct cryptop *));
void	ises_callback __P((struct ises_q *));
int	ises_feed __P((struct ises_softc *));
void	ises_bchu_switch_session __P((struct ises_softc *, 
				      struct ises_bchu_session *));

/* XXX for now... */
void	ubsec_mcopy __P((struct mbuf *, struct mbuf *, int, int));

#define READ_REG(sc,r) \
    bus_space_read_4((sc)->sc_memt, (sc)->sc_memh,r)

#define WRITE_REG(sc,reg,val) \
    bus_space_write_4((sc)->sc_memt, (sc)->sc_memh, reg, val)

/* XXX This should probably be (x) = htole32((x)) */
#define SWAP32(x) ((x) = swap32((x)))

#ifdef ISESDEBUG
#  define DPRINTF(x) printf x
#else
#  define DPRINTF(x)
#endif

#ifdef ISESDEBUG
void	ises_debug_init __P((struct ises_softc *));
void	ises_debug_2 __P((void));
void	ises_debug_loop __P((void *));
void	ises_showreg __P((void));
void	ises_debug_parse_omr __P((struct ises_softc *));
void	ises_debug_simple_cmd __P((struct ises_softc *, u_int32_t, u_int32_t));
struct ises_softc *ises_sc;
struct timeout ises_db_timeout;
int ises_db;
#endif

/* For HRNG entropy collection, these values gather 1600 bytes/s */
#ifndef ISESRNGBITS
#define ISESRNGBITS	128		/* Bits per iteration (mult. of 32) */
#define ISESRNGIPS	100		/* Iterations per second */
#endif

struct cfattach ises_ca = {
	sizeof(struct ises_softc), ises_match, ises_attach,
};

struct cfdriver ises_cd = {
	0, "ises", DV_DULL
};

struct ises_stats {
	u_int64_t	ibytes;
	u_int64_t	obytes;
	u_int32_t	ipkts;
	u_int32_t	opkts;
	u_int32_t	invalid;
	u_int32_t	nomem;
} isesstats;

int
ises_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_PIJNENBURG &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_PIJNENBURG_PCC_ISES)
		return (1);

	return (0);
}

void
ises_attach(struct device *parent, struct device *self, void *aux)
{
	struct ises_softc *sc = (struct ises_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_addr_t membase;
	bus_size_t memsize;
	u_int32_t cmd;

	bus_dma_segment_t seg;
	int nsegs, error, state;

	SIMPLEQ_INIT(&sc->sc_queue);
	SIMPLEQ_INIT(&sc->sc_qchip);
	SIMPLEQ_INIT(&sc->sc_cmdq);
	state = 0;

	/* Verify PCI space */
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}

	if (!(cmd & PCI_COMMAND_MASTER_ENABLE)) {
		printf(": failed to enable bus mastering\n");
		return;
	}

	/* Map control/status registers. */
	if (pci_mapreg_map(pa, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0, &sc->sc_memt,
	    &sc->sc_memh, &membase, &memsize, 0)) {
		printf(": can't find mem space\n");
		return;
	}
	state++;

	/* Map interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin, pa->pa_intrline,
	    &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}
	state++;

	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ises_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt\n");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}

	/* Initialize DMA map */
	sc->sc_dmat = pa->pa_dmat;
	error = bus_dmamap_create(sc->sc_dmat, 1 << PGSHIFT, 1, 1 << PGSHIFT,
	    0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->sc_dmamap_xfer);
	if (error) {
		printf(": cannot create dma map (%d)\n", error);
		goto fail;
	}
	state++;

	/* Allocate in DMAable memory. */
	if (bus_dmamem_alloc(sc->sc_dmat, sizeof sc->sc_dmamap, 1, 0, &seg, 1,
	    &nsegs, BUS_DMA_NOWAIT)) {
		printf(": can't alloc dma buffer space\n");
		goto fail;
	}
	state++;

	sc->sc_dmamap_phys = seg.ds_addr;
	if (bus_dmamem_map(sc->sc_dmat, &seg, nsegs, sizeof sc->sc_dmamap,
	    (caddr_t *)&sc->sc_dmamap, 0)) {
		printf(": can't map dma buffer space\n");
		goto fail;
	}
	state++;

	printf(": %s\n", intrstr);

	bzero(&isesstats, sizeof(isesstats));

	sc->sc_cid = crypto_get_driverid();

	if (sc->sc_cid < 0)
		goto fail;

	/*
	 * Since none of the initialization steps generate interrupts
	 * for example, the hardware reset, we use a number of timeouts
	 * (or init states) to do the rest of the chip initialization.
	 */

	sc->sc_initstate = 0;
	timeout_set(&sc->sc_timeout, ises_initstate, sc);
	ises_initstate(sc);
#ifdef ISESDEBUG
	ises_debug_init(sc);
#endif
	return;

 fail:
	switch (state) { /* Always fallthrough here. */
	case 4:
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)&sc->sc_dmamap,
		    sizeof sc->sc_dmamap);
	case 3:
		bus_dmamem_free(sc->sc_dmat, &seg, nsegs);
	case 2:
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap_xfer);
	case 1:
		pci_intr_disestablish(pc, sc->sc_ih);
	default: /* 0 */
		bus_space_unmap(sc->sc_memt, sc->sc_memh, memsize);
	}
	return;
}

void
ises_initstate(void *v)
{
	/*
	 * Step through chip initialization.
	 * sc->sc_initstate tells us what to do.
	 */
	extern int hz;
	struct ises_softc *sc = v;
	char *dv = sc->sc_dv.dv_xname;
	u_int32_t stat;
	int p, ticks;

	ticks = hz; 

#if 0 /* Too noisy */
	DPRINTF (("%s: entered initstate %d\n", dv, sc->sc_initstate));
#endif

	switch (sc->sc_initstate) {
	case 0:
		/* Power up the chip (clear powerdown bit) */
		stat = READ_REG(sc, ISES_BO_STAT);
		if (stat & ISES_BO_STAT_POWERDOWN) {
			stat &= ~ISES_BO_STAT_POWERDOWN;
			WRITE_REG(sc, ISES_BO_STAT, stat);
			/* Selftests will take 1 second. */
			break;
		}
		/* FALLTHROUGH (chip is already powered up) */
		sc->sc_initstate++;

	case 1:
		/* Perform a hardware reset */
		stat = 0;

		printf ("%s: initializing...\n", dv);

		/* Clear all possible bypass bits. */
		for (p = 0; p < 128; p++)
			WRITE_REG(sc, ISES_B_BDATAOUT, 0L);

		stat |= ISES_BO_STAT_HWRESET;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		stat &= ~ISES_BO_STAT_HWRESET;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		/* Again, selftests will take 1 second. */
		break;

	case 2:
		/* Set AConf to zero, i.e 32-bits access to A-int. */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat &= ~ISES_BO_STAT_ACONF;
		WRITE_REG(sc, ISES_BO_STAT, stat);

		/* Is the firmware already loaded? */
		if (READ_REG(sc, ISES_A_STAT) & ISES_STAT_HW_DA) {
			/* Yes it is, jump ahead a bit */
			ticks = 1;
			sc->sc_initstate += 4; /* Next step --> 7 */
			break;
		}

		/*
		 * Download the Basic Functionality firmware.
		 * Prior to downloading we need to reset the NSRAM.
		 * Setting the tamper bit will erase the contents
		 * in 1 microsecond.
		 */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat |= ISES_BO_STAT_TAMPER;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		ticks = 1;
		break;

	case 3:
		/* After tamper bit has been set, powerdown chip. */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat |= ISES_BO_STAT_POWERDOWN;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		/* Wait one second for power to dissipate. */
		break;

	case 4:
		/* Clear tamper and powerdown bits. */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat &= ~(ISES_BO_STAT_TAMPER | ISES_BO_STAT_POWERDOWN);
		WRITE_REG(sc, ISES_BO_STAT, stat);
		/* Again, wait one second for selftests. */
		break;

	case 5:
		/*
		 * We'll need some space in the input queue (IQF)
		 * and we need to be in the 'waiting for program
		 * length' IDP state (0x4).
		 */
		p = ISES_STAT_IDP_STATE(READ_REG(sc, ISES_A_STAT));
		if (READ_REG(sc, ISES_A_IQF) < 4 || p != 0x4) {
			printf("%s: cannot download firmware, "
			    "IDP state is \"%s\"\n", dv, ises_idp_state[p]);
			return;
		}

		/* Write firmware length */
		WRITE_REG(sc, ISES_A_IQD, ISES_BF_IDPLEN);

		/* Write firmware code */
		for (p = 0; p < sizeof(ises_bf_fw)/sizeof(u_int32_t); p++) {
			WRITE_REG(sc, ISES_A_IQD, ises_bf_fw[p]);
			if (READ_REG(sc, ISES_A_IQF) < 4)
				DELAY(10);
		}

		/* Write firmware CRC */
		WRITE_REG(sc, ISES_A_IQD, ISES_BF_IDPCRC);

		/* Wait 1s while chip resets and runs selftests */
		break;

	case 6:
		/* Did the download succed? */
		if (READ_REG(sc, ISES_A_STAT) & ISES_STAT_HW_DA) {
			ticks = 1;
			break;
		}

		/* We failed. We cannot do anything else. */
		printf ("%s: firmware download failed\n", dv);
		return;

	case 7:
		if (ises_assert_cmd_mode(sc) < 0)
			goto fail;

		/*
		 * Now that the basic functionality firmware should be
		 * up and running, try to get the firmware version.
		 */

		stat = ises_get_fwversion(sc);
		if (stat == 0)
			goto fail;

		printf("%s: firmware v%d.%d loaded (%d bytes)", dv,
		    stat & 0xffff, (stat >> 16) & 0xffff, ISES_BF_IDPLEN << 2);

		/* We can use firmware version 1.x & 2.x */
		switch (stat & 0xffff) {
		case 0:
			printf(" diagnostic, %s disabled\n", dv);
			goto fail;
		case 1: /* Basic Func "base" firmware */
		case 2: /* Basic Func "ipsec" firmware, no ADP code */
			break;
		default:
			printf(" unknown, %s disabled\n", dv);
			goto fail;
		}

		stat = READ_REG(sc, ISES_A_STAT);
		DPRINTF((", mode %s",
		    ises_sw_mode[ISES_STAT_SW_MODE(stat)]));

		/* Reuse the timeout for HRNG entropy collection. */
		timeout_del(&sc->sc_timeout);
		ises_hrng_init(sc);

		/* Set the interrupt mask */
		sc->sc_intrmask = ISES_STAT_BCHU_OAF | ISES_STAT_BCHU_ERR |
		    ISES_STAT_BCHU_OFHF | ISES_STAT_SW_OQSINC |
		    ISES_STAT_LNAU_BUSY_1 | ISES_STAT_LNAU_ERR_1 |
		    ISES_STAT_LNAU_BUSY_2 | ISES_STAT_LNAU_ERR_2;
#if 0
		    ISES_STAT_BCHU_ERR | ISES_STAT_BCHU_OAF |
		    ISES_STAT_BCHU_IFE | ISES_STAT_BCHU_IFHE |
		    ISES_STAT_BCHU_OFHF | ISES_STAT_BCHU_OFF;
#endif

		WRITE_REG(sc, ISES_A_INTE, sc->sc_intrmask);

		/* We're done. */
		printf("\n");

		/* Register ourselves with crypto framework. */
#ifdef notyet
		crypto_register(sc->sc_cid, CRYPTO_3DES_CBC,
		    ises_newsession, ises_freesession, ises_process);
		crypto_register(sc->sc_cid, CRYPTO_DES_CBC, NULL, NULL, NULL);
		crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC, NULL, NULL, NULL);
		crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC, NULL, NULL,
		    NULL);
		crypto_register(sc->sc_cid, CRYPTO_RIPEMD160_HMAC, NULL, NULL,
		    NULL);
#endif

		return;

	default:
		printf("%s: entered unknown initstate %d\n", dv,
		    sc->sc_initstate);
		goto fail;
	}

	/* Increment state counter and schedule next step in 'ticks' ticks. */
	sc->sc_initstate++;
	timeout_add(&sc->sc_timeout, ticks);
	return;

 fail:
	printf("%s: firmware failure\n", dv);
	timeout_del(&sc->sc_timeout);
	return;
}

/* Put a command on the A-interface queue. */
int
ises_queue_cmd(struct ises_softc *sc, u_int32_t cmd, u_int32_t *data, 
	       u_int32_t (*callback)(struct ises_softc *, struct ises_cmd *))
{
	struct ises_cmd *cq;
	int p, len, s, code;

	len = cmd >> 24;
	code = (cmd >> 16) & 0xFF;
	
	s = splimp();

	if (len > READ_REG(sc, ISES_A_IQF)) {
		splx(s);
		return (EAGAIN); /* XXX ENOMEM ? */
	}

	cq = (struct ises_cmd *) 
	    malloc(sizeof (struct ises_cmd), M_DEVBUF, M_NOWAIT);
	if (cq == NULL) {
		splx(s);
		isesstats.nomem++;
		return (ENOMEM);
	}
	bzero(cq, sizeof (struct ises_cmd));
	cq->cmd_code = code;
	cq->cmd_cb = callback;
	SIMPLEQ_INSERT_TAIL(&sc->sc_cmdq, cq, cmd_next);

	WRITE_REG(sc, ISES_A_IQD, cmd);

	/* LNAU register data should be written in reverse order */
	if ((code >= ISES_CMD_LW_A_1 && code <= ISES_CMD_LW_U_1) || /* LNAU1 */
	    (code >= ISES_CMD_LW_A_2 && code <= ISES_CMD_LW_U_2))   /* LNAU2 */
		for (p = len - 1; p >= 0; p--)
			WRITE_REG(sc, ISES_A_IQD, *(data + p));
	else
		for (p = 0; p < len; p++)
			WRITE_REG(sc, ISES_A_IQD, *(data + p));

	WRITE_REG(sc, ISES_A_IQS, 0);

	splx(s);
	return (0);
}

/* Process all completed responses in the output queue. */
void
ises_process_oqueue(struct ises_softc *sc)
{
#ifdef ISESDEBUG
	char *dv = sc->sc_dv.dv_xname;
#endif
	struct ises_cmd *cq;
	u_int32_t oqs, r, d;
	int cmd, len, c;

	r = READ_REG(sc, ISES_A_OQS);
	if (r > 1)
		DPRINTF(("%s:process_oqueue: OQS=%d\n", dv, r));

	/* OQS gives us the number of responses we have to process. */
	while ((oqs = READ_REG(sc, ISES_A_OQS)) > 0) {
		/* Read command response. [ len(8) | cmd(8) | rc(16) ] */
		r = READ_REG(sc, ISES_A_OQD);
		len = (r >> 24);
		cmd = (r >> 16) & 0xff;
		r   = r & 0xffff;

		if (!SIMPLEQ_EMPTY(&sc->sc_cmdq)) {
			cq = SIMPLEQ_FIRST(&sc->sc_cmdq);
			SIMPLEQ_REMOVE_HEAD(&sc->sc_cmdq, cq, cmd_next);
			cq->cmd_rlen = len;
		} else {
			cq = NULL;
			DPRINTF(("%s:process_oqueue: cmd queue empty!\n", dv));
		}

		if (r) {
			/* This command generated an error */
			DPRINTF(("%s:process_oqueue: cmd %d err %d\n", dv, cmd,
			    (r & ISES_RC_MASK)));
		} else {
			/* Use specified callback, if any */
			if (cq && cq->cmd_cb) {
				if (cmd == cq->cmd_code) {
					cq->cmd_cb(sc, cq);
					cmd = ISES_CMD_NONE;
				} else {
					DPRINTF(("%s:process_oqueue: expected"
					    " cmd %d, got %d\n", dv, 
					    cq->cmd_code, cmd));
					/* XXX Some error handling here? */
				}
			}

			switch (cmd) {
			case ISES_CMD_NONE:
				break;

			case ISES_CMD_HBITS:
				/* XXX How about increasing the pool size? */
				/* XXX Use add_entropy_words instead? */
				/* XXX ... at proper spl */
				/* Cmd generated by ises_rng() via timeouts */
				while (len--) {
					d = READ_REG(sc, ISES_A_OQD);
					add_true_randomness(d);
				}
				break;

			case ISES_CMD_LUPLOAD_1:
				/* Get result of LNAU 1 operation. */
				DPRINTF(("%s:process_oqueue: LNAU 1 result "
				     "upload (len=%d)\n", dv, len));
				sc->sc_lnau1_rlen = len;
				bzero(sc->sc_lnau1_r, 2048 / 8);
				while (len--) {
					/* first word is LSW */
					sc->sc_lnau1_r[len] = 
					    READ_REG(sc, ISES_A_OQD);
				}
				break;

			case ISES_CMD_LUPLOAD_2:
				/* Get result of LNAU 1 operation. */
				DPRINTF(("%s:process_oqueue: LNAU 2 result "
				     "upload (len=%d)\n", dv, len));
				sc->sc_lnau2_rlen = len;
				bzero(sc->sc_lnau1_r, 2048 / 8);
				while (len--) {
					/* first word is LSW */
					sc->sc_lnau2_r[len] = 
					    READ_REG(sc, ISES_A_OQD);
				}
				break;

			case ISES_CMD_BR_OMR:
				sc->sc_bsession.omr = READ_REG(sc, ISES_A_OQD);
				DPRINTF(("%s:process_oqueue: read OMR[%08x]\n",
				    dv, sc->sc_bsession.omr));
#ifdef ISESDEBUG
				ises_debug_parse_omr(sc);
#endif
				break;

			case ISES_CMD_BSWITCH:
				DPRINTF(("%s:process_oqueue: BCHU_SWITCH\n"));
				/* Put switched BCHU session in sc_bsession. */
				for(c = 0; len > 0; len--, c++)
					*((u_int32_t *)&sc->sc_bsession + c) =
					    READ_REG(sc, ISES_A_OQD);
				break;

			default:
				/* All other are ok (no response data) */
				DPRINTF(("%s:process_oqueue [cmd %d len %d]\n",
				    dv, cmd, len));
				if (cq && cq->cmd_cb) 
					len -= cq->cmd_cb(sc, cq);
			}
		}

		if (cq)
			free(cq, M_DEVBUF);
		
		/* This will drain any remaining data and ACK this reponse. */
		while (len-- > 0)
			d = READ_REG(sc, ISES_A_OQD);
		WRITE_REG(sc, ISES_A_OQS, 0);
		if (oqs > 1)
			DELAY(1); /* Wait for fw to decrement OQS (8 clocks) */
	}
}

int
ises_intr(void *arg)
{
	struct ises_softc *sc = arg;
	volatile u_int32_t ints;
	u_int32_t cmd;
#ifdef ISESDEBUG
	char *dv = sc->sc_dv.dv_xname;
#endif

	ints = READ_REG(sc, ISES_A_INTS);
	if (!(ints & sc->sc_intrmask))
		return (0); /* Not our interrupt. */

	WRITE_REG(sc, ISES_A_INTS, ints); /* Clear all set intr bits. */

#if 0
	/* Check it we've got room for more data. */
	if (READ_REG(sc, ISES_A_STAT) &
	    (ISES_STAT_BCHU_IFE | ISES_STAT_BCHU_IFHE))
		ises_feed(sc);
#endif

	if (ints & ISES_STAT_SW_OQSINC) {	/* A-intf output q has data */
		ises_process_oqueue(sc);
	}

	if (ints & ISES_STAT_LNAU_BUSY_1) {
		DPRINTF(("%s:ises_intr: LNAU 1 job complete\n", dv));
		/* upload LNAU 1 result (into sc->sc_lnau1_r) */
		cmd = ISES_MKCMD(ISES_CMD_LUPLOAD_1, 0);
		ises_queue_cmd(sc, cmd, NULL, NULL);
	}

	if (ints & ISES_STAT_LNAU_BUSY_2) {
		DPRINTF(("%s:ises_intr: LNAU 2 job complete\n", dv));
		/* upload LNAU 2 result (into sc->sc_lnau2_r) */
		cmd = ISES_MKCMD(ISES_CMD_LUPLOAD_2, 0);
		ises_queue_cmd(sc, cmd, NULL, NULL);
	}

	if (ints & ISES_STAT_LNAU_ERR_1) {
		DPRINTF(("%s:ises_intr: LNAU 1 error\n", dv));
		sc->sc_lnau1_rlen = -1;
	}

	if (ints & ISES_STAT_LNAU_ERR_2) {
		DPRINTF(("%s:ises_intr: LNAU 2 error\n", dv));
		sc->sc_lnau2_rlen = -1;
	}

	if (ints & ISES_STAT_BCHU_OAF) {	/* output data available */
		DPRINTF(("%s:ises_intr: BCHU_OAF bit set\n", dv));
		/* ises_process_oqueue(sc); */
	}

	if (ints & ISES_STAT_BCHU_ERR) {	/* We got a BCHU error */
		DPRINTF(("%s:ises_intr: BCHU error\n", dv));
		/* XXX Error handling */
	}

	if (ints & ISES_STAT_BCHU_OFHF) {	/* Output is half full */
		DPRINTF(("%s:ises_intr: BCHU output FIFO half full\n", dv));
		/* XXX drain data? */
	}

#if 0 /* XXX Useful? */
	if (ints & ISES_STAT_BCHU_OFF) {	/* Output is full */
		/* XXX drain data / error handling? */
	}
#endif
	return (1);
}

int
ises_feed(struct ises_softc *sc)
{
	struct ises_q *q;

	while (!SIMPLEQ_EMPTY(&sc->sc_queue)) {
		if (READ_REG(sc, ISES_A_STAT) & ISES_STAT_BCHU_IFF)
			break;
		q = SIMPLEQ_FIRST(&sc->sc_queue);
#if 0
		WRITE_REG(sc, ISES_OFFSET_BCHU_DATA,
		    (u_int32_t)vtophys(&q->q_mcr));
		printf("feed: q->chip %08x %08x\n", q,
		    (u_int32_t)vtophys(&q->q_mcr));
#endif
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q, q_next);
		--sc->sc_nqueue;
		SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
	}
	return (0);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
int
ises_newsession(u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *c, *mac = NULL, *enc = NULL;
	struct ises_softc *sc = NULL;
	struct ises_session *ses;
	MD5_CTX	   md5ctx;
	SHA1_CTX   sha1ctx;
	RMD160_CTX rmd160ctx;
	int i, sesn;

	if (sidp == NULL || cri == NULL)
		return (EINVAL);

	for (i = 0; i < ises_cd.cd_ndevs; i++) {
		sc = ises_cd.cd_devs[i];
		if (sc == NULL || sc->sc_cid == (*sidp))
			break;
	}
	if (sc == NULL)
		return (EINVAL);

	for (c = cri; c != NULL; c = c->cri_next) {
		if (c->cri_alg == CRYPTO_MD5_HMAC ||
		    c->cri_alg == CRYPTO_SHA1_HMAC ||
		    c->cri_alg == CRYPTO_RIPEMD160_HMAC) {
			if (mac)
				return (EINVAL);
			mac = c;
		} else if (c->cri_alg == CRYPTO_DES_CBC ||
		    c->cri_alg == CRYPTO_3DES_CBC) {
			if (enc)
				return (EINVAL);
			enc = c;
		} else
			return (EINVAL);
	}
	if (mac == 0 && enc == 0)
		return (EINVAL);

	/* Allocate a new session */
	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct ises_session *)
		    malloc(sizeof(struct ises_session), M_DEVBUF, M_NOWAIT);
		if (ses == NULL) {
			isesstats.nomem++;
			return (ENOMEM);
		}
		sesn = 0;
		sc->sc_nsessions = 1;
	} else {
		ses = NULL;
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++)
			if (sc->sc_sessions[sesn].ses_used == 0) {
				ses = &sc->sc_sessions[sesn];
				break;
			}

		if (ses == NULL) {
			i = sc->sc_nsessions * sizeof(struct ises_session);
			ses = (struct ises_session *)
			    malloc(i + sizeof(struct ises_session), M_DEVBUF,
			    M_NOWAIT);
			if (ses == NULL) {
				isesstats.nomem++;
				return (ENOMEM);
			}

			bcopy(sc->sc_sessions, ses, i);
			bzero(sc->sc_sessions, i);
			free(sc->sc_sessions, M_DEVBUF);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sc->sc_nsessions];
			sc->sc_nsessions++;
		}
	}

	bzero(ses, sizeof(struct ises_session));
	ses->ses_used = 1;

	if (enc) {
		/* get an IV, network byte order */
		/* XXX switch to using builtin HRNG ! */
		get_random_bytes(ses->ses_iv, sizeof(ses->ses_iv));

		/* crypto key */
		if (c->cri_alg == CRYPTO_DES_CBC) {
			bcopy(enc->cri_key, &ses->ses_deskey[0], 8);
			bcopy(enc->cri_key, &ses->ses_deskey[2], 8);
			bcopy(enc->cri_key, &ses->ses_deskey[4], 8);
		} else
			bcopy(enc->cri_key, &ses->ses_deskey[0], 24);

		SWAP32(ses->ses_deskey[0]);
		SWAP32(ses->ses_deskey[1]);
		SWAP32(ses->ses_deskey[2]);
		SWAP32(ses->ses_deskey[3]);
		SWAP32(ses->ses_deskey[4]);
		SWAP32(ses->ses_deskey[5]);
	}

	if (mac) {
		for (i = 0; i < mac->cri_klen / 8; i++)
			mac->cri_key[i] ^= HMAC_IPAD_VAL;

		switch (mac->cri_alg) {
		case CRYPTO_MD5_HMAC:
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, mac->cri_key, mac->cri_klen / 8);
			MD5Update(&md5ctx, hmac_ipad_buffer, HMAC_BLOCK_LEN -
			    (mac->cri_klen / 8));
			bcopy(md5ctx.state, ses->ses_hminner,
			    sizeof(md5ctx.state));
			break;
		case CRYPTO_SHA1_HMAC:
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, mac->cri_key, mac->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_ipad_buffer, HMAC_BLOCK_LEN -
			    (mac->cri_klen / 8));
			bcopy(sha1ctx.state, ses->ses_hminner,
			    sizeof(sha1ctx.state));
			break;
		case CRYPTO_RIPEMD160_HMAC:
		default:
			RMD160Init(&rmd160ctx);
			RMD160Update(&rmd160ctx, mac->cri_key,
			    mac->cri_klen / 8);
			RMD160Update(&rmd160ctx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (mac->cri_klen / 8));
			bcopy(rmd160ctx.state, ses->ses_hminner,
			    sizeof(rmd160ctx.state));
			break;
		}

		for (i = 0; i < mac->cri_klen / 8; i++)
			mac->cri_key[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

		switch (mac->cri_alg) {
		case CRYPTO_MD5_HMAC:
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, mac->cri_key, mac->cri_klen / 8);
			MD5Update(&md5ctx, hmac_ipad_buffer, HMAC_BLOCK_LEN -
			    (mac->cri_klen / 8));
			bcopy(md5ctx.state, ses->ses_hmouter,
			    sizeof(md5ctx.state));
			break;
		case CRYPTO_SHA1_HMAC:
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, mac->cri_key, mac->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_ipad_buffer, HMAC_BLOCK_LEN -
			    (mac->cri_klen / 8));
			bcopy(sha1ctx.state, ses->ses_hmouter,
			    sizeof(sha1ctx.state));
			break;
		case CRYPTO_RIPEMD160_HMAC:
		default:
			RMD160Init(&rmd160ctx);
			RMD160Update(&rmd160ctx, mac->cri_key,
			    mac->cri_klen / 8);
			RMD160Update(&rmd160ctx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (mac->cri_klen / 8));
			bcopy(rmd160ctx.state, ses->ses_hmouter,
			    sizeof(rmd160ctx.state));
			break;
		}

		for (i = 0; i < mac->cri_klen / 8; i++)
			mac->cri_key[i] ^= HMAC_OPAD_VAL;
	}

	*sidp = ISES_SID(sc->sc_dv.dv_unit, sesn);
	return (0);
}

/*
 * Deallocate a session.
 */
int
ises_freesession(u_int64_t tsid)
{
	struct ises_softc *sc;
	int card, sesn;
	u_int32_t sid = ((u_int32_t) tsid) & 0xffffffff;

	card = ISES_CARD(sid);
	if (card >= ises_cd.cd_ndevs || ises_cd.cd_devs[card] == NULL)
		return (EINVAL);

	sc = ises_cd.cd_devs[card];
	sesn = ISES_SESSION(sid);
	bzero(&sc->sc_sessions[sesn], sizeof(sc->sc_sessions[sesn]));

	return (0);
}

int
ises_process(struct cryptop *crp)
{
	int card, err;
	struct ises_softc *sc;
	struct ises_q *q;
	struct cryptodesc *maccrd, *enccrd, *crd;
	struct ises_session *ses;
#if 0
	int s, i, j;
#else
	int s;
#endif
	int encoffset = 0, macoffset = 0;
	int sskip, stheend, dtheend, cpskip, cpoffset, dskip, nicealign;
	int16_t coffset;

	if (crp == NULL || crp->crp_callback == NULL)
		return (EINVAL);

	card = ISES_CARD(crp->crp_sid);
	if (card >= ises_cd.cd_ndevs || ises_cd.cd_devs[card] == NULL) {
		err = EINVAL;
		goto errout;
	}

	sc = ises_cd.cd_devs[card];

	s = splnet();
	if (sc->sc_nqueue == ISES_MAX_NQUEUE) {
		splx(s);
		err = ENOMEM;
		goto errout;
	}
	splx(s);

	q = (struct ises_q *)malloc(sizeof(struct ises_q), M_DEVBUF, M_NOWAIT);
	if (q == NULL) {
		err = ENOMEM;
		goto errout;
	}
	bzero(q, sizeof(struct ises_q));

	q->q_sesn = ISES_SESSION(crp->crp_sid);
	ses = &sc->sc_sessions[q->q_sesn];

	/* XXX */

	q->q_sc = sc;
	q->q_crp = crp;

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		q->q_src_m = (struct mbuf *)crp->crp_buf;
		q->q_dst_m = (struct mbuf *)crp->crp_buf;
	} else {
		err = EINVAL;
		goto errout;
	}

	/*
	 * Check if the crypto descriptors are sane. We accept:
	 * - just one crd; either auth or crypto
	 * - two crds; must be one auth and one crypto, although now
	 *   for encryption we only want the first to be crypto, while
	 *   for decryption the second one should be crypto.
	 */
	maccrd = enccrd = NULL;
	err = EINVAL;
	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		switch (crd->crd_alg) {
		case CRYPTO_MD5_HMAC:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_RIPEMD160_HMAC:
			if (maccrd || (enccrd &&
			    (enccrd->crd_flags & CRD_F_ENCRYPT) == 0))
				goto errout;
			maccrd = crd;
			break;
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
			if (enccrd ||
			    (maccrd && (crd->crd_flags & CRD_F_ENCRYPT)))
				goto errout;
			enccrd = crd;
			break;
		default:
			goto errout;
		}
	}
	if (!maccrd && !enccrd)
		goto errout;
	err = 0;

	if (enccrd) {
		encoffset = enccrd->crd_skip;

		if (enccrd->crd_alg == CRYPTO_3DES_CBC)
			q->q_bsession.omr |= ISES_SOMR_BOMR_3DES;
		else
			q->q_bsession.omr |= ISES_SOMR_BOMR_DES;
		q->q_bsession.omr |= ISES_SOMR_FMR_CBC;

		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			q->q_bsession.omr |= ISES_SOMR_EDR; /* XXX */

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, q->q_bsession.sccr, 8);
			else {
				q->q_bsession.sccr[0] = ses->ses_iv[0];
				q->q_bsession.sccr[1] = ses->ses_iv[1];
			}

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0)
				m_copyback(q->q_src_m, enccrd->crd_inject,
				    8, (caddr_t)q->q_bsession.sccr);
		} else {
			q->q_bsession.omr &= ~ISES_SOMR_EDR; /* XXX */

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, q->q_bsession.sccr, 8);
			else
				m_copyback(q->q_src_m, enccrd->crd_inject,
				    8, (caddr_t)q->q_bsession.sccr);
		}

		q->q_bsession.kr[0] = ses->ses_deskey[0];
		q->q_bsession.kr[1] = ses->ses_deskey[1];
		q->q_bsession.kr[2] = ses->ses_deskey[2];
		q->q_bsession.kr[3] = ses->ses_deskey[3];
		q->q_bsession.kr[4] = ses->ses_deskey[4];
		q->q_bsession.kr[5] = ses->ses_deskey[5];

		SWAP32(q->q_bsession.sccr[0]);
		SWAP32(q->q_bsession.sccr[1]);
	}

	if (maccrd) {
		macoffset = maccrd->crd_skip;

		switch (crd->crd_alg) {
		case CRYPTO_MD5_HMAC:
			q->q_bsession.omr |= ISES_HOMR_HFR_MD5;
			break;
		case CRYPTO_SHA1_HMAC:
			q->q_bsession.omr |= ISES_HOMR_HFR_SHA1;
			break;
		case CRYPTO_RIPEMD160_HMAC:
		default:
			q->q_bsession.omr |= ISES_HOMR_HFR_RMD160;
			break;
		}

		q->q_hminner[0] = ses->ses_hminner[0];
		q->q_hminner[1] = ses->ses_hminner[1];
		q->q_hminner[2] = ses->ses_hminner[2];
		q->q_hminner[3] = ses->ses_hminner[3];
		q->q_hminner[4] = ses->ses_hminner[4];
		q->q_hminner[5] = ses->ses_hminner[5];

		q->q_hmouter[0] = ses->ses_hmouter[0];
		q->q_hmouter[1] = ses->ses_hmouter[1];
		q->q_hmouter[2] = ses->ses_hmouter[2];
		q->q_hmouter[3] = ses->ses_hmouter[3];
		q->q_hmouter[4] = ses->ses_hmouter[4];
		q->q_hmouter[5] = ses->ses_hmouter[5];
	}

	if (enccrd && maccrd) {
		/* XXX Check if ises handles differing end of auth/enc etc */
		/* XXX For now, assume not (same as ubsec). */
		if (((encoffset + enccrd->crd_len) !=
		    (macoffset + maccrd->crd_len)) ||
		    (enccrd->crd_skip < maccrd->crd_skip)) {
			err = EINVAL;
			goto errout;
		}

		sskip = maccrd->crd_skip;
		cpskip = dskip = enccrd->crd_skip;
		stheend = maccrd->crd_len;
		dtheend = enccrd->crd_len;
		coffset = cpskip - sskip;
		cpoffset = cpskip + dtheend;
		/* XXX DEBUG ? */
	} else {
		cpskip = dskip = sskip = macoffset + encoffset;
		dtheend = enccrd ? enccrd->crd_len : maccrd->crd_len;
		stheend = dtheend;
		cpoffset = cpskip + dtheend;
		coffset = 0;
	}
	q->q_offset = coffset >> 2;

	q->q_src_l = mbuf2pages(q->q_src_m, &q->q_src_npa, &q->q_src_packp,
	    &q->q_src_packl, 1, &nicealign);
	if (q->q_src_l == 0) {
		err = ENOMEM;
		goto errout;
	}

	/* XXX mcr stuff; q->q_mcr->mcr_pktlen = stheend; */

#if 0 /* XXX */
	for (i = j = 0; i < q->q_src_npa; i++) {
		struct ises_pktbuf *pb;

		/* XXX DEBUG? */

		if (sskip) {
			if (sskip >= q->q_src_packl) {
				sskip -= q->q_src_packl;
				continue;
			}
			q->q_src_packp += sskip;
			q->q_src_packl -= sskip;
			sskip = 0;
		}

		pb = NULL; /* XXX initial packet */

		pb->pb_addr = q->q_src_packp;
		if (stheend) {
			if (q->q_src_packl > stheend) {
				pb->pb_len = stheend;
				stheend = 0;
			} else {
				pb->pb_len = q->q_src_packl;
				stheend -= pb->pb_len;
			}
		} else
			pb->pb_len = q->q_src_packl;

		if ((i + 1) == q->q_src_npa)
			pb->pb_next = 0;
		else
			pb->pb_next = vtophys(&q->q_srcpkt);

		j++;
	}

#endif /* XXX */
	/* XXX DEBUG ? */

	if (enccrd == NULL && maccrd != NULL) {
		/* XXX mcr stuff */
	} else {
		if (!nicealign) {
			int totlen, len;
			struct mbuf *m, *top, **mp;

			totlen = q->q_dst_l = q->q_src_l;
			if (q->q_src_m->m_flags & M_PKTHDR) {
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				M_DUP_PKTHDR(m, q->q_src_m);
				len = MHLEN;
			} else {
				MGET(m, M_DONTWAIT, MT_DATA);
				len = MLEN;
			}
			if (m == NULL) {
				err = ENOMEM;
				goto errout;
			}
			if (totlen >= MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if (m->m_flags & M_EXT)
					len = MCLBYTES;
			}
			m->m_len = len;
			top = NULL;
			mp = &top;

			while (totlen > 0) {
				if (top) {
					MGET(m, M_DONTWAIT, MT_DATA);
					if (m == NULL) {
						m_freem(top);
						err = ENOMEM;
						goto errout;
					}
					len = MLEN;
				}
				if (top && totlen >= MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if (m->m_flags & M_EXT)
						len = MCLBYTES;
				}
				m->m_len = len = min(totlen, len);
				totlen -= len;
				*mp = m;

				mp = &m->m_next;
			}
			q->q_dst_m = top;
			ubsec_mcopy(q->q_src_m, q->q_dst_m, cpskip, cpoffset);
		} else
			q->q_dst_m = q->q_src_m;

		q->q_dst_l = mbuf2pages(q->q_dst_m, &q->q_dst_npa,
		    &q->q_dst_packp, &q->q_dst_packl, 1, NULL);

#if 0
		for (i = j = 0; i < q->q_dst_npa; i++) {
			struct ises_pktbuf *pb;

			if (dskip) {
				if (dskip >= q->q_dst_packl[i]) {
					dskip -= q->q_dst_packl[i];
					continue;
				}
				q->q_dst_packp[i] += dskip;
				q->q_dst_packl[i] -= dskip;
				dskip = 0;
			}

			if (j == 0)
				pb = NULL; /* &q->q_mcr->mcr_opktbuf; */
			else
				pb = &q->q_dstpkt[j - 1];

			pb->pb_addr = q->q_dst_packp[i];

			if (dtheend) {
				if (q->q_dst_packl[i] > dtheend) {
					pb->pb_len = dtheend;
					dtheend = 0;
				} else {
					pb->pb_len = q->q_dst_packl[i];
					dtheend -= pb->pb_len;
				}
			} else
				pb->pb_len = q->q_dst_packl[i];

			if ((i + 1) == q->q_dst_npa) {
				if (maccrd)
					pb->pb_next = vtophys(q->q_macbuf);
				else
					pb->pb_next = 0;
			} else
				pb->pb_next = vtophys(&q->q_dstpkt[j]);
			j++;
		}
#endif
	}

	s = splnet();
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue, q, q_next);
	sc->sc_nqueue++;
	ises_feed(sc);
	splx(s);

	return (0);

errout:
	if (err == ENOMEM)
		isesstats.nomem++;
	else if (err == EINVAL)
		isesstats.invalid++;

	if (q) {
		if (q->q_src_m != q->q_dst_m)
			m_freem(q->q_dst_m);
		free(q, M_DEVBUF);
	}
	crp->crp_etype = err;
	crp->crp_callback(crp);
	return (0);
}

void
ises_callback(struct ises_q *q)
{
	struct cryptop *crp = (struct cryptop *)q->q_crp;
	struct cryptodesc *crd;

	if ((crp->crp_flags & CRYPTO_F_IMBUF) && (q->q_src_m != q->q_dst_m)) {
		m_freem(q->q_src_m);
		crp->crp_buf = (caddr_t)q->q_dst_m;
	}

	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		if (crd->crd_alg != CRYPTO_MD5_HMAC &&
		    crd->crd_alg != CRYPTO_SHA1_HMAC &&
		    crd->crd_alg != CRYPTO_RIPEMD160_HMAC)
			continue;
		m_copyback((struct mbuf *)crp->crp_buf,
		    crd->crd_inject, 12, (u_int8_t *)&q->q_macbuf[0]);
		break;
	}

	free(q, M_DEVBUF);
	crypto_done(crp);
}

/* Initilize the ISES hardware RNG, and set up timeouts. */
void
ises_hrng_init(struct ises_softc *sc)
{
	u_int32_t cmd, r;
	int i;
#ifdef ISESDEBUG
	struct timeval tv1, tv2;
#endif

	/* Asking for random data will seed LFSR and start the RBG */
	cmd = ISES_MKCMD(ISES_CMD_HBITS, 1);
	r   = 8; /* 8 * 32 = 256 bits */

	if (ises_queue_cmd(sc, cmd, &r, NULL))
		return;

	/* Wait until response arrives. */
	for (i = 1000; i && READ_REG(sc, ISES_A_OQS) == 0; i--)
		DELAY(10);

	if (!READ_REG(sc, ISES_A_OQS))
		return;

	/* Drain cmd response and 8*32 bits data */
	for (i = 0; i <= r; i++)
		(void)READ_REG(sc, ISES_A_OQD);

	/* ACK the response */
	WRITE_REG(sc, ISES_A_OQS, 0);
	DELAY(1);
	printf(", rng active", sc->sc_dv.dv_xname);

#ifdef ISESDEBUG
	/* Benchmark the HRNG. */

	/*
	 * XXX These values gets surprisingly large. Docs state the
	 * HNRG produces > 1 mbit/s of random data. The values I'm seeing
	 * are much higher, ca 2.7-2.8 mbit/s. AFAICT the algorithm is sound.
	 * Compiler optimization issues, perhaps?
	 */

#define ISES_WPR 250
#define ISES_ROUNDS 100
	cmd = ISES_MKCMD(ISES_CMD_HBITS, 1);
	r = ISES_WPR;

	/* Queue 100 cmds; each generate 250 32-bit words of rnd data. */
	microtime(&tv1);
	for (i = 0; i < ISES_ROUNDS; i++)
		ises_queue_cmd(sc, cmd, &r, NULL);
	for (i = 0; i < ISES_ROUNDS; i++) {
		while (READ_REG(sc, ISES_A_OQS) == 0) ; /* Wait for response */

		(void)READ_REG(sc, ISES_A_OQD);		/* read response */
		for (r = ISES_WPR; r--;)
			(void)READ_REG(sc, ISES_A_OQD);	/* read data */
		WRITE_REG(sc, ISES_A_OQS, 0);		/* ACK resp */
		DELAY(1); /* OQS needs 1us to decrement */
	}
	microtime(&tv2);

	timersub(&tv2, &tv1, &tv1);
	tv1.tv_usec += 1000000 * tv1.tv_sec;
	printf(", %dKb/sec",
	    ISES_WPR * ISES_ROUNDS * 32 / 1024 * 1000000 / tv1.tv_usec);
#endif

	timeout_set(&sc->sc_timeout, ises_hrng, sc);
	ises_hrng(sc); /* Call first update */
}

/* Called by timeout (and once by ises_init_hrng()). */
void
ises_hrng(void *v)
{
	/*
	 * Throw a HRNG read random bits command on the command queue.
	 * The normal loop will manage the result and add it to the pool.
	 */
	struct ises_softc *sc = v;
	u_int32_t cmd, n;
	extern int hz; /* from param.c */

	timeout_add(&sc->sc_timeout, hz / ISESRNGIPS);

	if (ises_assert_cmd_mode(sc) != 0)
		return;

	cmd = ISES_MKCMD(ISES_CMD_HBITS, 1);
	n   = (ISESRNGBITS >> 5) & 0xff; /* ask for N 32 bit words */

	ises_queue_cmd(sc, cmd, &n, NULL);
}

u_int32_t
ises_get_fwversion(struct ises_softc *sc)
{
	u_int32_t r;
	int i;

	r = ISES_MKCMD(ISES_CMD_CHIP_ID, 0);
	WRITE_REG(sc, ISES_A_IQD, r);
	WRITE_REG(sc, ISES_A_IQS, 0);

	for (i = 100; i > 0 && READ_REG(sc, ISES_A_OQS) == 0; i--)
		DELAY(1);

	if (i < 1)
		return (0); /* No response */

	r = READ_REG(sc, ISES_A_OQD);

	/* Check validity. On error drain reponse data. */
	if (((r >> 16) & 0xff) != ISES_CMD_CHIP_ID ||
	    ((r >> 24) & 0xff) != 3 || (r & ISES_RC_MASK) != ISES_RC_SUCCESS) {
		if ((r & ISES_RC_MASK) == ISES_RC_SUCCESS)
			for (i = ((r >> 24) & 0xff); i; i--)
				(void)READ_REG(sc, ISES_A_OQD);
		r = 0;
		goto out;
	}

	r = READ_REG(sc, ISES_A_OQD); /* read version */
	(void)READ_REG(sc, ISES_A_OQD); /* Discard 64bit "chip-id" */
	(void)READ_REG(sc, ISES_A_OQD);
 out:
	WRITE_REG(sc, ISES_A_OQS, 0); /* Ack the response */
	DELAY(1);
	return (r);
}

/*
 * ises_assert_cmd_mode() returns
 *   -1 for failure to go to cmd
 *    0 if mode already was cmd
 *   >0 if mode was other (WFC/WFR) but now is cmd (this has reset the queues)
 */
int
ises_assert_cmd_mode(struct ises_softc *sc)
{
	switch (ISES_STAT_SW_MODE(READ_REG(sc, ISES_A_STAT))) {
	case 0x0: /* Selftest. XXX This is a transient state. */
		DELAY(1000000);
		if (ISES_STAT_SW_MODE(READ_REG(sc, ISES_A_STAT)) == 0)
			return (-1);
		return (ises_assert_cmd_mode(sc));
	case 0x1: /* Command mode */
		return (0);
	case 0x2: /* Waiting For Continue / WFC */
		bus_space_write_2(sc->sc_memt, sc->sc_memh, ISES_A_CTRL,
		    ISES_A_CTRL_CONTINUE);
		DELAY(1);
		return ((ISES_STAT_SW_MODE(READ_REG(sc, ISES_A_STAT)) == 0) ?
		    1 : -1);
	case 0x3: /* Waiting For Reset / WFR */
		bus_space_write_2(sc->sc_memt, sc->sc_memh, ISES_A_CTRL,
		    ISES_A_CTRL_RESET);
		DELAY(1000000);
		return ((ISES_STAT_SW_MODE(READ_REG(sc, ISES_A_STAT)) == 0) ?
		    2 : -1);
	default:
		return (-1); /* Unknown mode */
	}
}

#ifdef ISESDEBUG
/*
 * Development code section below here.
 */

void
ises_debug_init (struct ises_softc *sc)
{
	ises_sc = sc;
	ises_db = 0;
	timeout_set (&ises_db_timeout, ises_debug_loop, sc);
	timeout_add (&ises_db_timeout, 100);
	printf ("ises0: ISESDEBUG active (ises_sc = %p)\n", ises_sc);
}

void
ises_debug_2 (void)
{
	timeout_set (&ises_db_timeout, ises_debug_loop, ises_sc);
	timeout_add (&ises_db_timeout, 100);
	printf ("ises0: another debug timeout scheduled!\n");
}

void
ises_debug_simple_cmd (struct ises_softc *sc, u_int32_t code, u_int32_t d)
{
	u_int32_t cmd, data;
	
	cmd = ISES_MKCMD(code, (d ? 1 : 0));
	data = d;
	ises_queue_cmd(sc, cmd, &d, NULL);
}

void
ises_bchu_switch_session (struct ises_softc *sc, struct ises_bchu_session *ss)
{
	/* It appears that the BCHU_SWITCH_SESSION command is broken. */
	/* We have to work around it. */
	
	u_int32_t cmd;

	cmd = ISES_MKCMD(ISES_CMD_BR_KR0, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
	cmd = ISES_MKCMD(ISES_CMD_BR_KR1, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
	cmd = ISES_MKCMD(ISES_CMD_BR_KR2, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
	
	cmd = ISES_MKCMD(ISES_CMD_BR_OMR, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
	cmd = ISES_MKCMD(ISES_CMD_BR_SCCR, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
	cmd = ISES_MKCMD(ISES_CMD_BR_DBCR, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
	cmd = ISES_MKCMD(ISES_CMD_BR_HMLR, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
	cmd = ISES_MKCMD(ISES_CMD_BR_CVR, 0); 
	ises_queue_cmd(sc, cmd, NULL, NULL);
}

void
ises_debug_loop (void *v)
{
	struct ises_softc *sc = (struct ises_softc *)v;
	struct ises_bchu_session bses;
	u_int32_t cmd, stat;
	int i;

	if (ises_db)
		printf ("ises0: ises_db = %d  sc = %p\n", ises_db, sc);

	timeout_add (&ises_db_timeout, 300); /* Every 3 secs */

	stat = READ_REG(sc, ISES_A_OQS);
	cmd  = READ_REG(sc, ISES_A_IQS);
	if (stat || cmd)
		printf ("ises0: IQS=%d OQS=%d / IQF=%d OQF=%d\n",
		    cmd, stat, READ_REG(sc, ISES_A_IQF),
		    READ_REG(sc, ISES_A_OQF));
	
	switch (ises_db) {
	default: 
		/* 0 - do nothing (just loop) */
		break;
	case 1:
		/* Just dump register info */
		ises_showreg();
		break;
	case 2:
		/* Reset LNAU 1 registers */
		ises_debug_simple_cmd(sc, ISES_CMD_LRESET_1, 0);
		
		/* Compute R = (141 * 5623) % 117 (R should be 51 (0x33)) */
		ises_debug_simple_cmd(sc, ISES_CMD_LW_A_1, 141);
		ises_debug_simple_cmd(sc, ISES_CMD_LW_B_1, 5623);
		ises_debug_simple_cmd(sc, ISES_CMD_LW_N_1, 117);
		
		/* Launch LNAU operation. */
		ises_debug_simple_cmd(sc, ISES_CMD_LMULMOD_1, 0);
		break;
	case 3:
		/* Read result LNAU_1 R register (should not be necessary) */
		ises_debug_simple_cmd(sc, ISES_CMD_LUPLOAD_1, 0);
		break;
	case 4:
		/* Print result */
		printf ("LNAU_1 R length = %d\n", sc->sc_lnau1_rlen);
		for (i = 0; i < sc->sc_lnau1_rlen; i++)
			printf ("W%02d-[%08x]-(%u)\t%s", i, sc->sc_lnau1_r[i],
			    sc->sc_lnau1_r[i], (i%4)==3 ? "\n" : "");
		printf ("%s", (i%4) ? "\n" : "");
		break;
	case 5:
		/* Crypto. */

		/* Load BCHU session data */
		bzero(&bses, sizeof bses);
		bses.kr[0] = 0xD0;
		bses.kr[1] = 0xD1;
		bses.kr[2] = 0xD2;
		bses.kr[3] = 0xD3;
		bses.kr[4] = 0xD4;
		bses.kr[5] = 0xD5;

		/* cipher data out is hash in, SHA1, 3DES, encrypt, ECB */
		bses.omr = ISES_SELR_BCHU_HISOF | ISES_HOMR_HFR_SHA1 |
		    ISES_SOMR_BOMR_3DES | ISES_SOMR_EDR | ISES_SOMR_FMR_ECB;

		printf ("Queueing OMR write\n");
		cmd = ISES_MKCMD(ISES_CMD_BW_OMR, 1);
		ises_queue_cmd(sc, cmd, &bses.omr, NULL);

		printf ("Queueing KR0, KR1, KR2 writes\n");
		cmd = ISES_MKCMD(ISES_CMD_BW_KR0, 2);
		ises_queue_cmd(sc, cmd, &bses.kr[4], NULL);
		cmd = ISES_MKCMD(ISES_CMD_BW_KR1, 2);
		ises_queue_cmd(sc, cmd, &bses.kr[2], NULL);
		cmd = ISES_MKCMD(ISES_CMD_BW_KR2, 2);
		ises_queue_cmd(sc, cmd, &bses.kr[0], NULL);

#if 0 /* switch session does not appear to work - it never returns */
		printf ("Queueing BCHU session switch\n");
		cmd = ISES_MKCMD(ISES_CMD_BSWITCH, sizeof bses / 4);
		printf ("session is %d 32bit words (== 18 ?), cmd = [%08x]\n", 
			sizeof bses / 4, cmd);
		ises_queue_cmd(sc, cmd, (u_int32_t *)&bses, NULL);
#endif
		
		break;
	case 96:
		printf ("Stopping HRNG data collection\n");
		timeout_del(&sc->sc_timeout);
		break;
	case 97:
		printf ("Restarting HRNG data collection\n");
		if (!timeout_pending(&sc->sc_timeout))
			timeout_add(&sc->sc_timeout, hz);
		break;
	case 98:
		printf ("Resetting (wait >1s before cont.)\n");
		stat = ISES_BO_STAT_HWRESET;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		stat &= ~ISES_BO_STAT_HWRESET;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		break;
	case 99:
		printf ("Resetting everything!\n");
		if (timeout_pending(&sc->sc_timeout))
			timeout_del(&sc->sc_timeout);
		timeout_set(&sc->sc_timeout, ises_initstate, sc);
		sc->sc_initstate = 0;
		ises_initstate(sc);
		break;
	}
	
	ises_db = 0; 
}

void
ises_showreg (void)
{
	struct ises_softc *sc = ises_sc;
	u_int32_t stat, cmd;
	
	/* Board register */
	
	printf ("Board register: ");
	stat = READ_REG(sc, ISES_BO_STAT);
	
	if (stat & ISES_BO_STAT_LOOP)
		printf ("LoopMode ");
	if (stat & ISES_BO_STAT_TAMPER)
		printf ("Tamper ");
	if (stat & ISES_BO_STAT_POWERDOWN)
		printf ("PowerDown ");
	if (stat & ISES_BO_STAT_ACONF)
		printf ("16bitA-IF ");
	if (stat & ISES_BO_STAT_HWRESET)
		printf ("HWReset");
	if (stat & ISES_BO_STAT_AIRQ)
		printf ("A-IFintr");
	printf("\n");
	
	/* A interface */
	
	printf ("A Interface STAT register: \n\tLNAU-[");
	stat = READ_REG(sc, ISES_A_STAT);
	if (stat & ISES_STAT_LNAU_MASKED)
		printf ("masked");
	else {
		if (stat & ISES_STAT_LNAU_BUSY_1)
			printf ("busy1 ");
		if (stat & ISES_STAT_LNAU_ERR_1)
			printf ("err1 ");
		if (stat & ISES_STAT_LNAU_BUSY_2)
			printf ("busy2 ");
		if (stat & ISES_STAT_LNAU_ERR_2)
			printf ("err2 ");
	}
	printf ("]\n\tBCHU-[");
	
	if (stat & ISES_STAT_BCHU_MASKED)
		printf ("masked");
	else {
		if (stat & ISES_STAT_BCHU_BUSY)
			printf ("busy ");
		if (stat & ISES_STAT_BCHU_ERR)
			printf ("err ");
		if (stat & ISES_STAT_BCHU_SCIF)
			printf ("cr-inop ");
		if (stat & ISES_STAT_BCHU_HIF)
			printf ("ha-inop ");
		if (stat & ISES_STAT_BCHU_DDB)
			printf ("dscd-data ");
		if (stat & ISES_STAT_BCHU_IRF)
			printf ("inp-req ");
		if (stat & ISES_STAT_BCHU_OAF)
			printf ("out-avail ");
		if (stat & ISES_STAT_BCHU_DIE)
			printf ("inp-enabled ");
		if (stat & ISES_STAT_BCHU_UE)
			printf ("ififo-empty ");
		if (stat & ISES_STAT_BCHU_IFE)
			printf ("ififo-half ");
		if (stat & ISES_STAT_BCHU_IFHE)
			printf ("ififo-full ");
		if (stat & ISES_STAT_BCHU_OFE)
			printf ("ofifo-empty ");
		if (stat & ISES_STAT_BCHU_OFHF)
			printf ("ofifo-half ");
		if (stat & ISES_STAT_BCHU_OFF)
			printf ("ofifo-full ");
	}
	printf ("] \n\tmisc-[");
	
	if (stat & ISES_STAT_HW_DA)
		printf ("downloaded-appl ");
	if (stat & ISES_STAT_HW_ACONF)
		printf ("A-IF-conf ");
	if (stat & ISES_STAT_SW_WFOQ)
		printf ("OQ-wait ");
	if (stat & ISES_STAT_SW_OQSINC)
		printf ("OQS-increased ");
	printf ("]\n\t");
	
	if (stat & ISES_STAT_HW_DA)
		printf ("SW-mode is \"%s\"", 
		    ises_sw_mode[ISES_STAT_SW_MODE(stat)]);
	else
		printf ("LDP-state is \"%s\"", 
		    ises_idp_state[ISES_STAT_IDP_STATE(stat)]);
	printf ("\n");

	printf ("\tOQS = %d\n\tIQS = %d\n", READ_REG(sc, ISES_A_OQS),
	    READ_REG(sc, ISES_A_IQS));
	
	/* B interface */
	
	printf ("B-interface status register contains [%08x]\n", 
	    READ_REG(sc, ISES_B_STAT));
	
	/* DMA */
	
	printf ("DMA read starts at 0x%x, length %d bytes\n", 
	    READ_REG(sc, ISES_DMA_READ_START), 
	    READ_REG(sc, ISES_DMA_READ_COUNT));
	
	printf ("DMA write starts at 0x%x, length %d bytes\n",
	    READ_REG(sc, ISES_DMA_WRITE_START),
	    READ_REG(sc, ISES_DMA_WRITE_COUNT));

	printf ("DMA status register contains [%08x]\n", 
	    READ_REG(sc, ISES_DMA_STATUS));

	/* OMR / HOMR / SOMR */
	
	/*
	 * All these means throwing a cmd on to the A-interface, and then
	 * reading the result.
	 *
	 * Currently, put debug output in process_oqueue...
	 */
	
	printf ("Queueing Operation Method Register (OMR) READ cmd...\n");
	cmd = ISES_MKCMD(ISES_CMD_BR_OMR, 0);
	ises_queue_cmd(sc, cmd, NULL, NULL);
}

void
ises_debug_parse_omr (struct ises_softc *sc)
{
	u_int32_t omr = sc->sc_bsession.omr;
	
	printf ("SELR : ");
	if (omr & ISES_SELR_BCHU_EH)
		printf ("cont-on-error ");
	else
		printf ("stop-on-error ");
	
	if (omr & ISES_SELR_BCHU_HISOF)
		printf ("HU-input-is-SCU-output ");
	
	if (omr & ISES_SELR_BCHU_DIS)
		printf ("data-interface-select ");
	
	printf ("\n");
	
	printf ("HOMR : ");
	if (omr & ISES_HOMR_HMTR)
		printf ("expect-padded-hash-msg ");
	else
		printf ("expect-plaintext-hash-msg ");
	
	printf ("ER=%d ", (omr & ISES_HOMR_ER) >> 20); /* ick */
	
	printf ("HFR=");
	switch (omr & ISES_HOMR_HFR) {
	case ISES_HOMR_HFR_NOP:
		printf ("inactive ");
		break;
	case ISES_HOMR_HFR_MD5:
		printf ("MD5 ");
		break;
	case ISES_HOMR_HFR_RMD160:
		printf ("RMD160 ");
		break;
	case ISES_HOMR_HFR_RMD128:
		printf ("RMD128 ");
		break;
	case ISES_HOMR_HFR_SHA1:
		printf ("SHA-1 ");
		break;
	default:
		printf ("reserved! ");
		break;
	}
	printf ("\nSOMR : ");
	
	switch (omr & ISES_SOMR_BOMR) {
	case ISES_SOMR_BOMR_NOP:
		printf ("NOP ");
		break;
	case ISES_SOMR_BOMR_TRANSPARENT:
		printf ("transparent ");
		break;
	case ISES_SOMR_BOMR_DES:
		printf ("DES ");
		break;
	case ISES_SOMR_BOMR_3DES2:
		printf ("3DES-2 ");
		break;
	case ISES_SOMR_BOMR_3DES:
		printf ("3DES-3 ");
		break;
	default:
		if (omr & ISES_SOMR_BOMR_SAFER)
			printf ("SAFER ");
		else
			printf ("reserved! ");
		break;
	}
	
	if (omr & ISES_SOMR_EDR)
		printf ("mode=encrypt ");
	else
		printf ("mode=decrypt ");
	
	switch (omr & ISES_SOMR_FMR) {
	case ISES_SOMR_FMR_ECB:
		printf ("ECB");
		break;
	case ISES_SOMR_FMR_CBC:
		printf ("CBC");
		break;
	case ISES_SOMR_FMR_CFB64:
		printf ("CFB64");
		break;
	case ISES_SOMR_FMR_OFB64:
		printf ("OFB64");
		break;
	default:
		/* Nada */
	}
	printf ("\n");
}

#endif /* ISESDEBUG */
