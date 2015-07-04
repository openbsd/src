/*	$OpenBSD: mscp_subr.c,v 1.14 2015/07/04 12:49:55 dlg Exp $	*/
/*	$NetBSD: mscp_subr.c,v 1.18 2001/11/13 07:38:28 lukem Exp $	*/
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mscp.c	7.5 (Berkeley) 12/16/90
 */

/*
 * MSCP generic driver routines
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/sid.h>

#include <arch/vax/mscp/mscp.h>
#include <arch/vax/mscp/mscpreg.h>
#include <arch/vax/mscp/mscpvar.h>

#include "ra.h"
#include "mt.h"

int	mscp_match(struct device *, struct cfdata *, void *);
void	mscp_attach(struct device *, struct device *, void *);
void	mscp_start(struct	mscp_softc *);
int	mscp_init(struct  mscp_softc *);
void	mscp_initds(struct mscp_softc *);
int	mscp_waitstep(struct mscp_softc *, int, int);

struct	cfattach mscpbus_ca = {
	sizeof(struct mscp_softc), (cfmatch_t)mscp_match, mscp_attach
};

struct cfdriver mscpbus_cd = {
	NULL, "mscpbus", DV_DULL
};

#define	READ_SA		(bus_space_read_2(mi->mi_iot, mi->mi_sah, 0))
#define	READ_IP		(bus_space_read_2(mi->mi_iot, mi->mi_iph, 0))
#define	WRITE_IP(x)	bus_space_write_2(mi->mi_iot, mi->mi_iph, 0, (x))
#define	WRITE_SW(x)	bus_space_write_2(mi->mi_iot, mi->mi_swh, 0, (x))

struct	mscp slavereply;

/*
 * This function is for delay during init. Some MSCP clone card (Dilog)
 * can't handle fast read from its registers, and therefore need
 * a delay between them.
 */

#define DELAYTEN 1000
int
mscp_waitstep(mi, mask, result)
	struct mscp_softc *mi;
	int mask, result;
{
	int	status = 1;

	if ((READ_SA & mask) != result) {
		volatile int count = 0;
		while ((READ_SA & mask) != result) {
			DELAY(10000);
			count += 1;
			if (count > DELAYTEN)
				break;
		}
		if (count > DELAYTEN)
			status = 0;
	}
	return status;
}

int
mscp_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct	mscp_attach_args *ma = aux;

#if NRA || NRX
	if (ma->ma_type & MSCPBUS_DISK)
		return 1;
#endif
#if NMT
	if (ma->ma_type & MSCPBUS_TAPE)
		return 1;
#endif
	return 0;
};

void
mscp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct	mscp_attach_args *ma = aux;
	struct	mscp_softc *mi = (void *)self;
	volatile struct mscp *mp;
	volatile int i;
	int	timeout, next = 0;

	mi->mi_mc = ma->ma_mc;
	mi->mi_me = NULL;
	mi->mi_type = ma->ma_type;
	mi->mi_uda = ma->ma_uda;
	mi->mi_dmat = ma->ma_dmat;
	mi->mi_dmam = ma->ma_dmam;
	mi->mi_iot = ma->ma_iot;
	mi->mi_iph = ma->ma_iph;
	mi->mi_sah = ma->ma_sah;
	mi->mi_swh = ma->ma_swh;
	mi->mi_ivec = ma->ma_ivec;
	mi->mi_adapnr = ma->ma_adapnr;
	mi->mi_ctlrnr = ma->ma_ctlrnr;
	*ma->ma_softc = mi;
	/*
	 * Go out to init the bus, so that we can give commands
	 * to its devices.
	 */
	mi->mi_cmd.mri_size = NCMD;
	mi->mi_cmd.mri_desc = mi->mi_uda->mp_ca.ca_cmddsc;
	mi->mi_cmd.mri_ring = mi->mi_uda->mp_cmd;
	mi->mi_rsp.mri_size = NRSP;
	mi->mi_rsp.mri_desc = mi->mi_uda->mp_ca.ca_rspdsc;
	mi->mi_rsp.mri_ring = mi->mi_uda->mp_rsp;
	bufq_init(&mi->mi_bufq, BUFQ_FIFO);

	if (mscp_init(mi)) {
		printf("%s: can't init, controller hung\n",
		    mi->mi_dev.dv_xname);
		return;
	}
	for (i = 0; i < NCMD; i++) {
		mi->mi_mxiuse |= (1 << i);
		if (bus_dmamap_create(mi->mi_dmat, (64*1024), 16, (64*1024),
		    0, BUS_DMA_NOWAIT, &mi->mi_xi[i].mxi_dmam)) {
			printf("Couldn't alloc dmamap %d\n", i);
			return;
		}
	}
	

#if NRA
	if (ma->ma_type & MSCPBUS_DISK) {
		extern	struct mscp_device ra_device;

		mi->mi_me = &ra_device;
	}
#endif
#if NMT
	if (ma->ma_type & MSCPBUS_TAPE) {
		extern	struct mscp_device mt_device;

		mi->mi_me = &mt_device;
	}
#endif
	/*
	 * Go out and search for sub-units on this MSCP bus,
	 * and call config_found for each found.
	 */
findunit:
	mp = mscp_getcp(mi, MSCP_DONTWAIT);
	if (mp == NULL)
		panic("mscpattach: no packets");
	mp->mscp_opcode = M_OP_GETUNITST;
	mp->mscp_unit = next;
	mp->mscp_modifier = M_GUM_NEXTUNIT;
	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;
	slavereply.mscp_opcode = 0;

	i = bus_space_read_2(mi->mi_iot, mi->mi_iph, 0);
	mp = &slavereply;
	timeout = 1000;
	while (timeout-- > 0) {
		DELAY(10000);
		if (mp->mscp_opcode)
			goto gotit;
	}
	printf("%s: no response to Get Unit Status request\n",
	    mi->mi_dev.dv_xname);
	return;

gotit:	/*
	 * Got a slave response.  If the unit is there, use it.
	 */
	switch (mp->mscp_status & M_ST_MASK) {

	case M_ST_SUCCESS:	/* worked */
	case M_ST_AVAILABLE:	/* found another drive */
		break;		/* use it */

	case M_ST_OFFLINE:
		/*
		 * Figure out why it is off line.  It may be because
		 * it is nonexistent, or because it is spun down, or
		 * for some other reason.
		 */
		switch (mp->mscp_status & ~M_ST_MASK) {

		case M_OFFLINE_UNKNOWN:
			/*
			 * No such drive, and there are none with
			 * higher unit numbers either, if we are
			 * using M_GUM_NEXTUNIT.
			 */
			mi->mi_ierr = 3;
			return;

		case M_OFFLINE_UNMOUNTED:
			/*
			 * The drive is not spun up.  Use it anyway.
			 *
			 * N.B.: this seems to be a common occurrance
			 * after a power failure.  The first attempt
			 * to bring it on line seems to spin it up
			 * (and thus takes several minutes).  Perhaps
			 * we should note here that the on-line may
			 * take longer than usual.
			 */
			break;

		default:
			/*
			 * In service, or something else equally unusable.
			 */
			printf("%s: unit %d off line: ", mi->mi_dev.dv_xname,
				mp->mscp_unit);
			mscp_printevent((struct mscp *)mp);
			next++;
			goto findunit;
		}
		break;

	default:
		printf("%s: unable to get unit status: ", mi->mi_dev.dv_xname);
		mscp_printevent((struct mscp *)mp);
		return;
	}

	/*
	 * If we get a lower number, we have circulated around all
	 * devices and are finished, otherwise try to find next unit.
	 * We shouldn't ever get this, it's a workaround.
	 */
	if (mp->mscp_unit < next)
		return;

	next = mp->mscp_unit + 1;
	goto findunit;
}


/*
 * The ctlr gets initialised, normally after boot but may also be 
 * done if the ctlr gets in an unknown state. Returns 1 if init
 * fails, 0 otherwise.
 */
int
mscp_init(mi)
	struct	mscp_softc *mi;
{
	struct	mscp *mp;
	volatile int i;
	int	status, count;
	unsigned int j = 0;

	/*
	 * While we are thinking about it, reset the next command
	 * and response indices.
	 */
	mi->mi_cmd.mri_next = 0;
	mi->mi_rsp.mri_next = 0;

	mi->mi_flags |= MSC_IGNOREINTR;

	if ((mi->mi_type & MSCPBUS_KDB) == 0)
		WRITE_IP(0); /* Kick off */;

	status = mscp_waitstep(mi, MP_STEP1, MP_STEP1);/* Wait to it wakes up */
	if (status == 0)
		return 1; /* Init failed */
	if (READ_SA & MP_ERR) {
		(*mi->mi_mc->mc_saerror)(mi->mi_dev.dv_parent, 0);
		return 1;
	}

	/* step1 */
	WRITE_SW(MP_ERR | (NCMDL2 << 11) | (NRSPL2 << 8) |
	    MP_IE | (mi->mi_ivec >> 2));
	status = mscp_waitstep(mi, STEP1MASK, STEP1GOOD);
	if (status == 0) {
		(*mi->mi_mc->mc_saerror)(mi->mi_dev.dv_parent, 0);
		return 1;
	}

	/* step2 */
	WRITE_SW((mi->mi_dmam->dm_segs[0].ds_addr & 0xffff) + 
	    offsetof(struct mscp_pack, mp_ca.ca_rspdsc[0]));
	status = mscp_waitstep(mi, STEP2MASK, STEP2GOOD(mi->mi_ivec >> 2));
	if (status == 0) {
		(*mi->mi_mc->mc_saerror)(mi->mi_dev.dv_parent, 0);
		return 1;
	}

	/* step3 */
	WRITE_SW((mi->mi_dmam->dm_segs[0].ds_addr >> 16));
	status = mscp_waitstep(mi, STEP3MASK, STEP3GOOD);
	if (status == 0) { 
		(*mi->mi_mc->mc_saerror)(mi->mi_dev.dv_parent, 0);
		return 1;
	}
	i = READ_SA & 0377;
	printf(": version %d model %d\n", i & 15, i >> 4);

#define BURST 4 /* XXX */
	if (mi->mi_type & MSCPBUS_UDA) {
		WRITE_SW(MP_GO | (BURST - 1) << 2);
		printf("%s: DMA burst size set to %d\n", 
		    mi->mi_dev.dv_xname, BURST);
	}
	WRITE_SW(MP_GO);

	mscp_initds(mi);
	mi->mi_flags &= ~MSC_IGNOREINTR;

	/*
	 * Set up all necessary info in the bus softc struct, get a
	 * mscp packet and set characteristics for this controller.
	 */
	mi->mi_credits = MSCP_MINCREDITS + 1;
	mp = mscp_getcp(mi, MSCP_DONTWAIT);

	mi->mi_credits = 0;
	mp->mscp_opcode = M_OP_SETCTLRC;
	mp->mscp_unit = mp->mscp_modifier = mp->mscp_flags =
	    mp->mscp_sccc.sccc_version = mp->mscp_sccc.sccc_hosttimo = 
	    mp->mscp_sccc.sccc_time = mp->mscp_sccc.sccc_time1 =
	    mp->mscp_sccc.sccc_errlgfl = 0;
	mp->mscp_sccc.sccc_ctlrflags = M_CF_ATTN | M_CF_MISC | M_CF_THIS;
	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;
	i = READ_IP;

	count = 0;
	while (count < DELAYTEN) {
		if (((volatile int)mi->mi_flags & MSC_READY) != 0)
			break;
		if ((j = READ_SA) & MP_ERR)
			goto out;
		DELAY(10000);
		count += 1;
	}
	if (count == DELAYTEN) {
out:
		printf("%s: couldn't set ctlr characteristics, sa=%x\n", 
		    mi->mi_dev.dv_xname, j);
		return 1;
	}
	return 0;
}

/*
 * Initialise the various data structures that control the mscp protocol.
 */
void
mscp_initds(mi)
	struct mscp_softc *mi;
{
	struct mscp_pack *ud = mi->mi_uda;
	struct mscp *mp;
	int i;

	for (i = 0, mp = ud->mp_rsp; i < NRSP; i++, mp++) {
		ud->mp_ca.ca_rspdsc[i] = MSCP_OWN | MSCP_INT |
		    (mi->mi_dmam->dm_segs[0].ds_addr +
		    offsetof(struct mscp_pack, mp_rsp[i].mscp_cmdref));
		mp->mscp_addr = &ud->mp_ca.ca_rspdsc[i];
		mp->mscp_msglen = MSCP_MSGLEN;
	}
	for (i = 0, mp = ud->mp_cmd; i < NCMD; i++, mp++) {
		ud->mp_ca.ca_cmddsc[i] = MSCP_INT |
		    (mi->mi_dmam->dm_segs[0].ds_addr +
		    offsetof(struct mscp_pack, mp_cmd[i].mscp_cmdref));
		mp->mscp_addr = &ud->mp_ca.ca_cmddsc[i];
		mp->mscp_msglen = MSCP_MSGLEN;
		if (mi->mi_type & MSCPBUS_TAPE)
			mp->mscp_vcid = 1;
	}
}

static	void mscp_kickaway(struct mscp_softc *);

void
mscp_intr(mi)
	struct mscp_softc *mi;
{
	struct mscp_pack *ud = mi->mi_uda;

	if (mi->mi_flags & MSC_IGNOREINTR)
		return;
	/*
	 * Check for response and command ring transitions.
	 */
	if (ud->mp_ca.ca_rspint) {
		ud->mp_ca.ca_rspint = 0;
		mscp_dorsp(mi);
	}
	if (ud->mp_ca.ca_cmdint) {
		ud->mp_ca.ca_cmdint = 0;
		MSCP_DOCMD(mi);
	}

	/*
	 * If there are any not-yet-handled request, try them now.
	 */
	if (bufq_peek(&mi->mi_bufq))
		mscp_kickaway(mi);
}

int
mscp_print(aux, name)
	void *aux;
	const char *name;
{
	struct drive_attach_args *da = aux;
	struct	mscp *mp = da->da_mp;
	int type = mp->mscp_guse.guse_mediaid;

	if (name) {
		printf("%c%c", MSCP_MID_CHAR(2, type), MSCP_MID_CHAR(1, type));
		if (MSCP_MID_ECH(0, type))
			printf("%c", MSCP_MID_CHAR(0, type));
		printf("%d at %s drive %d", MSCP_MID_NUM(type), name,
		    mp->mscp_unit);
	}
	return UNCONF;
}

/*
 * common strategy routine for all types of MSCP devices.
 */
void
mscp_strategy(bp, usc)
	struct buf *bp;
	struct device *usc;
{
	struct	mscp_softc *mi = (void *)usc;
	int s = spl6();

	bufq_queue(&mi->mi_bufq, bp);
	mscp_kickaway(mi);
	splx(s);
}


void
mscp_kickaway(mi)
	struct	mscp_softc *mi;
{
	struct buf *bp;
	struct	mscp *mp;
	int next;

	while ((bp = bufq_dequeue(&mi->mi_bufq))) {
		/*
		 * Ok; we are ready to try to start a xfer. Get a MSCP packet
		 * and try to start...
		 */
		if ((mp = mscp_getcp(mi, MSCP_DONTWAIT)) == NULL) {
			if (mi->mi_credits > MSCP_MINCREDITS)
				printf("%s: command ring too small\n",
				    mi->mi_dev.dv_parent->dv_xname);
			/*
			 * By some (strange) reason we didn't get a MSCP packet.
			 * Just return and wait for free packets.
			 */
			bufq_requeue(&mi->mi_bufq, bp);
			return;
		}
	
		if ((next = (ffs(mi->mi_mxiuse) - 1)) < 0)
			panic("no mxi buffers");
		mi->mi_mxiuse &= ~(1 << next);
		if (mi->mi_xi[next].mxi_inuse)
			panic("mxi inuse");
		/*
		 * Set up the MSCP packet and ask the ctlr to start.
		 */
		mp->mscp_opcode =
		    (bp->b_flags & B_READ) ? M_OP_READ : M_OP_WRITE;
		mp->mscp_cmdref = next;
		mi->mi_xi[next].mxi_bp = bp;
		mi->mi_xi[next].mxi_mp = mp;
		mi->mi_xi[next].mxi_inuse = 1;
		bp->b_resid = next;
		(*mi->mi_me->me_fillin)(bp, mp);
		(*mi->mi_mc->mc_go)(mi->mi_dev.dv_parent, &mi->mi_xi[next]);
	}
}

void
mscp_dgo(mi, mxi)
	struct mscp_softc *mi;
	struct mscp_xi *mxi;
{
	volatile int i;
	struct	mscp *mp;

	/*
	 * Fill in the MSCP packet and move the buffer to the I/O wait queue.
	 */
	mp = mxi->mxi_mp;
	mp->mscp_seq.seq_buffer = mxi->mxi_dmam->dm_segs[0].ds_addr;

	*mp->mscp_addr |= MSCP_OWN | MSCP_INT;
	i = READ_IP;
}

#ifdef DIAGNOSTIC
/*
 * Dump the entire contents of an MSCP packet in hex.  Mainly useful
 * for debugging....
 */
void
mscp_hexdump(mp)
	struct mscp *mp;
{
	long *p = (long *) mp;
	int i = mp->mscp_msglen;

	if (i > 256)		/* sanity */
		i = 256;
	i /= sizeof (*p);	/* ASSUMES MULTIPLE OF sizeof(long) */
	while (--i >= 0)
		printf("0x%x ", (int)*p++);
	printf("\n");
}
#endif

/*
 * MSCP error reporting
 */

/*
 * Messages for the various subcodes.
 */
static char unknown_msg[] = "unknown subcode";

/*
 * Subcodes for Success (0)
 */
static char *succ_msgs[] = {
	"normal",		/* 0 */
	"spin down ignored",	/* 1 = Spin-Down Ignored */
	"still connected",	/* 2 = Still Connected */
	unknown_msg,
	"dup. unit #",		/* 4 = Duplicate Unit Number */
	unknown_msg,
	unknown_msg,
	unknown_msg,
	"already online",	/* 8 = Already Online */
	unknown_msg,
	unknown_msg,
	unknown_msg,
	unknown_msg,
	unknown_msg,
	unknown_msg,
	unknown_msg,
	"still online",		/* 16 = Still Online */
};

/*
 * Subcodes for Invalid Command (1)
 */
static char *icmd_msgs[] = {
	"invalid msg length",	/* 0 = Invalid Message Length */
};

/*
 * Subcodes for Command Aborted (2)
 */
/* none known */

/*
 * Subcodes for Unit Offline (3)
 */
static char *offl_msgs[] = {
	"unknown drive",	/* 0 = Unknown, or online to other ctlr */
	"not mounted",		/* 1 = Unmounted, or RUN/STOP at STOP */
	"inoperative",		/* 2 = Unit Inoperative */
	unknown_msg,
	"duplicate",		/* 4 = Duplicate Unit Number */
	unknown_msg,
	unknown_msg,
	unknown_msg,
	"in diagnosis",		/* 8 = Disabled by FS or diagnostic */
};

/*
 * Subcodes for Unit Available (4)
 */
/* none known */

/*
 * Subcodes for Media Format Error (5)
 */
static char *media_fmt_msgs[] = {
	"fct unread - edc",	/* 0 = FCT unreadable */
	"invalid sector header",/* 1 = Invalid Sector Header */
	"not 512 sectors",	/* 2 = Not 512 Byte Sectors */
	"not formatted",	/* 3 = Not Formatted */
	"fct ecc",		/* 4 = FCT ECC */
};

/*
 * Subcodes for Write Protected (6)
 * N.B.:  Code 6 subcodes are 7 bits higher than other subcodes
 * (i.e., bits 12-15).
 */
static char *wrprot_msgs[] = {
	unknown_msg,
	"software",		/* 1 = Software Write Protect */
	"hardware",		/* 2 = Hardware Write Protect */
};

/*
 * Subcodes for Compare Error (7)
 */
/* none known */

/*
 * Subcodes for Data Error (8)
 */
static char *data_msgs[] = {
	"forced error",		/* 0 = Forced Error (software) */
	unknown_msg,
	"header compare",	/* 2 = Header Compare Error */
	"sync timeout",		/* 3 = Sync Timeout Error */
	unknown_msg,
	unknown_msg,
	unknown_msg,
	"uncorrectable ecc",	/* 7 = Uncorrectable ECC */
	"1 symbol ecc",		/* 8 = 1 bit ECC */
	"2 symbol ecc",		/* 9 = 2 bit ECC */
	"3 symbol ecc",		/* 10 = 3 bit ECC */
	"4 symbol ecc",		/* 11 = 4 bit ECC */
	"5 symbol ecc",		/* 12 = 5 bit ECC */
	"6 symbol ecc",		/* 13 = 6 bit ECC */
	"7 symbol ecc",		/* 14 = 7 bit ECC */
	"8 symbol ecc",		/* 15 = 8 bit ECC */
};

/*
 * Subcodes for Host Buffer Access Error (9)
 */
static char *host_buffer_msgs[] = {
	unknown_msg,
	"odd xfer addr",	/* 1 = Odd Transfer Address */
	"odd xfer count",	/* 2 = Odd Transfer Count */
	"non-exist. memory",	/* 3 = Non-Existent Memory */
	"memory parity",	/* 4 = Memory Parity Error */
};

/*
 * Subcodes for Controller Error (10)
 */
static char *cntlr_msgs[] = {
	unknown_msg,
	"serdes overrun",	/* 1 = Serialiser/Deserialiser Overrun */
	"edc",			/* 2 = Error Detection Code? */
	"inconsistent internal data struct",/* 3 = Internal Error */
};

/*
 * Subcodes for Drive Error (11)
 */
static char *drive_msgs[] = {
	unknown_msg,
	"sdi command timeout",	/* 1 = SDI Command Timeout */
	"ctlr detected protocol",/* 2 = Controller Detected Protocol Error */
	"positioner",		/* 3 = Positioner Error */
	"lost rd/wr ready",	/* 4 = Lost R/W Ready Error */
	"drive clock dropout",	/* 5 = Lost Drive Clock */
	"lost recvr ready",	/* 6 = Lost Receiver Ready */
	"drive detected error", /* 7 = Drive Error */
	"ctlr detected pulse or parity",/* 8 = Pulse or Parity Error */
};

/*
 * The following table correlates message codes with the
 * decoding strings.
 */
struct code_decode {
	char	*cdc_msg;
	int	cdc_nsubcodes;
	char	**cdc_submsgs;
} code_decode[] = {
#define SC(m)	sizeof (m) / sizeof (m[0]), m
	{"success",			SC(succ_msgs)},
	{"invalid command",		SC(icmd_msgs)},
	{"command aborted",		0, 0},
	{"unit offline",		SC(offl_msgs)},
	{"unit available",		0, 0},
	{"media format error",		SC(media_fmt_msgs)},
	{"write protected",		SC(wrprot_msgs)},
	{"compare error",		0, 0},
	{"data error",			SC(data_msgs)},
	{"host buffer access error",	SC(host_buffer_msgs)},
	{"controller error",		SC(cntlr_msgs)},
	{"drive error",			SC(drive_msgs)},
#undef SC
};

/*
 * Print the decoded error event from an MSCP error datagram.
 */
void
mscp_printevent(mp)
	struct mscp *mp;
{
	int event = mp->mscp_event;
	struct code_decode *cdc;
	int c, sc;
	char *cm, *scm;

	/*
	 * The code is the lower six bits of the event number (aka
	 * status).  If that is 6 (write protect), the subcode is in
	 * bits 12-15; otherwise, it is in bits 5-11.
	 * I WONDER WHAT THE OTHER BITS ARE FOR.  IT SURE WOULD BE
	 * NICE IF DEC SOLD DOCUMENTATION FOR THEIR OWN CONTROLLERS.
	 */
	c = event & M_ST_MASK;
	sc = (c != 6 ? event >> 5 : event >> 12) & 0x7ff;
	if (c >= sizeof code_decode / sizeof code_decode[0])
		cm = "- unknown code", scm = "??";
	else {
		cdc = &code_decode[c];
		cm = cdc->cdc_msg;
		if (sc >= cdc->cdc_nsubcodes)
			scm = unknown_msg;
		else
			scm = cdc->cdc_submsgs[sc];
	}
	printf(" %s (%s) (code %d, subcode %d)\n", cm, scm, c, sc);
}

static char *codemsg[16] = {
	"lbn", "code 1", "code 2", "code 3",
	"code 4", "code 5", "rbn", "code 7",
	"code 8", "code 9", "code 10", "code 11",
	"code 12", "code 13", "code 14", "code 15"
};
/*
 * Print the code and logical block number for an error packet.
 * THIS IS PROBABLY PECULIAR TO DISK DRIVES.  IT SURE WOULD BE
 * NICE IF DEC SOLD DOCUMENTATION FOR THEIR OWN CONTROLLERS.
 */
int
mscp_decodeerror(name, mp, mi)
	char *name;
	struct mscp *mp;
	struct mscp_softc *mi;
{
	int issoft;
	/* 
	 * We will get three sdi errors of type 11 after autoconfig
	 * is finished; depending of searching for non-existing units.
	 * How can we avoid this???
	 */
	if (((mp->mscp_event & M_ST_MASK) == 11) && (mi->mi_ierr++ < 3))
		return 1;
	/*
	 * For bad blocks, mp->mscp_erd.erd_hdr identifies a code and
	 * the logical block number.  Code 0 is a regular block; code 6
	 * is a replacement block.  The remaining codes are currently
	 * undefined.  The code is in the upper four bits of the header
	 * (bits 0-27 are the lbn).
	 */
	issoft = mp->mscp_flags & (M_LF_SUCC | M_LF_CONT);
#define BADCODE(h)	(codemsg[(unsigned)(h) >> 28])
#define BADLBN(h)	((h) & 0xfffffff)

	printf("%s: drive %d %s error datagram%s:", name, mp->mscp_unit,
		issoft ? "soft" : "hard",
		mp->mscp_flags & M_LF_CONT ? " (continuing)" : "");
	switch (mp->mscp_format & 0377) {

	case M_FM_CTLRERR:	/* controller error */
		break;

	case M_FM_BUSADDR:	/* host memory access error */
		printf(" memory addr 0x%x:", (int)mp->mscp_erd.erd_busaddr);
		break;

	case M_FM_DISKTRN:
		printf(" unit %d: level %d retry %d, %s %d:",
			mp->mscp_unit,
			mp->mscp_erd.erd_level, mp->mscp_erd.erd_retry,
			BADCODE(mp->mscp_erd.erd_hdr),
			(int)BADLBN(mp->mscp_erd.erd_hdr));
		break;

	case M_FM_SDI:
		printf(" unit %d: %s %d:", mp->mscp_unit,
			BADCODE(mp->mscp_erd.erd_hdr),
			(int)BADLBN(mp->mscp_erd.erd_hdr));
		break;

	case M_FM_SMLDSK:
		printf(" unit %d: small disk error, cyl %d:",
			mp->mscp_unit, mp->mscp_erd.erd_sdecyl);
		break;

	case M_FM_TAPETRN:
		printf(" unit %d: tape transfer error, grp 0x%x event 0%o:",
		    mp->mscp_unit, mp->mscp_erd.erd_sdecyl, mp->mscp_event);
		break;

	case M_FM_STIERR:
		printf(" unit %d: STI error, event 0%o:", mp->mscp_unit,
		    mp->mscp_event);
		break;

	default:
		printf(" unit %d: unknown error, format 0x%x:",
			mp->mscp_unit, mp->mscp_format);
	}
	mscp_printevent(mp);
	return 0;
#undef BADCODE
#undef BADLBN
}
