/*	$OpenBSD: siop.c,v 1.21 2002/07/20 11:22:27 art Exp $ */
/*	$NetBSD: siop.c,v 1.39 2001/02/11 18:04:49 bouyer Exp $	*/

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

/* SYM53c7/8xx PCI-SCSI I/O Processors driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <machine/endian.h>
#include <machine/bus.h>

#include <dev/microcode/siop/siop.out>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <dev/ic/siopreg.h>
#include <dev/ic/siopvar.h>
#include <dev/ic/siopvar_common.h>

#ifndef DEBUG
#undef DEBUG
#endif
#undef SIOP_DEBUG
#undef SIOP_DEBUG_DR
#undef SIOP_DEBUG_INTR
#undef SIOP_DEBUG_SCHED
#undef DUMP_SCRIPT

#define SIOP_STATS

#ifndef SIOP_DEFAULT_TARGET
#define SIOP_DEFAULT_TARGET 7
#endif

/* number of cmd descriptors per block */
#define SIOP_NCMDPB (PAGE_SIZE / sizeof(struct siop_xfer))

/* number of scheduler slots (needs to match script) */
#define SIOP_NSLOTS 40

void	siop_reset(struct siop_softc *);
void	siop_handle_reset(struct siop_softc *);
int	siop_handle_qtag_reject(struct siop_cmd *);
void	siop_scsicmd_end(struct siop_cmd *);
void	siop_start(struct siop_softc *);
void 	siop_timeout(void *);
int	siop_scsicmd(struct scsi_xfer *);
void	siop_dump_script(struct siop_softc *);
int	siop_morecbd(struct siop_softc *);
struct siop_lunsw *siop_get_lunsw(struct siop_softc *);
void	siop_add_reselsw(struct siop_softc *, int);
void	siop_update_scntl3(struct siop_softc *, struct siop_target *);

struct cfdriver siop_cd = {
	NULL, "siop", DV_DULL
};

struct scsi_adapter siop_adapter = {
	siop_scsicmd,
	siop_minphys,
	NULL,
	NULL,
};

struct scsi_device siop_dev = {
	NULL,
	NULL,
	NULL,
	NULL,
};

#ifdef SIOP_STATS
static int siop_stat_intr = 0;
static int siop_stat_intr_shortxfer = 0;
static int siop_stat_intr_sdp = 0;
static int siop_stat_intr_done = 0;
static int siop_stat_intr_xferdisc = 0;
static int siop_stat_intr_lunresel = 0;
static int siop_stat_intr_qfull = 0;
void siop_printstats(void);
#define INCSTAT(x) x++
#else
#define INCSTAT(x) 
#endif

static __inline__ void siop_script_sync(struct siop_softc *, int);
static __inline__ void
siop_script_sync(sc, ops)
	struct siop_softc *sc;
	int ops;
{
	if ((sc->features & SF_CHIP_RAM) == 0)
		bus_dmamap_sync(sc->sc_dmat, sc->sc_scriptdma,
		    0, PAGE_SIZE, ops);
}

static __inline__ u_int32_t siop_script_read(struct siop_softc *, u_int);
static __inline__ u_int32_t
siop_script_read(sc, offset)
	struct siop_softc *sc;
	u_int offset;
{
	if (sc->features & SF_CHIP_RAM) {
		return bus_space_read_4(sc->sc_ramt, sc->sc_ramh, offset * 4);
	} else {
		return letoh32(sc->sc_script[offset]);
	}
}

static __inline__ void siop_script_write(struct siop_softc *, u_int,
	u_int32_t);
static __inline__ void
siop_script_write(sc, offset, val)
	struct siop_softc *sc;
	u_int offset;
	u_int32_t val;
{
	if (sc->features & SF_CHIP_RAM) {
		bus_space_write_4(sc->sc_ramt, sc->sc_ramh, offset * 4, val);
	} else {
		sc->sc_script[offset] = htole32(val);
	}
}

void
siop_attach(sc)
	struct siop_softc *sc;
{
	int error, i;
	bus_dma_segment_t seg;
	int rseg;

	/*
	 * Allocate DMA-safe memory for the script and map it.
	 */
	if ((sc->features & SF_CHIP_RAM) == 0) {
		error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, 
		    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to allocate script DMA memory, "
			    "error = %d\n", sc->sc_dev.dv_xname, error);
			return;
		}
		error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, PAGE_SIZE,
		    (caddr_t *)&sc->sc_script, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
		if (error) {
			printf("%s: unable to map script DMA memory, "
			    "error = %d\n", sc->sc_dev.dv_xname, error);
			return;
		}
		error = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1,
		    PAGE_SIZE, 0, BUS_DMA_NOWAIT, &sc->sc_scriptdma);
		if (error) {
			printf("%s: unable to create script DMA map, "
			    "error = %d\n", sc->sc_dev.dv_xname, error);
			return;
		}
		error = bus_dmamap_load(sc->sc_dmat, sc->sc_scriptdma,
		    sc->sc_script, PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to load script DMA map, "
			    "error = %d\n", sc->sc_dev.dv_xname, error);
			return;
		}
		sc->sc_scriptaddr = sc->sc_scriptdma->dm_segs[0].ds_addr;
		sc->ram_size = PAGE_SIZE;
	}
	TAILQ_INIT(&sc->free_list);
	TAILQ_INIT(&sc->ready_list);
	TAILQ_INIT(&sc->urgent_list);
	TAILQ_INIT(&sc->cmds);
	TAILQ_INIT(&sc->lunsw_list);
	sc->sc_currschedslot = 0;
#ifdef SIOP_DEBUG
	printf("%s: script size = %d, PHY addr=0x%x, VIRT=%p\n",
	    sc->sc_dev.dv_xname, (int)sizeof(siop_script),
	    (u_int32_t)sc->sc_scriptaddr, sc->sc_script);
#endif
	/* Start with one page worth of commands */
	siop_morecbd(sc);

	/*
	 * sc->sc_link is the template for all device sc_link's
	 * for devices attached to this adapter. It is passed to
	 * the upper layers in config_found().
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.openings = SIOP_OPENINGS;
	sc->sc_link.adapter_buswidth =
	    (sc->features & SF_BUS_WIDE) ? 16 : 8;
	sc->sc_link.adapter_target = bus_space_read_1(sc->sc_rt,
	    sc->sc_rh, SIOP_SCID);
	if (sc->sc_link.adapter_target == 0 ||
	    sc->sc_link.adapter_target >=
	    sc->sc_link.adapter_buswidth)
		sc->sc_link.adapter_target = SIOP_DEFAULT_TARGET;
	sc->sc_link.adapter = &siop_adapter;
	sc->sc_link.device = &siop_dev;
	sc->sc_link.flags  = 0;
	sc->sc_link.quirks = 0;
	if ((sc->features & SF_BUS_WIDE) == 0)
	  sc->sc_link.quirks |= SDEV_NOWIDE;

	for (i = 0; i < 16; i++)
		sc->targets[i] = NULL;

	/* find min_dt_sync and min_st_sync for this chip */
	sc->min_dt_sync = 0;
	for (i = 0; i < sizeof(period_factor) / sizeof(period_factor[0]); i++)
		if (period_factor[i].scf[sc->scf_index].dt_scf != 0) {
			sc->min_dt_sync = period_factor[i].factor;
			break;
		}

	sc->min_st_sync = 0;
	for (i = 0; i < sizeof(period_factor) / sizeof(period_factor[0]); i++)
		if (period_factor[i].scf[sc->scf_index].st_scf != 0) {
			sc->min_st_sync = period_factor[i].factor;
			break;
		}

	if (sc->min_st_sync == 0)
		panic("%s: can't find minimum allowed sync period factor\n", sc->sc_dev.dv_xname);

	/* Do a bus reset, so that devices fall back to narrow/async */
	siop_resetbus(sc);
	/*
	 * siop_reset() will reset the chip, thus clearing pending interrupts
	 */
	siop_reset(sc);
#ifdef DUMP_SCRIPT
	siop_dump_script(sc);
#endif

	config_found((struct device*)sc, &sc->sc_link, scsiprint);
}

void
siop_reset(sc)
	struct siop_softc *sc;
{
	int i, j;
	struct siop_lunsw *lunsw;

	siop_common_reset(sc);

	/* copy and patch the script */
	if (sc->features & SF_CHIP_RAM) {
		bus_space_write_region_4(sc->sc_ramt, sc->sc_ramh, 0,
		    siop_script, sizeof(siop_script) / sizeof(siop_script[0]));
		for (j = 0; j <
		    (sizeof(E_abs_msgin_Used) / sizeof(E_abs_msgin_Used[0]));
		    j++) {
			bus_space_write_4(sc->sc_ramt, sc->sc_ramh,
			    E_abs_msgin_Used[j] * 4,
			    sc->sc_scriptaddr + Ent_msgin_space);
		}
	} else {
		for (j = 0;
		    j < (sizeof(siop_script) / sizeof(siop_script[0])); j++) {
			sc->sc_script[j] = htole32(siop_script[j]);
		}
		for (j = 0; j <
		    (sizeof(E_abs_msgin_Used) / sizeof(E_abs_msgin_Used[0]));
		    j++) {
			sc->sc_script[E_abs_msgin_Used[j]] =
			    htole32(sc->sc_scriptaddr + Ent_msgin_space);
		}
	}
	sc->script_free_lo = sizeof(siop_script) / sizeof(siop_script[0]);
	sc->script_free_hi = sc->ram_size / 4;

	/* free used and unused lun switches */
	while((lunsw = TAILQ_FIRST(&sc->lunsw_list)) != NULL) {
#ifdef SIOP_DEBUG
		printf("%s: free lunsw at offset %d\n",
				sc->sc_dev.dv_xname, lunsw->lunsw_off);
#endif
		TAILQ_REMOVE(&sc->lunsw_list, lunsw, next);
		free(lunsw, M_DEVBUF);
	}
	TAILQ_INIT(&sc->lunsw_list);
	/* restore reselect switch */
	for (i = 0; i < sc->sc_link.adapter_buswidth; i++) {
		if (sc->targets[i] == NULL)
			continue;
#ifdef SIOP_DEBUG
		printf("%s: restore sw for target %d\n",
				sc->sc_dev.dv_xname, i);
#endif
		free(sc->targets[i]->lunsw, M_DEVBUF);
		sc->targets[i]->lunsw = siop_get_lunsw(sc);
		if (sc->targets[i]->lunsw == NULL) {
			printf("%s: can't alloc lunsw for target %d\n",
			    sc->sc_dev.dv_xname, i);
			break;
		}
		siop_add_reselsw(sc, i);
	}

	/* start script */
	siop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP,
	    sc->sc_scriptaddr + Ent_reselect);
}

#if 0
#define CALL_SCRIPT(ent) do {\
	printf ("start script DSA 0x%lx DSP 0x%lx\n", \
	    siop_cmd->dsa, \
	    sc->sc_scriptaddr + ent); \
bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP, sc->sc_scriptaddr + ent); \
} while (0)
#else
#define CALL_SCRIPT(ent) do {\
bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP, sc->sc_scriptaddr + ent); \
} while (0)
#endif

int
siop_intr(v)
	void *v;
{
	struct siop_softc *sc = v;
	struct siop_target *siop_target;
	struct siop_cmd *siop_cmd;
	struct siop_lun *siop_lun;
	struct scsi_xfer *xs;
	int istat, sist = 0, sstat1 = 0, dstat = 0;
	u_int32_t irqcode = 0;
	int need_reset = 0;
	int offset, target, lun, tag;
	bus_addr_t dsa;
	struct siop_cbd *cbdp;
	int freetarget = 0;
	int restart = 0;

	istat = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT);
	if ((istat & (ISTAT_INTF | ISTAT_DIP | ISTAT_SIP)) == 0)
		return 0;
	INCSTAT(siop_stat_intr);
	if (istat & ISTAT_INTF) {
		printf("INTRF\n");
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_INTF);
	}
	/* use DSA to find the current siop_cmd */
	dsa = bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA);
	for (cbdp = TAILQ_FIRST(&sc->cmds); cbdp != NULL;
	    cbdp = TAILQ_NEXT(cbdp, next)) {
		if (dsa >= cbdp->xferdma->dm_segs[0].ds_addr &&
	    	    dsa < cbdp->xferdma->dm_segs[0].ds_addr + PAGE_SIZE) {
			dsa -= cbdp->xferdma->dm_segs[0].ds_addr;
			siop_cmd = &cbdp->cmds[dsa / sizeof(struct siop_xfer)];
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			break;
		}
	} 
	if (cbdp == NULL) {
		siop_cmd = NULL;
	}
	if (siop_cmd) {
		xs = siop_cmd->xs;
		siop_target = siop_cmd->siop_target;
		target = siop_cmd->xs->sc_link->target;
		lun = siop_cmd->xs->sc_link->lun;
		tag = siop_cmd->tag;
		siop_lun = siop_target->siop_lun[lun];
#ifdef DIAGNOSTIC
		if (siop_cmd->status != CMDST_ACTIVE &&
		    siop_cmd->status != CMDST_SENSE_ACTIVE) {
			printf("siop_cmd (lun %d) not active (%d)\n",
				lun, siop_cmd->status);
			xs = NULL;
			siop_target = NULL;
			target = -1;
			lun = -1;
			tag = -1;
			siop_lun = NULL;
			siop_cmd = NULL;
		} else if (siop_lun->siop_tag[tag].active != siop_cmd) {
			printf("siop_cmd (lun %d tag %d) not in siop_lun "
			    "active (%p != %p)\n", lun, tag, siop_cmd,
			    siop_lun->siop_tag[tag].active);
		}
#endif
	} else {
		xs = NULL;
		siop_target = NULL;
		target = -1;
		lun = -1;
		tag = -1;
		siop_lun = NULL;
	}
	if (istat & ISTAT_DIP) {
		dstat = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_DSTAT);
		if (dstat & DSTAT_SSI) {
			printf("single step dsp 0x%08x dsa 0x08%x\n",
			    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
			    sc->sc_scriptaddr),
			    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA));
			if ((dstat & ~(DSTAT_DFE | DSTAT_SSI)) == 0 &&
			    (istat & ISTAT_SIP) == 0) {
				bus_space_write_1(sc->sc_rt, sc->sc_rh,
				    SIOP_DCNTL, bus_space_read_1(sc->sc_rt,
				    sc->sc_rh, SIOP_DCNTL) | DCNTL_STD);
			}
			return 1;
		}
		if (dstat & ~(DSTAT_SIR | DSTAT_DFE | DSTAT_SSI)) {
			printf("DMA IRQ:");
			if (dstat & DSTAT_IID)
				printf("Illegal instruction");
			if (dstat & DSTAT_ABRT)
				printf(" abort");
			if (dstat & DSTAT_BF)
				printf(" bus fault");
			if (dstat & DSTAT_MDPE)
				printf(" parity");
			if (dstat & DSTAT_DFE)
				printf(" dma fifo empty");
			printf(", DSP=0x%x DSA=0x%x: ",
			    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
				sc->sc_scriptaddr),
			    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA));
			if (siop_cmd)
				printf("last msg_in=0x%x status=0x%x\n",
				    siop_cmd->siop_tables.msg_in[0],
				    letoh32(siop_cmd->siop_tables.status));
			else 
				printf("%s: current DSA invalid\n",
				    sc->sc_dev.dv_xname);
			need_reset = 1;
		}
	}
	if (istat & ISTAT_SIP) {
		if (istat & ISTAT_DIP)
			delay(10);
		/*
		 * Can't read sist0 & sist1 independantly, or we have to
		 * insert delay
		 */
		sist = bus_space_read_2(sc->sc_rt, sc->sc_rh, SIOP_SIST0);
		sstat1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT1);
#ifdef SIOP_DEBUG_INTR
		printf("scsi interrupt, sist=0x%x sstat1=0x%x "
		    "DSA=0x%x DSP=0x%lx\n", sist,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT1),
		    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA),
		    (u_long)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
		    sc->sc_scriptaddr));
#endif
		if (sist & SIST0_RST) {
			siop_handle_reset(sc);
			siop_start(sc);
			/* no table to flush here */
			return 1;
		}
		if (sist & SIST0_SGE) {
			if (siop_cmd)
				sc_print_addr(xs->sc_link);
			else
				printf("%s:", sc->sc_dev.dv_xname);
			printf("scsi gross error\n");
			goto reset;
		}
		if ((sist & SIST0_MA) && need_reset == 0) {
			if (siop_cmd) { 
				int scratcha0;
				dstat = bus_space_read_1(sc->sc_rt, sc->sc_rh,
				    SIOP_DSTAT);
				/*
				 * first restore DSA, in case we were in a S/G
				 * operation.
				 */
				bus_space_write_4(sc->sc_rt, sc->sc_rh,
				    SIOP_DSA, siop_cmd->dsa);
				scratcha0 = bus_space_read_1(sc->sc_rt,
				    sc->sc_rh, SIOP_SCRATCHA);
				switch (sstat1 & SSTAT1_PHASE_MASK) {
				case SSTAT1_PHASE_STATUS:
				/*
				 * Previous phase may have aborted for any reason
				 * (for example, the target has less data to
				 * transfer than requested). Just go to status
				 * and the command should terminate.
				 */
					INCSTAT(siop_stat_intr_shortxfer);
					if ((dstat & DSTAT_DFE) == 0)
						siop_clearfifo(sc);
					/* no table to flush here */
					CALL_SCRIPT(Ent_status);
					return 1;
				case SSTAT1_PHASE_MSGIN:
					/*
					 * Target may be ready to disconnect.
					 * Save data pointers just in case.
					 */
					INCSTAT(siop_stat_intr_xferdisc);
					if (scratcha0 & A_flag_data)
						siop_sdp(siop_cmd);
					else if ((dstat & DSTAT_DFE) == 0)
						siop_clearfifo(sc);
					bus_space_write_1(sc->sc_rt, sc->sc_rh,
					    SIOP_SCRATCHA,
					    scratcha0 & ~A_flag_data);
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_msgin);
					return 1;
				}
				printf("%s: unexpected phase mismatch %d\n",
				    sc->sc_dev.dv_xname,
				    sstat1 & SSTAT1_PHASE_MASK);
			} else {
				printf("%s: phase mismatch without command\n",
				    sc->sc_dev.dv_xname);
			}
			need_reset = 1;
		}
		if (sist & SIST0_PAR) {
			/* parity error, reset */
			if (siop_cmd)
				sc_print_addr(xs->sc_link);
			else
				printf("%s:", sc->sc_dev.dv_xname);
			printf("parity error\n");
			goto reset;
		}
		if ((sist & (SIST1_STO << 8)) && need_reset == 0) {
			/* selection time out, assume there's no device here */
			if (siop_cmd) {
				siop_cmd->status = CMDST_DONE;
				xs->error = XS_SELTIMEOUT;
				freetarget = 1;
				goto end;
			} else {
				printf("%s: selection timeout without "
				    "command\n", sc->sc_dev.dv_xname);
				need_reset = 1;
			}
		}
		if (sist & SIST0_UDC) {
			/*
			 * unexpected disconnect. Usually the target signals
			 * a fatal condition this way. Attempt to get sense.
			 */
			 if (siop_cmd) {
				siop_cmd->siop_tables.status =
				    htole32(SCSI_CHECK);
				goto end;
			}
			printf("%s: unexpected disconnect without "
			    "command\n", sc->sc_dev.dv_xname);
			goto reset;
		}
		if (sist & (SIST1_SBMC << 8)) {
			/* SCSI bus mode change */
			if (siop_modechange(sc) == 0 || need_reset == 1)
				goto reset;
			if ((istat & ISTAT_DIP) && (dstat & DSTAT_SIR)) {
				/*
				 * We have a script interrupt. It will
				 * restart the script.
				 */
				goto scintr;
			}
			/*
			 * Else we have to restart the script ourself, at the
			 * interrupted instruction.
			 */
			bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP,
			    bus_space_read_4(sc->sc_rt, sc->sc_rh,
			    SIOP_DSP) - 8);
			return 1;
		}
		/* Else it's an unhandled exception (for now). */
		printf("%s: unhandled scsi interrupt, sist=0x%x sstat1=0x%x "
		    "DSA=0x%x DSP=0x%x\n", sc->sc_dev.dv_xname, sist,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT1),
		    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA),
		    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
		    sc->sc_scriptaddr));
		if (siop_cmd) {
			siop_cmd->status = CMDST_DONE;
			xs->error = XS_SELTIMEOUT;
			goto end;
		}
		need_reset = 1;
	}
	if (need_reset) {
reset:
		/* fatal error, reset the bus */
		siop_resetbus(sc);
		/* no table to flush here */
		return 1;
	}

scintr:
	if ((istat & ISTAT_DIP) && (dstat & DSTAT_SIR)) { /* script interrupt */
		irqcode = bus_space_read_4(sc->sc_rt, sc->sc_rh,
		    SIOP_DSPS);
#ifdef SIOP_DEBUG_INTR
		printf("script interrupt 0x%x\n", irqcode);
#endif
		/*
		 * no command, or an inactive command is only valid for a
		 * reselect interrupt
		 */
		if ((irqcode & 0x80) == 0) {
			if (siop_cmd == NULL) {
				printf("%s: script interrupt (0x%x) with "
				    "invalid DSA !!!\n", sc->sc_dev.dv_xname,
				    irqcode);
				goto reset;
			}
			if (siop_cmd->status != CMDST_ACTIVE &&
			    siop_cmd->status != CMDST_SENSE_ACTIVE) {
				printf("%s: command with invalid status "
				    "(IRQ code 0x%x current status %d) !\n",
				    sc->sc_dev.dv_xname,
				    irqcode, siop_cmd->status);
				xs = NULL;
			}
		}
		switch(irqcode) {
		case A_int_err:
			printf("error, DSP=0x%x\n",
			    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh,
			    SIOP_DSP) - sc->sc_scriptaddr));
			if (xs) {
				xs->error = XS_SELTIMEOUT;
				goto end;
			} else {
				goto reset;
			}
		case A_int_reseltarg:
			printf("%s: reselect with invalid target\n",
				    sc->sc_dev.dv_xname);
			goto reset;
		case A_int_resellun: 
			INCSTAT(siop_stat_intr_lunresel);
			target = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SCRATCHA) & 0xf;
			lun = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SCRATCHA + 1);
			tag = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SCRATCHA + 2);
			siop_target = sc->targets[target];
			if (siop_target == NULL) {
				printf("%s: reselect with invalid "
				    "target %d\n", sc->sc_dev.dv_xname, target);
				goto reset;
			}
			siop_lun = siop_target->siop_lun[lun];
			if (siop_lun == NULL) {
				printf("%s: target %d reselect with invalid "
				    "lun %d\n", sc->sc_dev.dv_xname,
				    target, lun);
				goto reset;
			}
			if (siop_lun->siop_tag[tag].active == NULL) {
				printf("%s: target %d lun %d tag %d reselect "
				    "without command\n", sc->sc_dev.dv_xname,
				    target, lun, tag);
				goto reset;
			}
			siop_cmd = siop_lun->siop_tag[tag].active;
			bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP,
			    siop_cmd->dsa + sizeof(struct siop_xfer_common) +
			    Ent_ldsa_reload_dsa);
			siop_table_sync(siop_cmd, BUS_DMASYNC_PREWRITE);
			return 1;
		case A_int_reseltag:
			printf("%s: reselect with invalid tag\n",
				    sc->sc_dev.dv_xname);
			goto reset;
		case A_int_msgin:
		{
			int msgin = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SFBR);
			if (msgin == MSG_MESSAGE_REJECT) {
				int msg, extmsg;
				if (siop_cmd->siop_tables.msg_out[0] & 0x80) {
					/*
					 * Message was part of an identify +
					 * something else. Identify shouldn't
					 * have been rejected.
					 */
					msg = siop_cmd->siop_tables.msg_out[1];
					extmsg =
					    siop_cmd->siop_tables.msg_out[3];
				} else {
					msg = siop_cmd->siop_tables.msg_out[0];
					extmsg =
					    siop_cmd->siop_tables.msg_out[2];
				}
				if (msg == MSG_MESSAGE_REJECT) {
					/* MSG_REJECT for a MSG_REJECT! */
					if (xs)
						sc_print_addr(xs->sc_link);
					else
						printf("%s: ",
						   sc->sc_dev.dv_xname);
					printf("our reject message was "
					    "rejected\n");
					goto reset;
				}
				if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_WDTR) {
					if ((siop_target->flags & TARF_SYNC)
					    == 0) {
						siop_target->status = TARST_OK;
						siop_print_info(sc, target);
						/* no table to flush here */
						CALL_SCRIPT(Ent_msgin_ack);
						return 1;
					}
					siop_target->status = TARST_SYNC_NEG;
					siop_sdtr_msg(siop_cmd, 0,
					    sc->min_st_sync, sc->maxoff);
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				} else if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_SDTR) {
					siop_target->status = TARST_OK;
					siop_print_info(sc, target);
					/* no table to flush here */
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				} else if (msg == MSG_SIMPLE_Q_TAG || 
				    msg == MSG_HEAD_OF_Q_TAG ||
				    msg == MSG_ORDERED_Q_TAG) {
					if (siop_handle_qtag_reject(
					    siop_cmd) == -1)
						goto reset;
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				}
				if (xs)
					sc_print_addr(xs->sc_link);
				else
					printf("%s: ", sc->sc_dev.dv_xname);
				if (msg == MSG_EXTENDED) {
					printf("scsi message reject, extended "
					    "message sent was 0x%x\n", extmsg);
				} else {
					printf("scsi message reject, message "
					    "sent was 0x%x\n", msg);
				}
				/* no table to flush here */
				CALL_SCRIPT(Ent_msgin_ack);
				return 1;
			}
			if (xs)
				sc_print_addr(xs->sc_link);
			else
				printf("%s: ", sc->sc_dev.dv_xname);
			printf("unhandled message 0x%x\n",
			    siop_cmd->siop_tables.msg_in[0]);
			siop_cmd->siop_tables.msg_out[0] = MSG_MESSAGE_REJECT;
			siop_cmd->siop_tables.t_msgout.count= htole32(1);
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return 1;
		}
		case A_int_extmsgin:
#ifdef SIOP_DEBUG_INTR
			printf("extended message: msg 0x%x len %d\n",
			    siop_cmd->siop_tables.msg_in[2], 
			    siop_cmd->siop_tables.msg_in[1]);
#endif
			if (siop_cmd->siop_tables.msg_in[1] > 6)
				printf("%s: extended message too big (%d)\n",
				    sc->sc_dev.dv_xname,
				    siop_cmd->siop_tables.msg_in[1]);
			siop_cmd->siop_tables.t_extmsgdata.count =
			    htole32(siop_cmd->siop_tables.msg_in[1] - 1);
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_get_extmsgdata);
			return 1;
		case A_int_extmsgdata:
		{
#ifdef SIOP_DEBUG_INTR
			{
			int i;
			printf("extended message: 0x%x, data:",
			    siop_cmd->siop_tables.msg_in[2]);
			for (i = 3; i < 2 + siop_cmd->siop_tables.msg_in[1];
			    i++)
				printf(" 0x%x",
				    siop_cmd->siop_tables.msg_in[i]);
			printf("\n");
			}
#endif
			int neg_action = SIOP_NEG_NOP;
			const char *neg_name = "";

			switch (siop_cmd->siop_tables.msg_in[2]) {
			case MSG_EXT_WDTR:
				neg_action = siop_wdtr_neg(siop_cmd);
				neg_name = "wdtr";
				break;
			case MSG_EXT_SDTR:
				neg_action = siop_sdtr_neg(siop_cmd);
				neg_name = "sdtr";
				break;
			case MSG_EXT_PPR:
				neg_action = siop_ppr_neg(siop_cmd);
				neg_name = "ppr";
				break;
			default:
				neg_action = SIOP_NEG_MSGREJ;
				break;
			}

			switch (neg_action) {
			case SIOP_NEG_NOP:
				break;
			case SIOP_NEG_MSGOUT:
				siop_update_scntl3(sc, siop_cmd->siop_target);
				siop_table_sync(siop_cmd,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				CALL_SCRIPT(Ent_send_msgout);
				break;
			case SIOP_NEG_ACK:
				siop_update_scntl3(sc, siop_cmd->siop_target);
				siop_table_sync(siop_cmd,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				CALL_SCRIPT(Ent_msgin_ack);
				break;
			case SIOP_NEG_MSGREJ:
				siop_cmd->siop_tables.msg_out[0] = MSG_MESSAGE_REJECT;
				siop_cmd->siop_tables.t_msgout.count = htole32(1);
				siop_table_sync(siop_cmd,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				CALL_SCRIPT(Ent_send_msgout);
				break;
			default:
				panic("invalid return value from siop_%s_neg(): 0x%x", neg_name, neg_action);
			}
			
			return (1);
		}

		case A_int_disc:
			INCSTAT(siop_stat_intr_sdp);
			offset = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SCRATCHA + 1);
#ifdef SIOP_DEBUG_DR
			printf("disconnect offset %d\n", offset);
#endif
			if (offset > SIOP_NSG) {
				printf("%s: bad offset for disconnect (%d)\n",
				    sc->sc_dev.dv_xname, offset);
				goto reset;
			}
			/* 
			 * offset == SIOP_NSG may be a valid condition if
			 * we get a sdp when the xfer is done.
			 * Don't call memmove in this case.
			 */
			if (offset < SIOP_NSG) {
				bcopy(&siop_cmd->siop_tables.data[offset],
				    &siop_cmd->siop_tables.data[0],
				    (SIOP_NSG - offset) * sizeof(struct scr_table));
				siop_table_sync(siop_cmd,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			}
			CALL_SCRIPT(Ent_script_sched);
			/* check if we can put some command in scheduler */
			siop_start(sc);
			return 1;
		case A_int_resfail:
			printf("reselect failed\n");
			CALL_SCRIPT(Ent_script_sched);
			return  1;
		case A_int_done:
			if (xs == NULL) {
				printf("%s: done without command, DSA=0x%lx\n",
				    sc->sc_dev.dv_xname, (u_long)siop_cmd->dsa);
				siop_cmd->status = CMDST_FREE;
				siop_start(sc);
				CALL_SCRIPT(Ent_script_sched);
				return 1;
			}
#ifdef SIOP_DEBUG_INTR
			printf("done, DSA=0x%lx target id 0x%x last msg "
			    "in=0x%x status=0x%x\n", (u_long)siop_cmd->dsa,
			    letoh32(siop_cmd->siop_tables.id),
			    siop_cmd->siop_tables.msg_in[0],
			    letoh32(siop_cmd->siop_tables.status));
#endif
			INCSTAT(siop_stat_intr_done);
			if (siop_cmd->status == CMDST_SENSE_ACTIVE)
				siop_cmd->status = CMDST_SENSE_DONE;
			else
				siop_cmd->status = CMDST_DONE;
			goto end;
		default:
			printf("unknown irqcode %x\n", irqcode);
			if (xs) {
				xs->error = XS_SELTIMEOUT;
				goto end;
			}
			goto reset;
		}
		return 1;
	}
	/* We can get here if ISTAT_DIP and DSTAT_DFE are the only bits set. */
	/* But that *SHOULDN'T* happen. It does on powerpc (at least).	     */
	printf("%s: siop_intr() - we should not be here!\n"
	    "   istat = 0x%x, dstat = 0x%x, sist = 0x%x, sstat1 = 0x%x\n"
	    "   need_reset = %x, irqcode = %x, siop_cmd %s\n",
	    sc->sc_dev.dv_xname,
	    istat, dstat, sist, sstat1, need_reset, irqcode,
	    (siop_cmd == NULL) ? "== NULL" : "!= NULL");
	goto reset; /* Where we should have gone in the first place! */
end:
	/*
	 * Restart the script now if command completed properly.
	 * Otherwise wait for siop_scsicmd_end(), it may need to put
	 * a cmd at the front of the queue.
	 */
	if (letoh32(siop_cmd->siop_tables.status) == SCSI_OK &&
	    TAILQ_FIRST(&sc->urgent_list) != NULL)
		CALL_SCRIPT(Ent_script_sched);
	else
		restart = 1;
	siop_scsicmd_end(siop_cmd);
	siop_lun->siop_tag[tag].active = NULL;
	if (siop_cmd->status == CMDST_FREE) {
		TAILQ_INSERT_TAIL(&sc->free_list, siop_cmd, next);
		siop_lun->lun_flags &= ~SIOP_LUNF_FULL;
		if (freetarget && siop_target->status == TARST_PROBING)
			siop_del_dev(sc, target, lun);
	}
	siop_start(sc);
	if (restart)
		CALL_SCRIPT(Ent_script_sched);
	return 1;
}

void
siop_scsicmd_end(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct scsi_xfer *xs = siop_cmd->xs;
	struct siop_softc *sc = siop_cmd->siop_sc;

	switch(letoh32(siop_cmd->siop_tables.status)) {
	case SCSI_OK:
		xs->error = (siop_cmd->status == CMDST_DONE) ?
		    XS_NOERROR : XS_SENSE;
		break;
	case SCSI_BUSY:
		xs->error = XS_BUSY;
		break;
	case SCSI_CHECK:
		if (siop_cmd->status == CMDST_SENSE_DONE) {
			/* request sense on a request sense ? */
			printf("request sense failed\n");
			xs->error = XS_DRIVER_STUFFUP;
		} else {
			siop_cmd->status = CMDST_SENSE;
		}
		break;
	case SCSI_QUEUE_FULL:
		{
		struct siop_lun *siop_lun = siop_cmd->siop_target->siop_lun[
		    xs->sc_link->lun];
		/*
		 * Device didn't queue the command. We have to retry
		 * it.  We insert it into the urgent list, hoping to
		 * preserve order.  But unfortunately, commands already
		 * in the scheduler may be accepted before this one.
		 * Also remember the condition, to avoid starting new
		 * commands for this device before one is done.
		 */
		INCSTAT(siop_stat_intr_qfull);
#ifdef SIOP_DEBUG
		printf("%s:%d:%d: queue full (tag %d)\n", sc->sc_dev.dv_xname,
		    xs->sc_link->target,
		    xs->sc_link->lun, siop_cmd->tag);
#endif
		timeout_del(&xs->stimeout);
		siop_lun->lun_flags |= SIOP_LUNF_FULL;
		siop_cmd->status = CMDST_READY;
		siop_setuptables(siop_cmd);
		TAILQ_INSERT_TAIL(&sc->urgent_list, siop_cmd, next);
		return;
		}
	case SCSI_SIOP_NOCHECK:
		/*
		 * don't check status, xs->error is already valid
		 */
		break;
	case SCSI_SIOP_NOSTATUS:
		/*
		 * the status byte was not updated, cmd was
		 * aborted
		 */
		xs->error = XS_SELTIMEOUT;
		break;
	default:
		xs->error = XS_DRIVER_STUFFUP;
	}
	if (siop_cmd->status != CMDST_SENSE_DONE &&
	    xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data,
		    0, siop_cmd->dmamap_data->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_data);
	}
	bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_cmd);
	if (siop_cmd->status == CMDST_SENSE) {
		/* issue a request sense for this target */
		int error;
		siop_cmd->rs_cmd.opcode = REQUEST_SENSE;
		siop_cmd->rs_cmd.byte2 = xs->sc_link->lun << 5;
		siop_cmd->rs_cmd.unused[0] = siop_cmd->rs_cmd.unused[1] = 0;
		siop_cmd->rs_cmd.length = sizeof(struct scsi_sense_data);
		siop_cmd->rs_cmd.control = 0;
		siop_cmd->flags &= ~CMDFL_TAG;
		error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_cmd,
		    &siop_cmd->rs_cmd, sizeof(struct scsi_sense),
		    NULL, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to load cmd DMA map "
			    "(for SENSE): %d\n",
			    sc->sc_dev.dv_xname, error);
			xs->error = XS_DRIVER_STUFFUP;
			goto out;
		}
		error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_data,
		    &xs->sense, sizeof(struct scsi_sense_data),
		    NULL, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to load data DMA map "
			    "(for SENSE): %d\n",
			    sc->sc_dev.dv_xname, error);
			xs->error = XS_DRIVER_STUFFUP;
			bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_cmd);
			goto out;
		}
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data,
		    0, siop_cmd->dmamap_data->dm_mapsize,
		    BUS_DMASYNC_PREREAD);
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_cmd,
		    0, siop_cmd->dmamap_cmd->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		siop_setuptables(siop_cmd);
		/* arrange for the cmd to be handled now */
		TAILQ_INSERT_HEAD(&sc->urgent_list, siop_cmd, next);
		return;
	} else if (siop_cmd->status == CMDST_SENSE_DONE) {
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data,
		    0, siop_cmd->dmamap_data->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_data);
	}
out:
	timeout_del(&siop_cmd->xs->stimeout);
	siop_cmd->status = CMDST_FREE;
	xs->flags |= ITSDONE;
	xs->resid = 0;
	scsi_done(xs);
}

/*
 * handle a rejected queue tag message: the command will run untagged,
 * has to adjust the reselect script.
 */
int
siop_handle_qtag_reject(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct siop_softc *sc = siop_cmd->siop_sc;
	int target = siop_cmd->xs->sc_link->target;
	int lun = siop_cmd->xs->sc_link->lun;
	int tag = siop_cmd->siop_tables.msg_out[2];
	struct siop_lun *siop_lun = sc->targets[target]->siop_lun[lun];

#ifdef SIOP_DEBUG
	printf("%s:%d:%d: tag message %d (%d) rejected (status %d)\n",
	    sc->sc_dev.dv_xname, target, lun, tag, siop_cmd->tag,
	    siop_cmd->status);
#endif

	if (siop_lun->siop_tag[0].active != NULL) {
		printf("%s: untagged command already running for target %d "
		    "lun %d (status %d)\n", sc->sc_dev.dv_xname, target, lun,
		    siop_lun->siop_tag[0].active->status);
		return -1;
	}
	/* clear tag slot */
	siop_lun->siop_tag[tag].active = NULL;
	/* add command to non-tagged slot */
	siop_lun->siop_tag[0].active = siop_cmd;
	siop_cmd->tag = 0;
	/* adjust reselect script if there is one */
	if (siop_lun->siop_tag[0].reseloff > 0) {
		siop_script_write(sc,
		    siop_lun->siop_tag[0].reseloff + 1,
		    siop_cmd->dsa + sizeof(struct siop_xfer_common) +
		    Ent_ldsa_reload_dsa);
		siop_table_sync(siop_cmd, BUS_DMASYNC_PREWRITE);
	}
	return 0;
}

/*
 * Handle a bus reset: reset chip, unqueue all active commands, free all
 * target structs and report losage to upper layer.
 * As the upper layer may requeue immediately we have to first store
 * all active commands in a temporary queue.
 */
void
siop_handle_reset(sc)
	struct siop_softc *sc;
{
	struct cmd_list reset_list;
	struct siop_cmd *siop_cmd, *next_siop_cmd;
	struct siop_lun *siop_lun;
	int target, lun, tag;
	/*
	 * SCSI bus reset. Reset the chip and restart
	 * the queue. Need to clean up all active commands.
	 */
	printf("%s: scsi bus reset\n", sc->sc_dev.dv_xname);
	/* stop, reset and restart the chip */
	siop_reset(sc);
	TAILQ_INIT(&reset_list);
	/*
	 * Process all commands: first commmands being executed
	 */
	for (target = 0; target < sc->sc_link.adapter_buswidth;
	    target++) {
		if (sc->targets[target] == NULL)
			continue;
		for (lun = 0; lun < 8; lun++) {
			siop_lun = sc->targets[target]->siop_lun[lun];
			if (siop_lun == NULL)
				continue;
			siop_lun->lun_flags &= ~SIOP_LUNF_FULL;
			for (tag = 0; tag <
			    ((sc->targets[target]->flags & TARF_TAG) ?
			    SIOP_NTAG : 1);
			    tag++) {
				siop_cmd = siop_lun->siop_tag[tag].active;
				if (siop_cmd == NULL)
					continue;
				printf("cmd %p (target %d:%d) in reset list\n",
				    siop_cmd, target, lun);
				TAILQ_INSERT_TAIL(&reset_list, siop_cmd, next);
				siop_lun->siop_tag[tag].active = NULL;
			}
		}
		sc->targets[target]->status = TARST_ASYNC;
		sc->targets[target]->flags  = 0;
	}
	/* Next commands from the urgent list */
	for (siop_cmd = TAILQ_FIRST(&sc->urgent_list); siop_cmd != NULL;
	    siop_cmd = next_siop_cmd) {
		next_siop_cmd = TAILQ_NEXT(siop_cmd, next);
		siop_cmd->flags &= ~CMDFL_TAG;
		printf("cmd %p (target %d:%d) in reset list (wait)\n",
		    siop_cmd, siop_cmd->xs->sc_link->target,
		    siop_cmd->xs->sc_link->lun);
		TAILQ_REMOVE(&sc->urgent_list, siop_cmd, next);
		TAILQ_INSERT_TAIL(&reset_list, siop_cmd, next);
	}
	/* Then command waiting in the input list */
	for (siop_cmd = TAILQ_FIRST(&sc->ready_list); siop_cmd != NULL;
	    siop_cmd = next_siop_cmd) {
		next_siop_cmd = TAILQ_NEXT(siop_cmd, next);
		siop_cmd->flags &= ~CMDFL_TAG;
		printf("cmd %p (target %d:%d) in reset list (wait)\n",
		    siop_cmd, siop_cmd->xs->sc_link->target,
		    siop_cmd->xs->sc_link->lun);
		TAILQ_REMOVE(&sc->ready_list, siop_cmd, next);
		TAILQ_INSERT_TAIL(&reset_list, siop_cmd, next);
	}

	for (siop_cmd = TAILQ_FIRST(&reset_list); siop_cmd != NULL;
	    siop_cmd = next_siop_cmd) {
		next_siop_cmd = TAILQ_NEXT(siop_cmd, next);
		siop_cmd->xs->error = (siop_cmd->flags & CMDFL_TIMEOUT) ?
		    XS_TIMEOUT : XS_RESET;
		siop_cmd->siop_tables.status = htole32(SCSI_SIOP_NOCHECK);
		printf("cmd %p (status %d) about to be processed\n", siop_cmd,
		    siop_cmd->status);
		if (siop_cmd->status == CMDST_SENSE ||
		    siop_cmd->status == CMDST_SENSE_ACTIVE) 
			siop_cmd->status = CMDST_SENSE_DONE;
		else
			siop_cmd->status = CMDST_DONE;
		TAILQ_REMOVE(&reset_list, siop_cmd, next);
		siop_scsicmd_end(siop_cmd);
		TAILQ_INSERT_TAIL(&sc->free_list, siop_cmd, next);
	}
}

int
siop_scsicmd(xs)
	struct scsi_xfer *xs;
{
	struct siop_softc *sc = (struct siop_softc *)xs->sc_link->adapter_softc;
	struct siop_cmd *siop_cmd;
	int s, error, i, j;
	const int target = xs->sc_link->target;
	const int lun = xs->sc_link->lun;

	s = splbio();
#ifdef SIOP_DEBUG_SCHED
	printf("starting cmd 0x%02x for %d:%d\n", xs->cmd->opcode, target, lun);
#endif
	siop_cmd = TAILQ_FIRST(&sc->free_list);
	if (siop_cmd != NULL) {
		TAILQ_REMOVE(&sc->free_list, siop_cmd, next);
	} else {
		xs->error = XS_DRIVER_STUFFUP;
		splx(s);
		return(TRY_AGAIN_LATER);
	}

	/* Always reset xs->stimeout, lest we timeout_del() with trash */
	timeout_set(&xs->stimeout, siop_timeout, siop_cmd);

#ifdef DIAGNOSTIC
	if (siop_cmd->status != CMDST_FREE)
		panic("siop_scsicmd: new cmd not free");
#endif
	if (sc->targets[target] == NULL) {
#ifdef SIOP_DEBUG
		printf("%s: alloc siop_target for target %d\n",
			sc->sc_dev.dv_xname, target);
#endif
		sc->targets[target] =
		    malloc(sizeof(struct siop_target), M_DEVBUF, M_NOWAIT);
		if (sc->targets[target] == NULL) {
			printf("%s: can't malloc memory for target %d\n",
			    sc->sc_dev.dv_xname, target);
			xs->error = XS_DRIVER_STUFFUP;
			splx(s);
			return(TRY_AGAIN_LATER);
		}
		sc->targets[target]->status = TARST_PROBING;
		sc->targets[target]->flags  = 0;
		sc->targets[target]->id = sc->clock_div << 24; /* scntl3 */
		sc->targets[target]->id |=  target << 16; /* id */
		/* sc->targets[target]->id |= 0x0 << 8; scxfer is 0 */

		/* get a lun switch script */
		sc->targets[target]->lunsw = siop_get_lunsw(sc);
		if (sc->targets[target]->lunsw == NULL) {
			printf("%s: can't alloc lunsw for target %d\n",
			    sc->sc_dev.dv_xname, target);
			xs->error = XS_DRIVER_STUFFUP;
			splx(s);
			return(TRY_AGAIN_LATER);
		}
		for (i=0; i < 8; i++)
			sc->targets[target]->siop_lun[i] = NULL;
		siop_add_reselsw(sc, target);
	}

	if (sc->targets[target]->siop_lun[lun] == NULL) {
		sc->targets[target]->siop_lun[lun] =
		    malloc(sizeof(struct siop_lun), M_DEVBUF, M_NOWAIT);
		if (sc->targets[target]->siop_lun[lun] == NULL) {
			printf("%s: can't alloc siop_lun for target %d "
			    "lun %d\n", sc->sc_dev.dv_xname, target, lun);
			xs->error = XS_DRIVER_STUFFUP;
			splx(s);
			return(TRY_AGAIN_LATER);
		}
		memset(sc->targets[target]->siop_lun[lun], 0,
		    sizeof(struct siop_lun));
	}
	siop_cmd->siop_target = sc->targets[target];
	siop_cmd->xs = xs;
	siop_cmd->flags = 0;
	siop_cmd->status = CMDST_READY;

	/* load the DMA maps */
	error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_cmd,
	    xs->cmd, xs->cmdlen, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to load cmd DMA map: %d\n",
		    sc->sc_dev.dv_xname, error);
		xs->error = XS_DRIVER_STUFFUP;
		splx(s);
		return(TRY_AGAIN_LATER);
	}
	if (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
		error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_data,
		    xs->data, xs->datalen, NULL, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to load cmd DMA map: %d\n",
			    sc->sc_dev.dv_xname, error);
			xs->error = XS_DRIVER_STUFFUP;
			bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_cmd);
			splx(s);
			return(TRY_AGAIN_LATER);
		}
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data,
		    0, siop_cmd->dmamap_data->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ?
		    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	}
	bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_cmd,
	    0, siop_cmd->dmamap_cmd->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	siop_setuptables(siop_cmd);

	TAILQ_INSERT_TAIL(&sc->ready_list, siop_cmd, next);

	siop_start(sc);
	if (xs->flags & SCSI_POLL) {
		/* poll for command completion */
		for(i = xs->timeout; i > 0; i--) {
			siop_intr(sc);
			if (xs->flags & ITSDONE) {
				if ((xs->cmd->opcode == INQUIRY)
				    && (xs->error == XS_NOERROR)) {
					error = ((struct scsi_inquiry_data *)xs->data)->device & SID_QUAL;
					if (error != SID_QUAL_BAD_LU) {
						/* 
						 * Allocate enough commands to hold at least max openings
						 * worth of commands. Do this statically now because 
						 * a) We can't rely on the upper layers to ask for more
						 * b) Doing it dynamically in siop_startcmd may cause 
						 *    calls to bus_dma* functions in interrupt context
						 */
						for (j = 0; j < SIOP_NTAG; j += SIOP_NCMDPB)
							siop_morecbd(sc);
						if (sc->targets[target]->status == TARST_PROBING)
							sc->targets[target]->status = TARST_ASYNC;
						/* Can't do lun 0 here, because flags not set yet */
						if (lun > 0)
							siop_add_dev(sc, target, lun);
					}
				}
				break;
			}
			delay(1000);
		}
		splx(s);
		if (i == 0) {
			siop_timeout(siop_cmd);
			while ((xs->flags & ITSDONE) == 0) {
				s = splbio();
				siop_intr(sc);
				splx(s);
			}
		}
		return (COMPLETE);
	}
	splx(s);
	return (SUCCESSFULLY_QUEUED);
}

void
siop_start(sc)
	struct siop_softc *sc;
{
	struct siop_cmd *siop_cmd, *next_siop_cmd;
	struct siop_lun *siop_lun;
	u_int32_t dsa;
	int timeout;
	int target, lun, tag, slot;
	int newcmd = 0; 
	int doingready = 0;

	/*
	 * first make sure to read valid data
	 */
	siop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * The queue management here is a bit tricky: the script always looks
	 * at the slots from first to last, so if we always use the first 
	 * free slot commands can stay at the tail of the queue ~forever.
	 * The algorithm used here is to restart from the head when we know
	 * that the queue is empty, and only add commands after the last one.
	 * When we're at the end of the queue wait for the script to clear it.
	 * The best thing to do here would be to implement a circular queue,
	 * but using only 53c720 features this can be "interesting".
	 * A mid-way solution could be to implement 2 queues and swap orders.
	 */
	slot = sc->sc_currschedslot;
	/*
	 * If the instruction is 0x80000000 (JUMP foo, IF FALSE) the slot is
	 * free. As this is the last used slot, all previous slots are free,
	 * we can restart from 1.
	 * slot 0 is reserved for request sense commands.
	 */
	if (siop_script_read(sc, (Ent_script_sched_slot0 / 4) + slot * 2) ==
	    0x80000000) {
		slot = sc->sc_currschedslot = 1;
	} else {
		slot++;
	}
	/* first handle commands from the urgent list */
	siop_cmd = TAILQ_FIRST(&sc->urgent_list);
again:
	for (; siop_cmd != NULL; siop_cmd = next_siop_cmd) {
		next_siop_cmd = TAILQ_NEXT(siop_cmd, next);
#ifdef DIAGNOSTIC
		if (siop_cmd->status != CMDST_READY &&
		    siop_cmd->status != CMDST_SENSE)
			panic("siop: non-ready cmd in ready list");
#endif	
		target = siop_cmd->xs->sc_link->target;
		lun = siop_cmd->xs->sc_link->lun;
		siop_lun = sc->targets[target]->siop_lun[lun];
		/* if non-tagged command active, wait */
		if (siop_lun->siop_tag[0].active != NULL)
			continue;
		/*
		 * if we're in a queue full condition don't start a new
		 * command, unless it's a request sense
		 */
		if ((siop_lun->lun_flags & SIOP_LUNF_FULL) &&
		    siop_cmd->status == CMDST_READY)
			continue;
		/* find a free tag if needed */
		if (siop_cmd->flags & CMDFL_TAG) {
			for (tag = 1; tag < SIOP_NTAG; tag++) {
				if (siop_lun->siop_tag[tag].active == NULL)
					break;
			}
			if (tag == SIOP_NTAG) /* no free tag */
				continue;
		} else {
			tag = 0;
		}
		siop_cmd->tag = tag;
		/*
		 * find a free scheduler slot and load it. If it's a request
		 * sense we need to use slot 0.
		 */
		if (siop_cmd->status != CMDST_SENSE) {
			for (; slot < SIOP_NSLOTS; slot++) {
				/*
				 * If cmd if 0x80000000 the slot is free
				 */
				if (siop_script_read(sc,
				    (Ent_script_sched_slot0 / 4) + slot * 2) ==
				    0x80000000)
					break;
			}
			/* no more free slots, no need to continue */
			if (slot == SIOP_NSLOTS) {
				goto end;
			}
		} else {
			slot = 0;
			if (siop_script_read(sc, Ent_script_sched_slot0 / 4)
			    != 0x80000000) 
				goto end;
		}

#ifdef SIOP_DEBUG_SCHED
		printf("using slot %d for DSA 0x%lx\n", slot,
		    (u_long)siop_cmd->dsa);
#endif
		/* Ok, we can add the tag message */
		if (tag > 0) {
#ifdef DIAGNOSTIC
			int msgcount =
			    letoh32(siop_cmd->siop_tables.t_msgout.count);
			if (msgcount != 1)
				printf("%s:%d:%d: tag %d with msgcount %d\n",
				    sc->sc_dev.dv_xname, target, lun, tag,
				    msgcount);
#endif
			if (siop_cmd->xs->bp != NULL &&
			    (siop_cmd->xs->bp->b_flags & B_ASYNC))
				siop_cmd->siop_tables.msg_out[1] =
				    MSG_SIMPLE_Q_TAG;
			else
				siop_cmd->siop_tables.msg_out[1] =
				    MSG_ORDERED_Q_TAG;
			siop_cmd->siop_tables.msg_out[2] = tag;
			siop_cmd->siop_tables.t_msgout.count = htole32(3);
		}
		/* note that we started a new command */
		newcmd = 1;
		/* mark command as active */
		if (siop_cmd->status == CMDST_READY) {
			siop_cmd->status = CMDST_ACTIVE;
		} else if (siop_cmd->status == CMDST_SENSE) {
			siop_cmd->status = CMDST_SENSE_ACTIVE;
		} else
			panic("siop_start: bad status");
		if (doingready)
			TAILQ_REMOVE(&sc->ready_list, siop_cmd, next);
		else
			TAILQ_REMOVE(&sc->urgent_list, siop_cmd, next);
		siop_lun->siop_tag[tag].active = siop_cmd;
		/* patch scripts with DSA addr */
		dsa = siop_cmd->dsa;
		/* first reselect switch, if we have an entry */
		if (siop_lun->siop_tag[tag].reseloff > 0)
			siop_script_write(sc,
			    siop_lun->siop_tag[tag].reseloff + 1,
			    dsa + sizeof(struct siop_xfer_common) +
			    Ent_ldsa_reload_dsa);
		/* CMD script: MOVE MEMORY addr */
		siop_cmd->siop_xfer->resel[E_ldsa_abs_slot_Used[0]] = 
		   htole32(sc->sc_scriptaddr + Ent_script_sched_slot0 +
		   slot * 8);
		siop_table_sync(siop_cmd, BUS_DMASYNC_PREWRITE);
		/* scheduler slot: JUMP ldsa_select */
		siop_script_write(sc,
		    (Ent_script_sched_slot0 / 4) + slot * 2 + 1,
		    dsa + sizeof(struct siop_xfer_common) + Ent_ldsa_select);
		/* handle timeout */
		if (siop_cmd->status == CMDST_ACTIVE) {
			if ((siop_cmd->xs->flags & SCSI_POLL) == 0) {
				/* start expire timer */
				timeout = (u_int64_t) siop_cmd->xs->timeout *
				    (u_int64_t)hz / 1000;
				if (timeout == 0)
					timeout = 1;
				timeout_add(&siop_cmd->xs->stimeout, timeout);
			}
		}
		/*
		 * Change JUMP cmd so that this slot will be handled
		 */
		siop_script_write(sc, (Ent_script_sched_slot0 / 4) + slot * 2,
		    0x80080000);
		/* if we're using the request sense slot, stop here */
		if (slot == 0)
			goto end;
		sc->sc_currschedslot = slot;
		slot++;
	}
	if (doingready == 0) {
		/* now process ready list */
		doingready = 1;
		siop_cmd = TAILQ_FIRST(&sc->ready_list);
		goto again;
	}

end:
	/* if nothing changed no need to flush cache and wakeup script */
	if (newcmd == 0)
		return;
	/* make sure SCRIPT processor will read valid data */
	siop_script_sync(sc,BUS_DMASYNC_PREREAD |  BUS_DMASYNC_PREWRITE);
	/* Signal script it has some work to do */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_SIGP);
	/* and wait for IRQ */
	return;
}

void
siop_timeout(v)
	void *v;
{
	struct siop_cmd *siop_cmd = v;
	struct siop_softc *sc = siop_cmd->siop_sc;
	int s;

	sc_print_addr(siop_cmd->xs->sc_link);
	printf("timeout on SCSI command 0x%x\n", siop_cmd->xs->cmd->opcode);

	s = splbio();
	/* reset the scsi bus */
	siop_resetbus(sc);

	/* deactivate callout */
	timeout_del(&siop_cmd->xs->stimeout);
	/*
	 * Mark command as being timed out and just return. The bus
	 * reset will generate an interrupt, which will be handled
	 * in siop_intr().
	 */
	siop_cmd->flags |= CMDFL_TIMEOUT;
	splx(s);
	return;

}

void
siop_dump_script(sc)
	struct siop_softc *sc;
{
	int i;
	for (i = 0; i < PAGE_SIZE / 4; i += 2) {
		printf("0x%04x: 0x%08x 0x%08x", i * 4,
		    letoh32(sc->sc_script[i]), letoh32(sc->sc_script[i+1]));
		if ((letoh32(sc->sc_script[i]) & 0xe0000000) == 0xc0000000) {
			i++;
			printf(" 0x%08x", letoh32(sc->sc_script[i+1]));
		}
		printf("\n");
	}
}

int
siop_morecbd(sc)
	struct siop_softc *sc;
{
	int error, i, j;
	bus_dma_segment_t seg;
	int rseg;
	struct siop_cbd *newcbd;
	bus_addr_t dsa;
	u_int32_t *scr;

	/* allocate a new list head */
	newcbd = malloc(sizeof(struct siop_cbd), M_DEVBUF, M_NOWAIT);
	if (newcbd == NULL) {
		printf("%s: can't allocate memory for command descriptors "
		    "head\n", sc->sc_dev.dv_xname);
		return ENOMEM;
	}
	memset(newcbd, 0, sizeof(struct siop_cbd));

	/* allocate cmd list */
	newcbd->cmds =
	    malloc(sizeof(struct siop_cmd) * SIOP_NCMDPB, M_DEVBUF, M_NOWAIT);
	if (newcbd->cmds == NULL) {
		printf("%s: can't allocate memory for command descriptors\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto bad3;
	}
	memset(newcbd->cmds, 0, sizeof(struct siop_cmd) * SIOP_NCMDPB);
	error = bus_dmamem_alloc(sc->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0, &seg,
	    1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to allocate cbd DMA memory, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto bad2;
	}
	error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, PAGE_SIZE,
	    (caddr_t *)&newcbd->xfers, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		printf("%s: unable to map cbd DMA memory, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto bad2;
	}
	error = bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT, &newcbd->xferdma);
	if (error) {
		printf("%s: unable to create cbd DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto bad1;
	}
	error = bus_dmamap_load(sc->sc_dmat, newcbd->xferdma, newcbd->xfers,
	    PAGE_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to load cbd DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto bad0;
	}
#ifdef DEBUG
	printf("%s: alloc newcdb at PHY addr 0x%lx\n", sc->sc_dev.dv_xname,
	    (unsigned long)newcbd->xferdma->dm_segs[0].ds_addr);
#endif

	for (i = 0; i < SIOP_NCMDPB; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MAXPHYS, SIOP_NSG,
		    MAXPHYS, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &newcbd->cmds[i].dmamap_data);
		if (error) {
			printf("%s: unable to create data DMA map for cbd: "
			    "error %d\n",
			    sc->sc_dev.dv_xname, error);
			goto bad0;
		}
		error = bus_dmamap_create(sc->sc_dmat,
		    sizeof(struct scsi_generic), 1,
		    sizeof(struct scsi_generic), 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &newcbd->cmds[i].dmamap_cmd);
		if (error) {
			printf("%s: unable to create cmd DMA map for cbd %d\n",
			    sc->sc_dev.dv_xname, error);
			goto bad0;
		}
		newcbd->cmds[i].siop_sc = sc;
		newcbd->cmds[i].siop_cbdp = newcbd;
		newcbd->cmds[i].siop_xfer = &newcbd->xfers[i];
		memset(newcbd->cmds[i].siop_xfer, 0,
		    sizeof(struct siop_xfer));
		newcbd->cmds[i].dsa = newcbd->xferdma->dm_segs[0].ds_addr +
		    i * sizeof(struct siop_xfer);
		dsa = newcbd->cmds[i].dsa;
		newcbd->cmds[i].status = CMDST_FREE;
		newcbd->cmds[i].siop_tables.t_msgout.count= htole32(1);
		newcbd->cmds[i].siop_tables.t_msgout.addr = htole32(dsa);
		newcbd->cmds[i].siop_tables.t_msgin.count= htole32(1);
		newcbd->cmds[i].siop_tables.t_msgin.addr = htole32(dsa + 16);
		newcbd->cmds[i].siop_tables.t_extmsgin.count= htole32(2);
		newcbd->cmds[i].siop_tables.t_extmsgin.addr = htole32(dsa + 17);
		newcbd->cmds[i].siop_tables.t_extmsgdata.addr =
		    htole32(dsa + 19);
		newcbd->cmds[i].siop_tables.t_status.count= htole32(1);
		newcbd->cmds[i].siop_tables.t_status.addr = htole32(dsa + 32);

		/* The select/reselect script */
		scr = &newcbd->cmds[i].siop_xfer->resel[0];
		for (j = 0; j < sizeof(load_dsa) / sizeof(load_dsa[0]); j++)
			scr[j] = htole32(load_dsa[j]);
		/*
		 * 0x78000000 is a 'move data8 to reg'. data8 is the second
		 * octet, reg offset is the third.
		 */
		scr[Ent_rdsa0 / 4] =
		    htole32(0x78100000 | ((dsa & 0x000000ff) <<  8));
		scr[Ent_rdsa1 / 4] =
		    htole32(0x78110000 | ( dsa & 0x0000ff00       ));
		scr[Ent_rdsa2 / 4] =
		    htole32(0x78120000 | ((dsa & 0x00ff0000) >>  8));
		scr[Ent_rdsa3 / 4] =
		    htole32(0x78130000 | ((dsa & 0xff000000) >> 16));
		scr[E_ldsa_abs_reselected_Used[0]] =
		    htole32(sc->sc_scriptaddr + Ent_reselected);
		scr[E_ldsa_abs_reselect_Used[0]] =
		    htole32(sc->sc_scriptaddr + Ent_reselect);
		scr[E_ldsa_abs_selected_Used[0]] =
		    htole32(sc->sc_scriptaddr + Ent_selected);
		scr[E_ldsa_abs_data_Used[0]] =
		    htole32(dsa + sizeof(struct siop_xfer_common) +
		    Ent_ldsa_data);
		/* JUMP foo, IF FALSE - used by MOVE MEMORY to clear the slot */
		scr[Ent_ldsa_data / 4] = htole32(0x80000000);
		TAILQ_INSERT_TAIL(&sc->free_list, &newcbd->cmds[i], next);
#ifdef SIOP_DEBUG
		printf("tables[%d]: in=0x%x out=0x%x status=0x%x\n", i,
		    letoh32(newcbd->cmds[i].siop_tables.t_msgin.addr),
		    letoh32(newcbd->cmds[i].siop_tables.t_msgout.addr),
		    letoh32(newcbd->cmds[i].siop_tables.t_status.addr));
#endif
	}
	TAILQ_INSERT_TAIL(&sc->cmds, newcbd, next);
	return 0;
bad0:
	bus_dmamap_destroy(sc->sc_dmat, newcbd->xferdma);
bad1:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
bad2:
	free(newcbd->cmds, M_DEVBUF);
bad3:
	free(newcbd, M_DEVBUF);
	return error;
}

struct siop_lunsw *
siop_get_lunsw(sc)
	struct siop_softc *sc;
{
	struct siop_lunsw *lunsw;
	int i;

	if (sc->script_free_lo + (sizeof(lun_switch) / sizeof(lun_switch[0])) >=
	    sc->script_free_hi)
		return NULL;
	lunsw = TAILQ_FIRST(&sc->lunsw_list);
	if (lunsw != NULL) {
#ifdef SIOP_DEBUG
		printf("siop_get_lunsw got lunsw at offset %d\n",
		    lunsw->lunsw_off);
#endif
		TAILQ_REMOVE(&sc->lunsw_list, lunsw, next);
		return lunsw;
	}
	lunsw = malloc(sizeof(struct siop_lunsw), M_DEVBUF, M_NOWAIT);
	if (lunsw == NULL)
		return NULL;
	memset(lunsw, 0, sizeof(struct siop_lunsw));
#ifdef SIOP_DEBUG
	printf("allocating lunsw at offset %d\n", sc->script_free_lo);
#endif
	if (sc->features & SF_CHIP_RAM) {
		bus_space_write_region_4(sc->sc_ramt, sc->sc_ramh,
		    sc->script_free_lo * 4, lun_switch,
		    sizeof(lun_switch) / sizeof(lun_switch[0]));
		bus_space_write_4(sc->sc_ramt, sc->sc_ramh,
		    (sc->script_free_lo + E_abs_lunsw_return_Used[0]) * 4,
		    sc->sc_scriptaddr + Ent_lunsw_return);
	} else {
		for (i = 0; i < sizeof(lun_switch) / sizeof(lun_switch[0]);
		    i++)
			sc->sc_script[sc->script_free_lo + i] =
			    htole32(lun_switch[i]);
		sc->sc_script[sc->script_free_lo + E_abs_lunsw_return_Used[0]] = 
		    htole32(sc->sc_scriptaddr + Ent_lunsw_return);
	}
	lunsw->lunsw_off = sc->script_free_lo;
	lunsw->lunsw_size = sizeof(lun_switch) / sizeof(lun_switch[0]);
	sc->script_free_lo += lunsw->lunsw_size;
	if (sc->script_free_lo > 1024)
		printf("%s: script_free_lo (%d) > 1024\n", sc->sc_dev.dv_xname,
		    sc->script_free_lo);
	siop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return lunsw;
}

void
siop_add_reselsw(sc, target)
	struct siop_softc *sc;
	int target;
{
	int i;
	struct siop_lun *siop_lun;
	/*
	 * add an entry to resel switch
	 */
	siop_script_sync(sc, BUS_DMASYNC_POSTWRITE);
	for (i = 0; i < 15; i++) {
		sc->targets[target]->reseloff = Ent_resel_targ0 / 4 + i * 2;
		if ((siop_script_read(sc, sc->targets[target]->reseloff) & 0xff)
		    == 0xff) { /* it's free */
#ifdef SIOP_DEBUG
			printf("siop: target %d slot %d offset %d\n",
			    target, i, sc->targets[target]->reseloff);
#endif
			/* JUMP abs_foo, IF target | 0x80; */
			siop_script_write(sc, sc->targets[target]->reseloff,
			    0x800c0080 | target);
			siop_script_write(sc, sc->targets[target]->reseloff + 1,
			    sc->sc_scriptaddr +
			    sc->targets[target]->lunsw->lunsw_off * 4 +
			    Ent_lun_switch_entry);
			break;
		}
	}
	if (i == 15) /* no free slot, shouldn't happen */
		panic("siop: resel switch full");

	sc->sc_ntargets++;
	for (i = 0; i < 8; i++) {
		siop_lun = sc->targets[target]->siop_lun[i];
		if (siop_lun == NULL)
			continue;
		if (siop_lun->reseloff > 0) {
			siop_lun->reseloff = 0;
			siop_add_dev(sc, target, i);
		}
	}
	siop_update_scntl3(sc, sc->targets[target]);
	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
}

void
siop_update_scntl3(sc, siop_target)
	struct siop_softc *sc;
	struct siop_target *siop_target;
{
	/* MOVE target->id >> 24 TO SCNTL3 */
	siop_script_write(sc,
	    siop_target->lunsw->lunsw_off + (Ent_restore_scntl3 / 4),
	    0x78030000 | ((siop_target->id >> 16) & 0x0000ff00));
	/* MOVE target->id >> 8 TO SXFER */
	siop_script_write(sc,
	    siop_target->lunsw->lunsw_off + (Ent_restore_scntl3 / 4) + 2,
	    0x78050000 | (siop_target->id & 0x0000ff00));
	/* If DT, change null op ('MOVE 0xff TO SFBR') to 'MOVE n TO SCNTL4' */
	if (siop_target->flags & TARF_ISDT)
		siop_script_write(sc,
		    siop_target->lunsw->lunsw_off + (Ent_restore_scntl3 / 4) + 4,
		    0x78bc0000 | ((siop_target->id << 8) & 0x0000ff00));
	else
		siop_script_write(sc,
		    siop_target->lunsw->lunsw_off + (Ent_restore_scntl3 / 4) + 4,
		    0x7808ff00);

	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
}

void
siop_add_dev(sc, target, lun)
	struct siop_softc *sc;
	int target;
	int lun;
{
	struct siop_lunsw *lunsw;
	struct siop_lun *siop_lun = sc->targets[target]->siop_lun[lun];
	int i, ntargets;

	if (siop_lun->reseloff > 0)
		return;
	lunsw = sc->targets[target]->lunsw;
	if ((lunsw->lunsw_off + lunsw->lunsw_size) < sc->script_free_lo) {
		/*
		 * Can't extend this slot. Probably not worth trying to deal
		 * with this case.
		 */
#ifdef DEBUG
		printf("%s:%d:%d: can't allocate a lun sw slot\n",
		    sc->sc_dev.dv_xname, target, lun);
#endif
		return;
	}
	/* count how many free targets we still have to probe */
	ntargets =  (sc->sc_link.adapter_buswidth - 1) - 1 - sc->sc_ntargets;

	/*
	 * We need 8 bytes for the lun sw additional entry, and
	 * eventually sizeof(tag_switch) for the tag switch entry.
	 * Keep enough free space for the free targets that could be
	 * probed later.
	 */
	if (sc->script_free_lo + 2 +
	    (ntargets * sizeof(lun_switch) / sizeof(lun_switch[0])) >=
	    ((sc->targets[target]->flags & TARF_TAG) ?
	    sc->script_free_hi - (sizeof(tag_switch) / sizeof(tag_switch[0])) :
	    sc->script_free_hi)) {
		/*
		 * Not enough space, but probably not worth dealing with it.
		 * We can hold 13 tagged-queuing capable devices in the 4k RAM.
		 */
#ifdef DEBUG
		printf("%s:%d:%d: not enough memory for a lun sw slot\n",
		    sc->sc_dev.dv_xname, target, lun);
#endif
		return;
	}
#ifdef SIOP_DEBUG
	printf("%s:%d:%d: allocate lun sw entry\n",
	    sc->sc_dev.dv_xname, target, lun);
#endif
	/* INT int_resellun */
	siop_script_write(sc, sc->script_free_lo, 0x98080000);
	siop_script_write(sc, sc->script_free_lo + 1, A_int_resellun);
	/* Now the slot entry: JUMP abs_foo, IF lun */
	siop_script_write(sc, sc->script_free_lo - 2,
	    0x800c0000 | lun);
	siop_script_write(sc, sc->script_free_lo - 1, 0);
	siop_lun->reseloff = sc->script_free_lo - 2;
	lunsw->lunsw_size += 2;
	sc->script_free_lo += 2;
	if (sc->targets[target]->flags & TARF_TAG) {
		/* we need a tag switch */
		sc->script_free_hi -=
		    sizeof(tag_switch) / sizeof(tag_switch[0]);
		if (sc->features & SF_CHIP_RAM) {
			bus_space_write_region_4(sc->sc_ramt, sc->sc_ramh,
			    sc->script_free_hi * 4, tag_switch,
			    sizeof(tag_switch) / sizeof(tag_switch[0]));
		} else {
			for(i = 0;
			    i < sizeof(tag_switch) / sizeof(tag_switch[0]);
			    i++) {
				sc->sc_script[sc->script_free_hi + i] = 
				    htole32(tag_switch[i]);
			}
		}
		siop_script_write(sc, 
		    siop_lun->reseloff + 1,
		    sc->sc_scriptaddr + sc->script_free_hi * 4 +
		    Ent_tag_switch_entry);

		for (i = 0; i < SIOP_NTAG; i++) {
			siop_lun->siop_tag[i].reseloff =
			    sc->script_free_hi + (Ent_resel_tag0 / 4) + i * 2;
		}
	} else {
		/* non-tag case; just work with the lun switch */
		siop_lun->siop_tag[0].reseloff = 
		    sc->targets[target]->siop_lun[lun]->reseloff;
	}
	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
}

void
siop_del_dev(sc, target, lun)
	struct siop_softc *sc;
	int target;
	int lun;
{
	int i;
#ifdef SIOP_DEBUG
		printf("%s:%d:%d: free lun sw entry\n",
		    sc->sc_dev.dv_xname, target, lun);
#endif
	if (sc->targets[target] == NULL)
		return;
	free(sc->targets[target]->siop_lun[lun], M_DEVBUF);
	sc->targets[target]->siop_lun[lun] = NULL;
	/* XXX compact sw entry too ? */
	/* check if we can free the whole target */
	for (i = 0; i < 8; i++) {
		if (sc->targets[target]->siop_lun[i] != NULL)
			return;
	}
#ifdef SIOP_DEBUG
	printf("%s: free siop_target for target %d lun %d lunsw offset %d\n",
	    sc->sc_dev.dv_xname, target, lun,
	    sc->targets[target]->lunsw->lunsw_off);
#endif
	/*
	 * nothing here, free the target struct and resel
	 * switch entry
	 */
	siop_script_write(sc, sc->targets[target]->reseloff, 0x800c00ff);
	siop_script_sync(sc, BUS_DMASYNC_PREWRITE);
	TAILQ_INSERT_TAIL(&sc->lunsw_list, sc->targets[target]->lunsw, next);
	free(sc->targets[target], M_DEVBUF);
	sc->targets[target] = NULL;
	sc->sc_ntargets--;
}

#ifdef SIOP_STATS
void
siop_printstats()
{
	printf("siop_stat_intr %d\n", siop_stat_intr);
	printf("siop_stat_intr_shortxfer %d\n", siop_stat_intr_shortxfer);
	printf("siop_stat_intr_xferdisc %d\n", siop_stat_intr_xferdisc);
	printf("siop_stat_intr_sdp %d\n", siop_stat_intr_sdp);
	printf("siop_stat_intr_done %d\n", siop_stat_intr_done);
	printf("siop_stat_intr_lunresel %d\n", siop_stat_intr_lunresel);
	printf("siop_stat_intr_qfull %d\n", siop_stat_intr_qfull);
}
#endif
