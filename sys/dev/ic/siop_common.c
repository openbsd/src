/*	$OpenBSD: siop_common.c,v 1.11 2001/10/08 01:25:07 krw Exp $ */
/*	$NetBSD: siop_common.c,v 1.12 2001/02/11 18:04:50 bouyer Exp $	*/

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
#include <sys/scsiio.h>

#include <machine/endian.h>
#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <dev/ic/siopreg.h>
#include <dev/ic/siopvar.h>
#include <dev/ic/siopvar_common.h>

#undef DEBUG
#undef DEBUG_DR

int siop_find_lun0_quirks __P((struct siop_softc *, u_int8_t, u_int16_t));

void
siop_common_reset(sc)
	struct siop_softc *sc;
{
	u_int32_t stest3;

	/* reset the chip */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_SRST);
	delay(1000);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, 0);

	/* init registers */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL0,
	    SCNTL0_ARB_MASK | SCNTL0_EPC | SCNTL0_AAP);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3, sc->clock_div);
	if (sc->features & SF_CHIP_C10)
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL4, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SXFER, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DIEN, 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SIEN0,
	    0xff & ~(SIEN0_CMP | SIEN0_SEL | SIEN0_RSL));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SIEN1,
	    0xff & ~(SIEN1_HTH | SIEN1_GEN));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3, STEST3_TE);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STIME0,
	    (0xb << STIME0_SEL_SHIFT));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCID,
	    sc->sc_link.adapter_target | SCID_RRE);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_RESPID0,
	    1 << sc->sc_link.adapter_target);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DCNTL,
	    (sc->features & SF_CHIP_PF) ? DCNTL_COM | DCNTL_PFEN : DCNTL_COM);

	/* enable clock doubler or quadruler if appropriate */
	if (sc->features & (SF_CHIP_DBLR | SF_CHIP_QUAD)) {
		stest3 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1,
		    STEST1_DBLEN);
		if ((sc->features & (SF_CHIP_QUAD | SF_CHIP_C10)) == SF_CHIP_QUAD) {
			/* wait for PPL to lock */
			while ((bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_STEST4) & STEST4_LOCK) == 0)
				delay(10);
		} else {
			/* data sheet says 20us - more won't hurt */
			delay(100);
		}
		/* halt scsi clock, select doubler/quad, restart clock */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3,
		    stest3 | STEST3_HSC);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1,
		    STEST1_DBLEN | STEST1_DBLSEL);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3, stest3);
	} else {
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1, 0);
	}
	if (sc->features & SF_CHIP_FIFO)
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST5,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST5) |
		    CTEST5_DFS);
		
	sc->sc_reset(sc);
}

int
siop_find_lun0_quirks(sc, bus, target)
	struct siop_softc *sc;
	u_int8_t bus;
	u_int16_t target;
{
	struct scsi_link *sc_link;
	struct device *dev;

	for (dev = TAILQ_FIRST(&alldevs); dev != NULL; dev = TAILQ_NEXT(dev, dv_list))
		if (dev->dv_parent == (struct device *)sc) {
			sc_link = ((struct scsibus_softc *)dev)->sc_link[target][0];
			if ((sc_link != NULL) && (sc_link->scsibus == bus))
				return sc_link->quirks;
		}

	/* If we can't find a quirks entry, assume the worst */
	return (SDEV_NOTAGS | SDEV_NOWIDE | SDEV_NOSYNC);
}

/* prepare tables before sending a cmd */
void
siop_setuptables(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	int i;
	struct siop_softc *sc = siop_cmd->siop_sc;
	struct scsi_xfer *xs = siop_cmd->xs;
	int target = xs->sc_link->target;
	int lun = xs->sc_link->lun;
	int *targ_flags = &sc->targets[target]->flags;
	int quirks;

	siop_cmd->siop_tables.id = htole32(sc->targets[target]->id);
	memset(siop_cmd->siop_tables.msg_out, 0, 8);
	if (siop_cmd->status != CMDST_SENSE)
		siop_cmd->siop_tables.msg_out[0] = MSG_IDENTIFY(lun, 1);
	else
		siop_cmd->siop_tables.msg_out[0] = MSG_IDENTIFY(lun, 0);
	siop_cmd->siop_tables.t_msgout.count= htole32(1);
	if (sc->targets[target]->status == TARST_ASYNC) {
		*targ_flags = 0;
		if (lun == 0)
			quirks = xs->sc_link->quirks;
		else
			quirks = siop_find_lun0_quirks(sc, xs->sc_link->scsibus, target);

		if ((quirks & SDEV_NOTAGS) == 0) {
			*targ_flags |= TARF_TAG;
			xs->sc_link->openings += SIOP_NTAG - SIOP_OPENINGS;
		}
		if ((quirks & SDEV_NOWIDE) == 0)
			*targ_flags |= TARF_WIDE;
		if ((quirks & SDEV_NOSYNC) == 0)
			*targ_flags |= TARF_SYNC;

		/* Safe to call siop_add_dev() multiple times */
		siop_add_dev(sc, target, 0);

		if ((sc->features & SF_CHIP_C10)
		    && (*targ_flags & TARF_WIDE)
		    && (xs->sc_link->inquiry_flags2 & (SID_CLOCKING | SID_QAS | SID_IUS))) {
			sc->targets[target]->status = TARST_PPR_NEG;
			siop_ppr_msg(siop_cmd, 1, 
				     (sc->min_dt_sync == 0) ? sc->min_st_sync : sc->min_dt_sync,
				     sc->maxoff);
		} else if (*targ_flags & TARF_WIDE) {
			sc->targets[target]->status = TARST_WIDE_NEG;
			siop_wdtr_msg(siop_cmd, 1, MSG_EXT_WDTR_BUS_16_BIT);
		} else if (*targ_flags & TARF_SYNC) {
			sc->targets[target]->status = TARST_SYNC_NEG;
			siop_sdtr_msg(siop_cmd, 1, sc->min_st_sync, sc->maxoff);
		} else {
			sc->targets[target]->status = TARST_OK;
			siop_print_info(sc, target);
		}
	} else if (sc->targets[target]->status == TARST_OK &&
	    (*targ_flags & TARF_TAG) &&
	    siop_cmd->status != CMDST_SENSE) {
		siop_cmd->flags |= CMDFL_TAG;
	}
	siop_cmd->siop_tables.status =
	    htole32(SCSI_SIOP_NOSTATUS); /* set invalid status */

	siop_cmd->siop_tables.cmd.count =
	    htole32(siop_cmd->dmamap_cmd->dm_segs[0].ds_len);
	siop_cmd->siop_tables.cmd.addr =
	    htole32(siop_cmd->dmamap_cmd->dm_segs[0].ds_addr);
	if ((xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) ||
	    siop_cmd->status == CMDST_SENSE) {
		for (i = 0; i < siop_cmd->dmamap_data->dm_nsegs; i++) {
			siop_cmd->siop_tables.data[i].count =
			    htole32(siop_cmd->dmamap_data->dm_segs[i].ds_len);
			siop_cmd->siop_tables.data[i].addr =
			    htole32(siop_cmd->dmamap_data->dm_segs[i].ds_addr);
		}
	}
	siop_table_sync(siop_cmd, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

int
siop_ppr_neg(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct siop_softc *sc = siop_cmd->siop_sc;
	struct siop_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->sc_link->target;
	struct siop_xfer_common *tables = &siop_cmd->siop_xfer->tables;
	int offset, sync, protocol, scf;

	sync     = tables->msg_in[3];
	offset   = tables->msg_in[5];
	protocol = tables->msg_in[7];

#ifdef DEBUG
	printf("%s: siop_ppr_neg: sync = %x, offset = %x, protocol = %x\n",
	    sc->sc_dev.dv_xname, sync, offset, protocol);
#endif
	/*
	 * Process protocol bits first, because finding the correct scf
	 * via siop_period_factor_to_scf() requires the TARF_ISDT flag
	 * to be correctly set.
	 */
	if (protocol & MSG_EXT_PPR_PROT_IUS)
		siop_target->flags |= TARF_ISIUS;

	if (protocol & MSG_EXT_PPR_PROT_DT) {
		siop_target->flags |= TARF_ISDT;
		sc->targets[target]->id |= SCNTL4_ULTRA3;
	}

	if (protocol & MSG_EXT_PPR_PROT_QAS)
		siop_target->flags |= TARF_ISQAS;

	scf = siop_period_factor_to_scf(sc, sync, siop_target->flags);

	if ((offset > sc->maxoff) ||
	    (scf == 0) ||
	    ((siop_target->flags & TARF_ISDT) && (offset == 1))) {
		tables->t_msgout.count= htole32(1);
		tables->msg_out[0] = MSG_MESSAGE_REJECT;
		return (SIOP_NEG_MSGOUT);
	}

	siop_target->id |= scf << (24 + SCNTL3_SCF_SHIFT);

	if (((sc->features & SF_CHIP_C10) == 0) && (sync < 25))
		siop_target->id |= SCNTL3_ULTRA << 24;

	siop_target->id |= (offset & 0xff) << 8;
	
	if (tables->msg_in[6] == MSG_EXT_WDTR_BUS_16_BIT) {
		siop_target->flags |= TARF_ISWIDE;
		sc->targets[target]->id |= (SCNTL3_EWS << 24);
	}

#ifdef DEBUG
	printf("%s: siop_ppr_neg: id now 0x%x, flags is now 0x%x\n",
	    sc->sc_dev.dv_xname, siop_target->id, siop_target->flags);
#endif
	tables->id = htole32(siop_target->id);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
	    (siop_target->id >> 24) & 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SXFER,
	    (siop_target->id >> 8) & 0xff);
	/* Only cards with SCNTL4 can cause PPR negotiations, so ... */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL4,
	    (siop_target->id & 0xff));

	siop_target->status = TARST_OK;
	siop_print_info(sc, target);
	
	return (SIOP_NEG_ACK);
}

int
siop_wdtr_neg(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct siop_softc *sc = siop_cmd->siop_sc;
	struct siop_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->sc_link->target;
	struct siop_xfer_common *tables = &siop_cmd->siop_xfer->tables;

	/* revert to narrow async until told otherwise */
	sc->targets[target]->id    &= 0x07ff0000; /* Keep SCNTL3.CCF and id */
	sc->targets[target]->flags &= ~(TARF_ISWIDE | TARF_ISDT | TARF_ISQAS | TARF_ISIUS);

	tables->id = htole32(sc->targets[target]->id);
	
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
	    (sc->targets[target]->id >> 24) & 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SXFER,  0);
	if (sc->features & SF_CHIP_C10)
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL4, 0);

	if (siop_target->status == TARST_WIDE_NEG) {
		/* we initiated wide negotiation */
		switch (tables->msg_in[3]) {
		case MSG_EXT_WDTR_BUS_8_BIT:
			siop_target->flags &= ~TARF_ISWIDE;
			sc->targets[target]->id &= ~(SCNTL3_EWS << 24);
			break;
		case MSG_EXT_WDTR_BUS_16_BIT:
			if (siop_target->flags & TARF_WIDE) {
				siop_target->flags |= TARF_ISWIDE;
				sc->targets[target]->id |= (SCNTL3_EWS << 24);
				break;
			}
		/* FALLTHROUGH */
		default:
			/*
 			 * We got more than we can handle, which shouldn't
			 * happen. Reject, and stay async.
			 */
			siop_target->flags &= ~TARF_ISWIDE;
			siop_target->status = TARST_OK;
			printf("%s: rejecting invalid wide negotiation from "
			    "target %d (%d)\n", sc->sc_dev.dv_xname, target,
			    tables->msg_in[3]);
			siop_print_info(sc, target);
			tables->t_msgout.count= htole32(1);
			tables->msg_out[0] = MSG_MESSAGE_REJECT;
			return SIOP_NEG_MSGOUT;
		}
		tables->id = htole32(sc->targets[target]->id);
		bus_space_write_1(sc->sc_rt, sc->sc_rh,
		    SIOP_SCNTL3,
		    (sc->targets[target]->id >> 24) & 0xff);
		/* we now need to do sync */
		if (siop_target->flags & TARF_SYNC) {
			siop_target->status = TARST_SYNC_NEG;
			siop_sdtr_msg(siop_cmd, 0, sc->min_st_sync, sc->maxoff);
			return SIOP_NEG_MSGOUT;
		} else {
			siop_target->status = TARST_OK;
			siop_print_info(sc, target);
			return SIOP_NEG_ACK;
		}
	} else {
		/* target initiated wide negotiation */
		if (tables->msg_in[3] >= MSG_EXT_WDTR_BUS_16_BIT
		    && (siop_target->flags & TARF_WIDE)) {
			siop_target->flags |= TARF_ISWIDE;
			sc->targets[target]->id |= SCNTL3_EWS << 24;
		} else {
			siop_target->flags &= ~TARF_ISWIDE;
			sc->targets[target]->id &= ~(SCNTL3_EWS << 24);
		}
		tables->id = htole32(sc->targets[target]->id);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
		    (sc->targets[target]->id >> 24) & 0xff);
		/*
		 * Don't schedule a sync neg, target should initiate it.
		 */
		if (siop_target->status != TARST_PROBING) {
			siop_target->status = TARST_OK;
			siop_print_info(sc, target);
		}
		siop_wdtr_msg(siop_cmd, 0, (siop_target->flags & TARF_ISWIDE) ?
		    MSG_EXT_WDTR_BUS_16_BIT : MSG_EXT_WDTR_BUS_8_BIT);
		return SIOP_NEG_MSGOUT;
	}
}

int
siop_sdtr_neg(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct siop_softc *sc = siop_cmd->siop_sc;
	struct siop_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->sc_link->target;
	int sync, offset, scf;
	int send_msgout = 0;
	struct siop_xfer_common *tables = &siop_cmd->siop_xfer->tables;

	sync = tables->msg_in[3];
	offset = tables->msg_in[4];

	/* revert to async until told otherwise */
	sc->targets[target]->id    &= 0x0fff0000; /* Keep SCNTL3.EWS, SCNTL3.CCF and id */
	sc->targets[target]->flags &= ~(TARF_ISDT | TARF_ISQAS | TARF_ISIUS);

	if (siop_target->status == TARST_SYNC_NEG) {
		/* we initiated sync negotiation */
#ifdef DEBUG
		printf("%s: sdtr for target %d: sync %d offset %d\n",
		    sc->sc_dev.dv_xname, target, sync, offset);
#endif
		scf = siop_period_factor_to_scf(sc, sync, sc->targets[target]->flags);
		if (offset > sc->maxoff || scf == 0)
			goto reject;
		sc->targets[target]->id |= scf << (24 + SCNTL3_SCF_SHIFT);
		if (((sc->features & SF_CHIP_C10) == 0) && (sync < 25))
			sc->targets[target]->id |= SCNTL3_ULTRA << 24;
		sc->targets[target]->id |= (offset & 0xff) << 8;
		goto end;
		/*
		 * We didn't find it in our table, so stay async and send reject
		 * msg.
		 */
reject:
		send_msgout = 1;
		tables->t_msgout.count= htole32(1);
		tables->msg_out[0] = MSG_MESSAGE_REJECT;
	} else { /* target initiated sync neg */
#ifdef DEBUG
		printf("%s: target initiated sdtr for target %d: sync %d offset %d\n",
		    sc->sc_dev.dv_xname, target, sync, offset);
		printf("sdtr (target): sync %d offset %d\n", sync, offset);
#endif
		if (sync < sc->min_st_sync)
			sync = sc->min_st_sync;
		scf = siop_period_factor_to_scf(sc, sync, sc->targets[target]->flags);
		if ((sc->targets[target]->flags & TARF_SYNC) == 0
		    || offset == 0
		    || scf == 0) {
			goto async;
		}
		if ((offset > 31) && ((sc->targets[target]->flags & TARF_ISDT) == 0))
			offset = 31;
		if (offset > sc->maxoff)
			offset = sc->maxoff;

		sc->targets[target]->id |= scf << (24 + SCNTL3_SCF_SHIFT);
		if (((sc->features & SF_CHIP_C10) == 0) && (sync < 25))
			sc->targets[target]->id |= SCNTL3_ULTRA << 24;
		sc->targets[target]->id |= (offset & 0xff) << 8;
		siop_sdtr_msg(siop_cmd, 0, sync, offset);
		send_msgout = 1;
		goto end;
async:
		siop_sdtr_msg(siop_cmd, 0, 0, 0);
		send_msgout = 1;
	}
end:
#ifdef DEBUG
	printf("id now 0x%x\n", sc->targets[target]->id);
#endif
	tables->id = htole32(sc->targets[target]->id);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
	    (sc->targets[target]->id >> 24) & 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SXFER,
	    (sc->targets[target]->id >> 8) & 0xff);

	if (siop_target->status != TARST_PROBING) {
		siop_target->status = TARST_OK;
		siop_print_info(sc, target);
	}

	if (send_msgout) {
		return SIOP_NEG_MSGOUT;
	} else {
		return SIOP_NEG_ACK;
	}
}

void
siop_ppr_msg(siop_cmd, offset, ssync, soff)
	struct siop_cmd *siop_cmd;
	int offset, ssync, soff;
{
	struct siop_softc *sc = siop_cmd->siop_sc;
	u_int8_t protocol;

	siop_cmd->siop_tables.msg_out[offset + 0] = MSG_EXTENDED;
	siop_cmd->siop_tables.msg_out[offset + 1] = MSG_EXT_PPR_LEN;
	siop_cmd->siop_tables.msg_out[offset + 2] = MSG_EXT_PPR;
	siop_cmd->siop_tables.msg_out[offset + 3] = ssync;
	siop_cmd->siop_tables.msg_out[offset + 4] = 0; /* RESERVED */
	siop_cmd->siop_tables.msg_out[offset + 5] = soff;
	siop_cmd->siop_tables.msg_out[offset + 6] = MSG_EXT_WDTR_BUS_16_BIT;

	protocol = 0;
	if (sc->min_dt_sync != 0)
		protocol |= MSG_EXT_PPR_PROT_DT;

	/* XXX - need tests for chip's capability to do QAS & IUS
	 *       
	 * if (test for QAS support)
	 *         protocol |= MSG_EXT_PPR_PROT_QAS;
	 * if (test for IUS support)
	 *         protocol |= MSG_EXT_PPR_PROT_IUS;
	 */

	siop_cmd->siop_tables.msg_out[offset + 7] = protocol;

	siop_cmd->siop_tables.t_msgout.count =
	    htole32(offset + MSG_EXT_PPR_LEN + 2);
}

void
siop_sdtr_msg(siop_cmd, offset, ssync, soff)
	struct siop_cmd *siop_cmd;
	int offset;
	int ssync, soff;
{
	siop_cmd->siop_tables.msg_out[offset + 0] = MSG_EXTENDED;
	siop_cmd->siop_tables.msg_out[offset + 1] = MSG_EXT_SDTR_LEN;
	siop_cmd->siop_tables.msg_out[offset + 2] = MSG_EXT_SDTR;
	siop_cmd->siop_tables.msg_out[offset + 3] = ssync;

	if ((soff > 31) && ((siop_cmd->siop_target->flags & TARF_ISDT) == 0))
		soff = 31;

	siop_cmd->siop_tables.msg_out[offset + 4] = soff;
	siop_cmd->siop_tables.t_msgout.count =
	    htole32(offset + MSG_EXT_SDTR_LEN + 2);
}

void
siop_wdtr_msg(siop_cmd, offset, wide)
	struct siop_cmd *siop_cmd;
	int offset;
{
	siop_cmd->siop_tables.msg_out[offset + 0] = MSG_EXTENDED;
	siop_cmd->siop_tables.msg_out[offset + 1] = MSG_EXT_WDTR_LEN;
	siop_cmd->siop_tables.msg_out[offset + 2] = MSG_EXT_WDTR;
	siop_cmd->siop_tables.msg_out[offset + 3] = wide;
	siop_cmd->siop_tables.t_msgout.count =
	    htole32(offset + MSG_EXT_WDTR_LEN + 2);
}

void
siop_minphys(bp)
	struct buf *bp;
{
	minphys(bp);
}

void
siop_sdp(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	/* save data pointer. Handle async only for now */
	int offset, dbc, sstat;
	struct siop_softc *sc = siop_cmd->siop_sc;
	scr_table_t *table; /* table to patch */

	if ((siop_cmd->xs->flags & (SCSI_DATA_OUT | SCSI_DATA_IN))
	    == 0)
	    return; /* no data pointers to save */
	offset = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCRATCHA + 1);
	if (offset >= SIOP_NSG) {
		printf("%s: bad offset in siop_sdp (%d)\n",
		    sc->sc_dev.dv_xname, offset);
		return;
	}
	table = &siop_cmd->siop_xfer->tables.data[offset];
#ifdef DEBUG_DR
	printf("sdp: offset %d count=%d addr=0x%x ", offset,
	    table->count, table->addr);
#endif
	dbc = bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DBC) & 0x00ffffff;
	if (siop_cmd->xs->flags & SCSI_DATA_OUT) {
		/* need to account for stale data in FIFO */
		if (sc->features & SF_CHIP_C10)
			dbc += bus_space_read_2(sc->sc_rt, sc->sc_rh, SIOP_DFBC);
		else {
			int dfifo = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_DFIFO);
			if (sc->features & SF_CHIP_FIFO) {
				dfifo |= (bus_space_read_1(sc->sc_rt, sc->sc_rh,
					      SIOP_CTEST5) & CTEST5_BOMASK) << 8;
				dbc += (dfifo - (dbc & 0x3ff)) & 0x3ff;
			} else {
				dbc += (dfifo - (dbc & 0x7f)) & 0x7f;
			}
		}
		sstat = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT0);
		if (sstat & SSTAT0_OLF)
			dbc++;
		if ((sc->features & SF_CHIP_C10) == 0)
			if (sstat & SSTAT0_ORF)
				dbc++;
		if (siop_cmd->siop_target->flags & TARF_ISWIDE) {
			sstat = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SSTAT2);
			if (sstat & SSTAT2_OLF1)
				dbc++;
			if ((sc->features & SF_CHIP_C10) == 0)
				if (sstat & SSTAT2_ORF1)
					dbc++;
		}
		/* clear the FIFO */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3) |
		    CTEST3_CLF);
	}
	table->addr =
	    htole32(letoh32(table->addr) + letoh32(table->count) - dbc);
	table->count = htole32(dbc);
#ifdef DEBUG_DR
	printf("now count=%d addr=0x%x\n", table->count, table->addr);
#endif
}

void
siop_clearfifo(sc)
	struct siop_softc *sc;
{
	int timeout = 0;
	int ctest3 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3);

#ifdef SIOP_DEBUG_INTR
	printf("DMA fifo not empty!\n");
#endif
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
	    ctest3 | CTEST3_CLF);
	while ((bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3) &
	    CTEST3_CLF) != 0) {
		delay(1);
		if (++timeout > 1000) {
			printf("clear fifo failed\n");
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
			    bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_CTEST3) & ~CTEST3_CLF);
			return;
		}
	}
}

int
siop_modechange(sc)
	struct siop_softc *sc;
{
	int retry;
	int sist0, sist1, stest2, stest4;
	for (retry = 0; retry < 5; retry++) {
		/*
		 * Datasheet says to wait 100ms and re-read SIST1,
		 * to check that DIFFSENSE is stable.
		 * We may delay() 5 times for 100ms at interrupt time;
		 * hopefully this will not happen often.
		 */
		delay(100000);
		sist0 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SIST0);
		sist1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SIST1);
		if (sist1 & SIEN1_SBMC)
			continue; /* we got an irq again */
		stest4 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST4) &
		    STEST4_MODE_MASK;
		stest2 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2);
		switch(stest4) {
		case STEST4_MODE_DIF:
			if (sc->features & SF_CHIP_C10) {
				printf("%s: invalid SCSI mode 0x%x\n",
				    sc->sc_dev.dv_xname, stest4);
				return 0;
			} else {
				printf("%s: switching to differential mode\n",
				    sc->sc_dev.dv_xname);
				bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2,
				    stest2 | STEST2_DIF);
			}
			break;
		case STEST4_MODE_SE:
			printf("%s: switching to single-ended mode\n",
			    sc->sc_dev.dv_xname);
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2,
			    stest2 & ~STEST2_DIF);
			break;
		case STEST4_MODE_LVD:
			printf("%s: switching to LVD mode\n",
			    sc->sc_dev.dv_xname);
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2,
			    stest2 & ~STEST2_DIF);
			break;
		default:
			printf("%s: invalid SCSI mode 0x%x\n",
			    sc->sc_dev.dv_xname, stest4);
			return 0;
		}
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST0,
		    stest4 >> 2);
		return 1;
	}
	printf("%s: timeout waiting for DIFFSENSE to stabilise\n",
	    sc->sc_dev.dv_xname);
	return 0;
}

void
siop_resetbus(sc)
	struct siop_softc *sc;
{
	int scntl1;
	scntl1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1,
	    scntl1 | SCNTL1_RST);
	/* minimum 25 us, more time won't hurt */
	delay(100);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, scntl1);
}

/*
 * siop_print_info: print the current negotiated wide/sync xfer values for
 *                  a particular target. This function is called whenever
 *		    a wide/sync negotiation completes, i.e. whenever
 *		    target->status is set to TARST_OK.
 */
void
siop_print_info(sc, target)
        struct siop_softc *sc;
        int target;
{
	struct siop_target *siop_target;
	u_int8_t scf, offset;
	int scf_index, factors, i;

	siop_target = sc->targets[target];
	
	printf("%s: target %d now using %s%s%s%s%d bit ",
            sc->sc_dev.dv_xname, target,
	    (siop_target->flags & TARF_TAG) ? "tagged " : "",
	    (siop_target->flags & TARF_ISDT) ? "DT " : "",
	    (siop_target->flags & TARF_ISQAS) ? "QAS " : "",
	    (siop_target->flags & TARF_ISIUS) ? "IUS " : "",
	    (siop_target->flags & TARF_ISWIDE) ? 16 : 8);

	offset = ((siop_target->id >> 8) & 0xff) >> SXFER_MO_SHIFT;

	if (offset == 0)
		printf("async ");
	else { 
		factors = sizeof(period_factor) / sizeof(period_factor[0]);
		
		scf = ((siop_target->id >> 24) & SCNTL3_SCF_MASK) >> SCNTL3_SCF_SHIFT;
		scf_index = sc->scf_index;

		for (i = 0; i < factors; i++)
			if (siop_target->flags & TARF_ISDT) {
				if (period_factor[i].scf[scf_index].dt_scf == scf)
					break;
			}
			else if	(period_factor[i].scf[scf_index].st_scf == scf)
				break;

		if (i >= factors)
			printf("?? ");
		else
			printf("%s ", period_factor[i].rate);

		printf("MHz %d REQ/ACK offset ", offset);
	}
	
	printf("xfers\n");
}

int
siop_period_factor_to_scf(sc, pf, flags)
	struct siop_softc *sc;
	int pf, flags;
{
	const int scf_index = sc->scf_index;
	int i;

	const int factors = sizeof(period_factor) / sizeof(period_factor[0]);

	for (i = 0; i < factors; i++)
		if (period_factor[i].factor == pf) {
			if (flags & TARF_ISDT)
				return (period_factor[i].scf[scf_index].dt_scf);
			else
				return (period_factor[i].scf[scf_index].st_scf);
		}

	return (0);
}
