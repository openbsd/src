/*	$OpenBSD: ises.c,v 1.25 2004/05/04 16:59:31 grange Exp $	*/

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
#include <dev/microcode/ises/ises_fw.h>

/*
 * Prototypes and count for the pci_device structure
 */
int	ises_match(struct device *, void *, void *);
void	ises_attach(struct device *, struct device *, void *);

void	ises_initstate(void *);
void	ises_hrng_init(struct ises_softc *);
void	ises_hrng(void *);
void	ises_process_oqueue(struct ises_softc *);
int	ises_queue_cmd(struct ises_softc *, u_int32_t, u_int32_t *, 
		       u_int32_t (*)(struct ises_softc *, struct ises_cmd *));
u_int32_t ises_get_fwversion(struct ises_softc *);
int	ises_assert_cmd_mode(struct ises_softc *);

int	ises_intr(void *);
int	ises_newsession(u_int32_t *, struct cryptoini *);
int	ises_freesession(u_int64_t);
int	ises_process(struct cryptop *);
void	ises_callback(struct ises_q *);
int	ises_feed(struct ises_softc *);
int	ises_bchu_switch_session(struct ises_softc *, 
				      struct ises_session *, int);
u_int32_t ises_bchu_switch_final(struct ises_softc *, struct ises_cmd *);

void	ises_read_dma(struct ises_softc *);

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
void	ises_debug_init(struct ises_softc *);
void	ises_debug_2(void);
void	ises_debug_loop(void *);
void	ises_showreg(void);
void	ises_debug_parse_omr(struct ises_softc *);
void	ises_debug_simple_cmd(struct ises_softc *, u_int32_t, u_int32_t);
struct ises_softc *ises_sc;
struct timeout ises_db_timeout;
int ises_db;
#endif

/* For HRNG entropy collection, these values gather 1600 bytes/s */
#ifndef ISESRNGBITS
#define ISESRNGBITS	128		/* Bits per iteration (mult. of 32) */
#define ISESRNGIPS	100		/* Iterations per second */
#endif

/* XXX Disable HRNG while debugging. */
#define ISES_HRNG_DISABLED

/* Maximum number of times we try to download the firmware. */
#define ISES_MAX_DOWNLOAD_RETRIES	3

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
	    &sc->sc_memh, NULL, &memsize, 0)) {
		printf(": can't find mem space\n");
		return;
	}
	state++;

	/* Map interrupt. */
	if (pci_intr_map(pa, &ih)) {
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
	    0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->sc_dmamap);
	if (error) {
		printf(": cannot create dma map (%d)\n", error);
		goto fail;
	}
	state++;

	/* Allocate in DMAable memory. */
	if (bus_dmamem_alloc(sc->sc_dmat, ISES_B_DATASIZE, 1, 0, &seg, 1,
	    &nsegs, BUS_DMA_NOWAIT)) {
		printf(": can't alloc dma buffer space\n");
		goto fail;
	}
	state++;

	if (bus_dmamem_map(sc->sc_dmat, &seg, nsegs, ISES_B_DATASIZE,
	    &sc->sc_dma_data, 0)) {
		printf(": can't map dma buffer space\n");
		goto fail;
	}
	state++;

	printf(": %s\n", intrstr);

	bzero(&isesstats, sizeof(isesstats));

	sc->sc_cid = crypto_get_driverid(0);

	if (sc->sc_cid < 0)
		goto fail;

	/*
	 * Since none of the initialization steps generate interrupts
	 * for example, the hardware reset, we use a number of timeouts
	 * (or init states) to do the rest of the chip initialization.
	 */

	sc->sc_initstate = 0;
	startuphook_establish(ises_initstate, sc);

#ifdef ISESDEBUG
	ises_debug_init(sc);
#endif
	return;

 fail:
	switch (state) { /* Always fallthrough here. */
	case 4:
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)&sc->sc_dma_data,
		    sizeof sc->sc_dma_data);
	case 3:
		bus_dmamem_free(sc->sc_dmat, &seg, nsegs);
	case 2:
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
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
	int p, ticks, algs[CRYPTO_ALGORITHM_MAX + 1];
	static int retry_count = 0; /* XXX Should be in softc */

	ticks = hz * 3 / 2; /* 1.5s */ 

	p = ISES_STAT_IDP_STATE(READ_REG(sc, ISES_A_STAT));
	DPRINTF(("%s: initstate %d, IDP state is %d \"%s\"\n", dv, 
		  sc->sc_initstate, p, ises_idp_state[p]));

	switch (sc->sc_initstate) {
	case 0:
		/* Called by dostartuphooks(9). */
		timeout_set(&sc->sc_timeout, ises_initstate, sc);

		/* FALLTHROUGH */
		sc->sc_initstate++;
	case 1:
		/* Power up the chip (clear powerdown bit) */
		stat = READ_REG(sc, ISES_BO_STAT);
		if (stat & ISES_BO_STAT_POWERDOWN) {
			stat &= ~ISES_BO_STAT_POWERDOWN;
			WRITE_REG(sc, ISES_BO_STAT, stat);
			/* Selftests will take 1 second. */
			break;
		}
#if 1
		else {
			/* Power down the chip for sane init, then rerun. */
			stat |= ISES_BO_STAT_POWERDOWN;
			WRITE_REG(sc, ISES_BO_STAT, stat);
			sc->sc_initstate--; /* Rerun state 1. */
			break;
		}
#else
		/* FALLTHROUGH (chip is already powered up) */
		sc->sc_initstate++;
#endif

	case 2:
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

	case 3:
		/* Set AConf to zero, i.e 32-bits access to A-int. */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat &= ~ISES_BO_STAT_ACONF;
		WRITE_REG(sc, ISES_BO_STAT, stat);

		/* Is the firmware already loaded? */
		if (READ_REG(sc, ISES_A_STAT) & ISES_STAT_HW_DA) {
			/* Yes it is, jump ahead a bit */
			ticks = 1;
			sc->sc_initstate += 3; /* Next step --> 7 */
			break;
		}

		/*
		 * Download the Basic Functionality firmware.
		 */

		p = ISES_STAT_IDP_STATE(READ_REG(sc, ISES_A_STAT));
		if (p == ISES_IDP_WFPL) {
			/* We're ready to download. */
			ticks = 1;
			sc->sc_initstate += 2; /* Next step --> 6 */
			break;
		}

		/*
		 * Prior to downloading we need to reset the NSRAM.
		 * Setting the tamper bit will erase the contents
		 * in 1 microsecond.
		 */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat |= ISES_BO_STAT_TAMPER;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		ticks = 1;
		break;

	case 4:
		/* After tamper bit has been set, powerdown chip. */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat |= ISES_BO_STAT_POWERDOWN;
		WRITE_REG(sc, ISES_BO_STAT, stat);
		/* Wait one second for power to dissipate. */
		break;

	case 5:
		/* Clear tamper and powerdown bits. */
		stat = READ_REG(sc, ISES_BO_STAT);
		stat &= ~(ISES_BO_STAT_TAMPER | ISES_BO_STAT_POWERDOWN);
		WRITE_REG(sc, ISES_BO_STAT, stat);
		/* Again we need to wait a second for selftests. */
		break;

	case 6:
		/*
		 * We'll need some space in the input queue (IQF)
		 * and we need to be in the 'waiting for program
		 * length' IDP state (0x4).
		 */
		p = ISES_STAT_IDP_STATE(READ_REG(sc, ISES_A_STAT));
		if (READ_REG(sc, ISES_A_IQF) < 4 || p != ISES_IDP_WFPL) {
			if (retry_count++ < ISES_MAX_DOWNLOAD_RETRIES) {
				/* Retry download. */
				sc->sc_initstate -= 5; /* Next step --> 2 */
				ticks = 1;
				break;
			}
			retry_count = 0;
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

	case 7:
		/* Did the download succed? */
		if (READ_REG(sc, ISES_A_STAT) & ISES_STAT_HW_DA) {
			ticks = 1;
			break;
		}

		/* We failed. */
		goto fail;

	case 8:
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

		/* We can use firmware versions 1.x & 2.x */
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
		bzero(algs, sizeof(algs));

		algs[CRYPTO_3DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
		algs[CRYPTO_DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
		algs[CRYPTO_MD5_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
		algs[CRYPTO_SHA1_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
		algs[CRYPTO_RIPEMD160_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;

		crypto_register(sc->sc_cid, algs,
		    ises_newsession, ises_freesession, ises_process);
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

#ifdef ISESDEBUG
	if (code != ISES_CMD_HBITS) /* ... since this happens 100 times/s */
		DPRINTF(("%s: queueing cmd 0x%x len %d\n", sc->sc_dv.dv_xname,
		    code, len));
#endif

	s = splnet();

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
	cq->cmd_session = sc->sc_cursession;
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

	/* Signal 'command ready'. */
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
	struct ises_session *ses;
	u_int32_t oqs, r, d;
	int cmd, len, c, s;

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

		s = splnet();
		if (!SIMPLEQ_EMPTY(&sc->sc_cmdq)) {
			cq = SIMPLEQ_FIRST(&sc->sc_cmdq);
			SIMPLEQ_REMOVE_HEAD(&sc->sc_cmdq, cmd_next);
			cq->cmd_rlen = len;
		} else {
			cq = NULL;
			DPRINTF(("%s:process_oqueue: cmd queue empty!\n", dv));
		}
		splx(s);

		if (r) {
			/* Ouch. This command generated an error */
			DPRINTF(("%s:process_oqueue: cmd 0x%x err %d\n", dv, 
			    cmd, (r & ISES_RC_MASK)));
			/* Abort any running session switch to force a retry.*/
			sc->sc_switching = 0;
			/* Return to CMD mode. This will reset all queues. */
			(void)ises_assert_cmd_mode(sc);
		} else {
			/* Use specified callback, if any */
			if (cq && cq->cmd_cb) {
				if (cmd == cq->cmd_code) {
					cq->cmd_cb(sc, cq);
					cmd = ISES_CMD_NONE;
				} else {
					DPRINTF(("%s:process_oqueue: expected"
					    " cmd 0x%x, got 0x%x\n", dv, 
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
				ses = &sc->sc_sessions[cq->cmd_session];
				ses->omr = READ_REG(sc, ISES_A_OQD);
				DPRINTF(("%s:process_oqueue: read OMR[%08x]\n",
				    dv, ses->omr));
#ifdef ISESDEBUG
				ises_debug_parse_omr(sc);
#endif
				break;

			case ISES_CMD_BSWITCH:
				/* XXX Currently BSWITCH does not work. */
				DPRINTF(("%s:process_oqueue: BCHU_SWITCH\n"));
				/* Put switched BCHU session in cur session. */
				ses = &sc->sc_sessions[cq->cmd_session];
				for(c = 0; len > 0; len--, c++)
#if 0 /* Don't store the key, just drain the data */
					*((u_int32_t *)&ses + c) =
#endif
					    READ_REG(sc, ISES_A_OQD);

				sc->sc_switching = 0;
				ises_feed (sc);
				break;

			case ISES_CMD_BW_HMLR:
				/* XXX Obsoleted by ises_bchu_switch_final */
				DPRINTF(("%s:process_oqueue: CMD_BW_HMLR !?\n",
				    dv));
				break;

			default:
				/* All other are ok (no response data) */
				DPRINTF(("%s:process_oqueue cmd 0x%x len %d\n",
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
	u_int32_t ints, dma_status, cmd; 
	char *dv = sc->sc_dv.dv_xname;

	dma_status = READ_REG(sc, ISES_DMA_STATUS);

	if (!(dma_status & (ISES_DMA_STATUS_R_ERR | ISES_DMA_STATUS_W_ERR))) {
		if ((sc->sc_dma_mask & ISES_DMA_STATUS_R_RUN) != 0 &&
		    (dma_status & ISES_DMA_STATUS_R_RUN) == 0) {
			DPRINTF(("%s: DMA read complete\n", dv));

			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, 0,
			    sc->sc_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

			/* XXX Pick up and return the data.*/

			WRITE_REG(sc, ISES_DMA_RESET, 0);
		}
		if ((sc->sc_dma_mask & ISES_DMA_STATUS_W_RUN) != 0 &&
		    (dma_status & ISES_DMA_STATUS_W_RUN) == 0) {
			DPRINTF(("%s: DMA write complete\n", dv));

			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, 0,
			    sc->sc_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);

			WRITE_REG(sc, ISES_DMA_RESET, 0);
			ises_feed(sc);
		}
	} else {
		printf ("%s: DMA error\n", dv);
		WRITE_REG(sc, ISES_DMA_RESET, 0);
	}

	ints = READ_REG(sc, ISES_A_INTS);
	if (!(ints & sc->sc_intrmask)) {
		DPRINTF (("%s: other intr mask [%08x]\n", ints));
		return (0); /* Not our interrupt. */
	}

	/* Clear all set intr bits. */
	WRITE_REG(sc, ISES_A_INTS, ints);

#if 0
	/* Check it we've got room for more data. */
	if (READ_REG(sc, ISES_A_STAT) &
	    (ISES_STAT_BCHU_IFE | ISES_STAT_BCHU_IFHE))
		ises_feed(sc);
#endif

	/* Does the A-intf output queue have data we need to process? */
	if (ints & ISES_STAT_SW_OQSINC)
		ises_process_oqueue(sc);

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
		/* Read DMA data from B-interface. */
		ises_read_dma (sc);
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
	bus_dma_segment_t *ds = &sc->sc_dmamap->dm_segs[0];
	u_int32_t dma_status;
	int s;
#ifdef ISESDEBUG
	char *dv = sc->sc_dv.dv_xname;
#endif

	DPRINTF(("%s:ises_feed: called (sc = %p)\n", dv, sc));
	DELAY(1000000);

	s = splnet();
	/* Anything to do? */
	if (SIMPLEQ_EMPTY(&sc->sc_queue) ||
	    (READ_REG(sc, ISES_A_STAT) & ISES_STAT_BCHU_IFF)) {
		splx(s);
		return (0);
	}

	/* Pick the first */
	q = SIMPLEQ_FIRST(&sc->sc_queue);
	splx(s);

	/* If we're currently switching sessions, we'll have to wait. */
	if (sc->sc_switching != 0) {
		DPRINTF(("%s:ises_feed: waiting for session switch\n", dv));
		return (0);
	}

	/* If on-chip data is not correct for this data, switch session. */
	if (sc->sc_cursession != q->q_sesn) {
		/* Session switch required */
		DPRINTF(("%s:ises_feed: initiating session switch\n", dv));
		if (ises_bchu_switch_session (sc, &q->q_session, q->q_sesn))
			sc->sc_cursession = q->q_sesn;
		else
			DPRINTF(("%s:ises_feed: session switch failed\n", dv));
		return (0);
	}

	DPRINTF(("%s:ises_feed: feed to chip (q = %p)\n", dv, q));
	DELAY(2000000);

	s = splnet();
	SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q_next);
	SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
	--sc->sc_nqueue;
	splx(s);

	if (q->q_crp->crp_flags & CRYPTO_F_IMBUF)
		bus_dmamap_load_mbuf(sc->sc_dmat, sc->sc_dmamap, 
		    q->q_src.mbuf, BUS_DMA_NOWAIT);
	else if (q->q_crp->crp_flags & CRYPTO_F_IOV)
		bus_dmamap_load_uio(sc->sc_dmat, sc->sc_dmamap, q->q_src.uio,
		    BUS_DMA_NOWAIT);
	/* ... else */	

	/* Start writing data to the ises. */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, 0,
	    sc->sc_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);
	
	DPRINTF(("%s:ises_feed: writing DMA\n", dv));
	DELAY(1000000);

	sc->sc_dma_mask |= ISES_DMA_STATUS_W_RUN;

	WRITE_REG(sc, ISES_DMA_WRITE_START, ds->ds_addr);
	WRITE_REG(sc, ISES_DMA_WRITE_COUNT, ISES_DMA_WCOUNT(ds->ds_len));

	dma_status = READ_REG(sc, ISES_DMA_STATUS);
	dma_status |= ISES_DMA_CTRL_ILT | ISES_DMA_CTRL_RLINE;
	WRITE_REG(sc, ISES_DMA_CTRL, dma_status);

	DPRINTF(("%s:ises_feed: done\n", dv));
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
#ifdef ISESDEBUG
	char *dv;
#endif

	if (sidp == NULL || cri == NULL)
		return (EINVAL);

	for (i = 0; i < ises_cd.cd_ndevs; i++) {
		sc = ises_cd.cd_devs[i];
		if (sc == NULL || sc->sc_cid == (*sidp))
			break;
	}
	if (sc == NULL)
		return (EINVAL);
#ifdef ISESDEBUG
	dv = sc->sc_dv.dv_xname;
#endif

	DPRINTF(("%s:ises_newsession: start\n", dv));

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

#ifdef ISESDEBUG
	printf ("%s:ises_newsession: mac=%p(%d) enc=%p(%d)\n",
	   dv, mac, (mac ? mac->cri_alg : -1), enc, (enc ? enc->cri_alg : -1));
#endif

	/* Allocate a new session */
	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct ises_session *)
		    malloc(sizeof(struct ises_session), M_DEVBUF, M_NOWAIT);
		if (ses == NULL) {
			isesstats.nomem++;
			return (ENOMEM);
		}
		sc->sc_cursession = -1;
		sesn = 0;
		sc->sc_nsessions = 1;
	} else {
		ses = NULL;
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++)
			if (sc->sc_sessions[sesn].omr == 0) {
				ses = &sc->sc_sessions[sesn];
				sc->sc_cursession = sesn;
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
			sc->sc_cursession = sc->sc_nsessions;
			sc->sc_nsessions++;
		}
	}

	DPRINTF(("%s:ises_newsession: nsessions=%d cursession=%d\n", dv,
	    sc->sc_nsessions, sc->sc_cursession));

	bzero(ses, sizeof(struct ises_session));

	/* Select data path through B-interface. */
	ses->omr |= ISES_SELR_BCHU_DIS;

	if (enc) {
		/* get an IV, network byte order */
		/* XXX switch to using builtin HRNG ! */
		get_random_bytes(ses->sccr, sizeof(ses->sccr));

		/* crypto key */
		if (enc->cri_alg == CRYPTO_DES_CBC) {
			bcopy(enc->cri_key, &ses->kr[0], 8);
			bcopy(enc->cri_key, &ses->kr[2], 8);
			bcopy(enc->cri_key, &ses->kr[4], 8);
		} else
			bcopy(enc->cri_key, &ses->kr[0], 24);

		SWAP32(ses->kr[0]);
		SWAP32(ses->kr[1]);
		SWAP32(ses->kr[2]);
		SWAP32(ses->kr[3]);
		SWAP32(ses->kr[4]);
		SWAP32(ses->kr[5]);
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
			MD5Final((u_int8_t *)&ses->cvr, &md5ctx);
			break;
		case CRYPTO_SHA1_HMAC:
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, mac->cri_key, mac->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_ipad_buffer, HMAC_BLOCK_LEN -
			    (mac->cri_klen / 8));
			SHA1Final((u_int8_t *)ses->cvr, &sha1ctx);
			break;
		case CRYPTO_RIPEMD160_HMAC:
		default:
			RMD160Init(&rmd160ctx);
			RMD160Update(&rmd160ctx, mac->cri_key,
			    mac->cri_klen / 8);
			RMD160Update(&rmd160ctx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (mac->cri_klen / 8));
			RMD160Final((u_int8_t *)ses->cvr, &rmd160ctx);
			break;
		}

		for (i = 0; i < mac->cri_klen / 8; i++)
			mac->cri_key[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

		switch (mac->cri_alg) {
		case CRYPTO_MD5_HMAC:
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, mac->cri_key, mac->cri_klen / 8);
			MD5Update(&md5ctx, hmac_opad_buffer, HMAC_BLOCK_LEN -
			    (mac->cri_klen / 8));
			MD5Update(&md5ctx, (u_int8_t *)ses->cvr,
			    sizeof(md5ctx.state));
			MD5Final((u_int8_t *)ses->cvr, &md5ctx);
			break;
		case CRYPTO_SHA1_HMAC:
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, mac->cri_key, mac->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_opad_buffer, HMAC_BLOCK_LEN -
			    (mac->cri_klen / 8));
			SHA1Update(&sha1ctx, (u_int8_t *)ses->cvr,
			    sizeof(sha1ctx.state));
			SHA1Final((u_int8_t *)ses->cvr, &sha1ctx);
			break;
		case CRYPTO_RIPEMD160_HMAC:
		default:
			RMD160Init(&rmd160ctx);
			RMD160Update(&rmd160ctx, mac->cri_key,
			    mac->cri_klen / 8);
			RMD160Update(&rmd160ctx, hmac_opad_buffer,
			    HMAC_BLOCK_LEN - (mac->cri_klen / 8));
			RMD160Update(&rmd160ctx, (u_int8_t *)ses->cvr, 
			    sizeof(rmd160ctx.state));
			RMD160Final((u_int8_t *)ses->cvr, &rmd160ctx);
			break;
		}

		for (i = 0; i < mac->cri_klen / 8; i++)
			mac->cri_key[i] ^= HMAC_OPAD_VAL;
	}

	DPRINTF(("%s:ises_newsession: done\n", dv));
	*sidp = ISES_SID(sc->sc_dv.dv_unit, sesn);
	return (0);
}

/* Deallocate a session. */
int
ises_freesession(u_int64_t tsid)
{
	struct ises_softc *sc;
	int card, sesn;
	u_int32_t sid = ((u_int32_t)tsid) & 0xffffffff;

	card = ISES_CARD(sid);
	if (card >= ises_cd.cd_ndevs || ises_cd.cd_devs[card] == NULL)
		return (EINVAL);

	sc = ises_cd.cd_devs[card];
	sesn = ISES_SESSION(sid);

	DPRINTF(("%s:ises_freesession: freeing session %d\n",
	    sc->sc_dv.dv_xname, sesn));

	if (sc->sc_cursession == sesn)
		sc->sc_cursession = -1;

	bzero(&sc->sc_sessions[sesn], sizeof(sc->sc_sessions[sesn]));

	return (0);
}

/* Called by the crypto framework, crypto(9). */
int
ises_process(struct cryptop *crp)
{
	struct ises_softc *sc;
	struct ises_q *q;
	struct cryptodesc *maccrd, *enccrd, *crd;
	struct ises_session *ses;
	int card, s, err = EINVAL;
	int encoffset, macoffset, cpskip, sskip, dskip, stheend, dtheend;
	int cpoffset, coffset;
#if 0
	int nicealign;
#endif
#ifdef ISESDEBUG
	char *dv;
#endif

	if (crp == NULL || crp->crp_callback == NULL)
		return (EINVAL);

	card = ISES_CARD(crp->crp_sid);
	if (card >= ises_cd.cd_ndevs || ises_cd.cd_devs[card] == NULL)
		goto errout;

	sc = ises_cd.cd_devs[card];
#ifdef ISESDEBUG
	dv = sc->sc_dv.dv_xname;
#endif

	DPRINTF(("%s:ises_process: start (crp = %p)\n", dv, crp));

	s = splnet();
	if (sc->sc_nqueue == ISES_MAX_NQUEUE) {
		splx(s);
		goto memerr;
	}
	splx(s);

	q = (struct ises_q *)malloc(sizeof(struct ises_q), M_DEVBUF, M_NOWAIT);
	if (q == NULL)
		goto memerr;
	bzero(q, sizeof(struct ises_q));

	q->q_sesn = ISES_SESSION(crp->crp_sid);
	ses = &sc->sc_sessions[q->q_sesn];

	DPRINTF(("%s:ises_process: session %d selected\n", dv, q->q_sesn));

	q->q_sc = sc;
	q->q_crp = crp;

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		q->q_src.mbuf = (struct mbuf *)crp->crp_buf;
		q->q_dst.mbuf = (struct mbuf *)crp->crp_buf;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		q->q_src.uio = (struct uio *)crp->crp_buf;
		q->q_dst.uio = (struct uio *)crp->crp_buf;
	} else {
		/* XXX for now... */
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

	DPRINTF(("%s:ises_process: enc=%p mac=%p\n", dv, enccrd, maccrd));

	/* Select data path through B-interface. */
	q->q_session.omr |= ISES_SELR_BCHU_DIS;

	if (enccrd) {
		encoffset = enccrd->crd_skip;

		/* Select algorithm */
		if (enccrd->crd_alg == CRYPTO_3DES_CBC)
			q->q_session.omr |= ISES_SOMR_BOMR_3DES;
		else
			q->q_session.omr |= ISES_SOMR_BOMR_DES;

		/* Set CBC mode */
		q->q_session.omr |= ISES_SOMR_FMR_CBC;

		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			/* Set encryption bit */
			q->q_session.omr |= ISES_SOMR_EDR;

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, q->q_session.sccr, 8);
			else {
				q->q_session.sccr[0] = ses->sccr[0];
				q->q_session.sccr[1] = ses->sccr[1];
			}

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0) {
				if (crp->crp_flags & CRYPTO_F_IMBUF)
					m_copyback(q->q_src.mbuf,
					    enccrd->crd_inject, 8,
					    (caddr_t)q->q_session.sccr);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					cuio_copyback(q->q_src.uio,
					    enccrd->crd_inject, 8,
					    (caddr_t)q->q_session.sccr);
				/* XXX else ... */
			}
		} else {
			/* Clear encryption bit == decrypt mode */
			q->q_session.omr &= ~ISES_SOMR_EDR;

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, q->q_session.sccr, 8);
			else if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata(q->q_src.mbuf, enccrd->crd_inject,
				    8, (caddr_t)q->q_session.sccr);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata(q->q_src.uio,
				    enccrd->crd_inject, 8,
				    (caddr_t)q->q_session.sccr);
			/* XXX else ... */
		}

		q->q_session.kr[0] = ses->kr[0];
		q->q_session.kr[1] = ses->kr[1];
		q->q_session.kr[2] = ses->kr[2];
		q->q_session.kr[3] = ses->kr[3];
		q->q_session.kr[4] = ses->kr[4];
		q->q_session.kr[5] = ses->kr[5];

		SWAP32(q->q_session.sccr[0]);
		SWAP32(q->q_session.sccr[1]);
	}

	if (maccrd) {
		macoffset = maccrd->crd_skip;

		/* Select algorithm */
		switch (crd->crd_alg) {
		case CRYPTO_MD5_HMAC:
			q->q_session.omr |= ISES_HOMR_HFR_MD5;
			break;
		case CRYPTO_SHA1_HMAC:
			q->q_session.omr |= ISES_HOMR_HFR_SHA1;
			break;
		case CRYPTO_RIPEMD160_HMAC:
		default:
			q->q_session.omr |= ISES_HOMR_HFR_RMD160;
			break;
		}

		q->q_session.cvr[0] = ses->cvr[0];
		q->q_session.cvr[1] = ses->cvr[1];
		q->q_session.cvr[2] = ses->cvr[2];
		q->q_session.cvr[3] = ses->cvr[3];
		q->q_session.cvr[4] = ses->cvr[4];
	}

	if (enccrd && maccrd) {
		/* XXX Check if ises handles differing end of auth/enc etc */
		/* XXX For now, assume not (same as ubsec). */
		if (((encoffset + enccrd->crd_len) !=
		    (macoffset + maccrd->crd_len)) ||
		    (enccrd->crd_skip < maccrd->crd_skip)) {
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

#if 0	/* XXX not sure about this, in bus_dma context */

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		q->q_src_l = mbuf2pages(q->q_src.mbuf, &q->q_src_npa,
		    q->q_src_packp, q->q_src_packl, 1, &nicealign);
	else if (crp->crp_flags & CRYPTO_F_IOV)
		q->q_src_l = iov2pages(q->q_src.uio, &q->q_src_npa,
		    q->q_src_packp, q->q_src_packl, 1, &nicealign);
	/* XXX else */

	DPRINTF(("%s:ises_process: foo2pages called!\n", dv));

	if (q->q_src_l == 0)
		goto memerr;
	else if (q->q_src_l > 0xfffc) {
		err = EIO;
		goto errout;
	}

	/* XXX ... */

	if (enccrd == NULL && maccrd != NULL) {
		/* XXX ... */
	} else {
		if (!nicealign && (crp->crp_flags & CRYPTO_F_IOV)) {
			goto errout;
		} else if (!nicealign && (crp->crp_flags & CRYPTO_F_IMBUF)) {
			int totlen, len;
			struct mbuf *m, *top, **mp;

			totlen = q->q_dst_l = q->q_src_l;
			if (q->q_src.mbuf->m_flags & M_PKTHDR) {
				MGETHDR(m, M_DONTWAIT, MT_DATA);
				M_DUP_PKTHDR(m, q->q_src.mbuf);
				len = MHLEN;
			} else {
				MGET(m, M_DONTWAIT, MT_DATA);
				len = MLEN;
			}
			if (m == NULL)
				goto memerr;
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
						goto memerr;
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
			q->q_dst.mbuf = top;
#if notyet
			ubsec_mcopy(q->q_src.mbuf, q->q_dst.mbuf, cpskip, cpoffset);
#endif
		} else
			q->q_dst.mbuf = q->q_src.mbuf;

#if 0
		/* XXX ? */
		q->q_dst_l = mbuf2pages(q->q_dst.mbuf, &q->q_dst_npa,
		    &q->q_dst_packp, &q->q_dst_packl, 1, NULL);
#endif
	}

#endif /* XXX */

	DPRINTF(("%s:ises_process: queueing request\n", dv));

	s = splnet();
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue, q, q_next);
	sc->sc_nqueue++;
	splx(s);
	ises_feed(sc);

	return (0);

memerr:
	err = ENOMEM;
	isesstats.nomem++;
errout:
	DPRINTF(("%s:ises_process: an error occurred, err=%d, q=%p\n", dv, 
		 err, q));

	if (err == EINVAL)
		isesstats.invalid++;

	if (q) {
		if (q->q_src.mbuf != q->q_dst.mbuf)
			m_freem(q->q_dst.mbuf);
		free(q, M_DEVBUF);
	}
	crp->crp_etype = err;
	crypto_done(crp);
	return (0);
}

void
ises_callback(struct ises_q *q)
{
	struct cryptop *crp = (struct cryptop *)q->q_crp;
	struct cryptodesc *crd;
	struct ises_softc *sc = q->q_sc;
	u_int8_t *sccr;

	if ((crp->crp_flags & CRYPTO_F_IMBUF) && 
	    (q->q_src.mbuf != q->q_dst.mbuf)) {
		m_freem(q->q_src.mbuf);
		crp->crp_buf = (caddr_t)q->q_dst.mbuf;
	}

	if (q->q_session.omr & ISES_SOMR_EDR) {
		/* Copy out IV after encryption. */
		sccr = (u_int8_t *)&sc->sc_sessions[q->q_sesn].sccr;
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_DES_CBC &&
			    crd->crd_alg != CRYPTO_3DES_CBC)
				continue;
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - 8, 8, sccr);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - 8, 8, sccr);
		}
	}

	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		if (crd->crd_alg != CRYPTO_MD5_HMAC &&
		    crd->crd_alg != CRYPTO_SHA1_HMAC &&
		    crd->crd_alg != CRYPTO_RIPEMD160_HMAC)
			continue;
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			m_copyback((struct mbuf *)crp->crp_buf,
			   crd->crd_inject, 12, (u_int8_t *)q->q_macbuf);
		else if (crp->crp_flags & CRYPTO_F_IOV)
			bcopy((u_int8_t *)q->q_macbuf, crp->crp_mac, 12);
		/* XXX else ... */
		break;
	}

	free(q, M_DEVBUF);
	DPRINTF(("%s:ises_callback: calling crypto_done\n",
	    sc->sc_dv.dv_xname));
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
#ifndef ISES_HRNG_DISABLED
	ises_hrng(sc); /* Call first update */
#endif
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

int
ises_bchu_switch_session (struct ises_softc *sc, struct ises_session *ss, 
			  int new_session)
{
	/* It appears that the BCHU_SWITCH_SESSION command is broken. */
	/* We have to work around it. */
	
	u_int32_t cmd;

	/* Do we have enough in-queue space? Count cmds + data, 16bit words. */
	if ((8 * 2 + sizeof (*ss) / 2) > READ_REG(sc, ISES_A_IQF))
		return (0);

	/* Mark 'switch' in progress. */
	sc->sc_switching = new_session + 1;

	/* Write the key. */
	cmd = ISES_MKCMD(ISES_CMD_BW_KR0, 2);
	ises_queue_cmd(sc, cmd, &ss->kr[4], NULL);
	cmd = ISES_MKCMD(ISES_CMD_BW_KR1, 2);
	ises_queue_cmd(sc, cmd, &ss->kr[2], NULL);
	cmd = ISES_MKCMD(ISES_CMD_BW_KR2, 2);
	ises_queue_cmd(sc, cmd, &ss->kr[0], NULL);

	/* Write OMR - Operation Method Register, clears SCCR+CVR+DBCR+HMLR */
	cmd = ISES_MKCMD(ISES_CMD_BW_OMR, 1);
	ises_queue_cmd(sc, cmd, &ss->omr, NULL);

	/* Write SCCR - Symmetric Crypto Chaining Register (IV) */
	cmd = ISES_MKCMD(ISES_CMD_BW_SCCR, 2);
	ises_queue_cmd(sc, cmd, &ss->sccr[0], NULL);

	/* Write CVR - Chaining Variables Register (hash state) */
	cmd = ISES_MKCMD(ISES_CMD_BW_CVR, 5);
	ises_queue_cmd(sc, cmd, &ss->cvr[0], NULL);

	/* Write DBCR - Data Block Count Register */
	cmd = ISES_MKCMD(ISES_CMD_BW_DBCR, 2);
	ises_queue_cmd(sc, cmd, &ss->dbcr[0], NULL);

	/* Write HMLR - Hash Message Length Register - last cmd in switch */
	cmd = ISES_MKCMD(ISES_CMD_BW_HMLR, 2);
	ises_queue_cmd(sc, cmd, &ss->hmlr[0], ises_bchu_switch_final);

	return (1);
}

u_int32_t
ises_bchu_switch_final (struct ises_softc *sc, struct ises_cmd *cmd)
{
	/* Session switch is complete. */

	DPRINTF(("%s:ises_bchu_switch_final: switch complete\n",
	    sc->sc_dv.dv_xname));

	sc->sc_cursession = sc->sc_switching - 1;
	sc->sc_switching = 0;

	/* Retry/restart feed. */
	ises_feed(sc);

	return (0);
}

/* XXX Currently unused. */
void
ises_read_dma (struct ises_softc *sc)
{
	bus_dma_segment_t *ds = &sc->sc_dmamap->dm_segs[0];
	u_int32_t dma_status;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, 0,
	    sc->sc_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	WRITE_REG(sc, ISES_DMA_READ_START, ds->ds_addr);
	WRITE_REG(sc, ISES_DMA_READ_START, ISES_DMA_RCOUNT(ds->ds_len));

	dma_status = READ_REG(sc, ISES_DMA_STATUS);
	dma_status |= ISES_DMA_CTRL_ILT | ISES_DMA_CTRL_WRITE;
	WRITE_REG(sc, ISES_DMA_CTRL, dma_status);
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
ises_debug_loop (void *v)
{
	struct ises_softc *sc = (struct ises_softc *)v;
	struct ises_session ses;
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
		bzero(&ses, sizeof ses);
		ses.kr[0] = 0xD0;
		ses.kr[1] = 0xD1;
		ses.kr[2] = 0xD2;
		ses.kr[3] = 0xD3;
		ses.kr[4] = 0xD4;
		ses.kr[5] = 0xD5;

		/* cipher data out is hash in, SHA1, 3DES, encrypt, ECB */
		ses.omr = ISES_SELR_BCHU_HISOF | ISES_HOMR_HFR_SHA1 |
		    ISES_SOMR_BOMR_3DES | ISES_SOMR_EDR | ISES_SOMR_FMR_ECB;

#if 1
		printf ("Queueing home-cooked session switch\n");
		ises_bchu_switch_session(sc, &ses, 0);
#else /* switch session does not appear to work - it never returns */
		printf ("Queueing BCHU session switch\n");
		cmd = ISES_MKCMD(ISES_CMD_BSWITCH, sizeof ses / 4);
		printf ("session is %d 32bit words (== 18 ?), cmd = [%08x]\n", 
			sizeof ses / 4, cmd);
		ises_queue_cmd(sc, cmd, (u_int32_t *)&ses, NULL);
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
		printf ("IDP-state is \"%s\"", 
		    ises_idp_state[ISES_STAT_IDP_STATE(stat)]);
	printf ("\n");

	printf ("\tOQS = %d  IQS = %d  OQF = %d  IQF = %d\n", 
	    READ_REG(sc, ISES_A_OQS), READ_REG(sc, ISES_A_IQS),
	    READ_REG(sc, ISES_A_OQF), READ_REG(sc, ISES_A_IQF));
	
	/* B interface */
	
	printf ("B-interface status register contains [%08x]\n", 
	    READ_REG(sc, ISES_B_STAT));
	
	/* DMA */
	
	printf ("DMA read starts at 0x%x, length %d bytes\n", 
	    READ_REG(sc, ISES_DMA_READ_START), 
	    READ_REG(sc, ISES_DMA_READ_COUNT) >> 16);
	
	printf ("DMA write starts at 0x%x, length %d bytes\n",
	    READ_REG(sc, ISES_DMA_WRITE_START),
	    READ_REG(sc, ISES_DMA_WRITE_COUNT) & 0x00ff);

	stat = READ_REG(sc, ISES_DMA_STATUS);
	printf ("DMA status register contains [%08x]\n", stat);

	if (stat & ISES_DMA_CTRL_ILT)
		printf (" -- Ignore latency timer\n");
	if (stat & 0x0C000000)
		printf (" -- PCI Read - multiple\n");
	else if (stat & 0x08000000)
		printf (" -- PCI Read - line\n");

	if (stat & ISES_DMA_STATUS_R_RUN)
		printf (" -- PCI Read running/incomplete\n");
	else
		printf (" -- PCI Read complete\n");
	if (stat & ISES_DMA_STATUS_R_ERR)
		printf (" -- PCI Read DMA Error\n");

	if (stat & ISES_DMA_STATUS_W_RUN)
		printf (" -- PCI Write running/incomplete\n");
	else
		printf (" -- PCI Write complete\n");
	if (stat & ISES_DMA_STATUS_W_ERR)
		printf (" -- PCI Write DMA Error\n");

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
	u_int32_t omr = sc->sc_sessions[sc->sc_cursession].omr;
	
	printf ("SELR : ");
	if (omr & ISES_SELR_BCHU_EH)
		printf ("cont-on-error ");
	else
		printf ("stop-on-error ");
	
	if (omr & ISES_SELR_BCHU_HISOF)
		printf ("HU-input-is-SCU-output ");
	
	if (omr & ISES_SELR_BCHU_DIS)
		printf ("data-interface-select=B ");
	else
		printf ("data-interface-select=DataIn/DataOut ");
	
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
