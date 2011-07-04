/* $OpenBSD: softraid_aoe.c,v 1.20 2011/07/04 03:24:51 tedu Exp $ */
/*
 * Copyright (c) 2008 Ted Unangst <tedu@openbsd.org>
 * Copyright (c) 2008 Marco Peereboom <marco@openbsd.org>
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/disk.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/sensors.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/uio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/ethertypes.h>
#include <netinet/if_ether.h>
#include <net/if_aoe.h>

/* AOE initiator functions. */
int	sr_aoe_create(struct sr_discipline *, struct bioc_createraid *,
	    int, int64_t);
int	sr_aoe_assemble(struct sr_discipline *, struct bioc_createraid *,
	    int);
int	sr_aoe_alloc_resources(struct sr_discipline *);
int	sr_aoe_free_resources(struct sr_discipline *);
int	sr_aoe_rw(struct sr_workunit *);

/* AOE target functions. */
int	sr_aoe_server_create(struct sr_discipline *, struct bioc_createraid *,
	    int, int64_t);
int	sr_aoe_server_assemble(struct sr_discipline *, struct bioc_createraid *,
	    int);
int	sr_aoe_server_alloc_resources(struct sr_discipline *);
int	sr_aoe_server_free_resources(struct sr_discipline *);
int	sr_aoe_server_start(struct sr_discipline *);

void	sr_aoe_input(struct aoe_handler *, struct mbuf *);
void	sr_aoe_setup(struct aoe_handler *, struct mbuf *);
void	sr_aoe_timeout(void *);

/* Discipline initialisation. */
void
sr_aoe_discipline_init(struct sr_discipline *sd)
{

	/* Fill out discipline members. */
	sd->sd_type = SR_MD_AOE_INIT;
	sd->sd_capabilities = SR_CAP_SYSTEM_DISK;
	sd->sd_max_wu = SR_RAIDAOE_NOWU;

	/* Setup discipline pointers. */
	sd->sd_create = sr_aoe_create;
	sd->sd_assemble = sr_aoe_assemble;
	sd->sd_alloc_resources = sr_aoe_alloc_resources;
	sd->sd_free_resources = sr_aoe_free_resources;
	sd->sd_start_discipline = NULL;
	sd->sd_scsi_inquiry = sr_raid_inquiry;
	sd->sd_scsi_read_cap = sr_raid_read_cap;
	sd->sd_scsi_tur = sr_raid_tur;
	sd->sd_scsi_req_sense = sr_raid_request_sense;
	sd->sd_scsi_start_stop = sr_raid_start_stop;
	sd->sd_scsi_sync = sr_raid_sync;
	sd->sd_scsi_rw = sr_aoe_rw;
	/* XXX reuse raid 1 functions for now FIXME */
	sd->sd_set_chunk_state = sr_raid1_set_chunk_state;
	sd->sd_set_vol_state = sr_raid1_set_vol_state;
}

void
sr_aoe_server_discipline_init(struct sr_discipline *sd)
{

	/* Fill out discipline members. */
	sd->sd_type = SR_MD_AOE_TARG;
	sd->sd_capabilities = 0;
	sd->sd_max_wu = SR_RAIDAOE_NOWU;

	/* Setup discipline pointers. */
	sd->sd_create = sr_aoe_server_create;
	sd->sd_assemble = sr_aoe_server_assemble;
	sd->sd_alloc_resources = sr_aoe_server_alloc_resources;
	sd->sd_free_resources = sr_aoe_server_free_resources;
	sd->sd_start_discipline = sr_aoe_server_start;
	sd->sd_scsi_inquiry = NULL;
	sd->sd_scsi_read_cap = NULL;
	sd->sd_scsi_tur = NULL;
	sd->sd_scsi_req_sense = NULL;
	sd->sd_scsi_start_stop = NULL;
	sd->sd_scsi_sync = NULL;
	sd->sd_scsi_rw = NULL;
	sd->sd_set_chunk_state = NULL;
	sd->sd_set_vol_state = NULL;
}

/* AOE initiator */
int
sr_aoe_create(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk, int64_t coerced_size)
{

	if (no_chunk != 1)
		return EINVAL;

	strlcpy(sd->sd_name, "AOE INIT", sizeof(sd->sd_name));

	sd->sd_max_ccb_per_wu = no_chunk;

	return 0;
}

int
sr_aoe_assemble(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk)
{

	sd->sd_max_ccb_per_wu = sd->sd_meta->ssdi.ssd_chunk_no;

	return 0;
}

void
sr_aoe_setup(struct aoe_handler *ah, struct mbuf *m)
{
	struct aoe_packet	*ap;
	int			s;

	ap = mtod(m, struct aoe_packet *);
	if (ap->command != 1)
		goto out;
	if (ap->tag != 0)
		goto out;
	s = splnet();
	ah->fn = (workq_fn)sr_aoe_input;
	wakeup(ah);
	splx(s);

out:
	m_freem(m);
}

int
sr_aoe_alloc_resources(struct sr_discipline *sd)
{
	struct ifnet		*ifp;
	struct aoe_handler	*ah;
	unsigned char		slot;
	unsigned short		shelf;
	const char		*nic;
#if 0
	struct mbuf *m;
	struct ether_header *eh;
	struct aoe_packet *ap;
	int rv;
#endif
	int s;

	if (!sd)
		return (EINVAL);

	DNPRINTF(SR_D_DIS, "%s: sr_aoe_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_wu_alloc(sd);
	sr_ccb_alloc(sd);

	/* where do these come from */
	slot = 3;
	shelf = 4;
	nic = "ne0";

	ifp = ifunit(nic);
	if (!ifp) {
		return (EINVAL);
	}
	shelf = htons(shelf);

	ah = malloc(sizeof(*ah), M_DEVBUF, M_WAITOK | M_ZERO);
	ah->ifp = ifp;
	ah->major = shelf;
	ah->minor = slot;
	ah->fn = (workq_fn)sr_aoe_input;
	TAILQ_INIT(&ah->reqs);

	s = splnet();
	TAILQ_INSERT_TAIL(&aoe_handlers, ah, next);
	splx(s);

	sd->mds.mdd_aoe.sra_ah = ah;
	sd->mds.mdd_aoe.sra_eaddr[0] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[1] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[2] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[3] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[4] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[5] = 0xff;

#if 0
	MGETHDR(m, M_WAIT, MT_HEADER);
	eh = mtod(m, struct ether_header *);
	memcpy(eh->ether_dhost, sd->mds.mdd_aoe.sra_eaddr, 6);
	memcpy(eh->ether_shost, ((struct arpcom *)ifp)->ac_enaddr, 6);
	eh->ether_type = htons(ETHERTYPE_AOE);
	ap = (struct aoe_packet *)&eh[1];
	ap->vers = 1;
	ap->flags = 0;
	ap->error = 0;
	ap->major = shelf;
	ap->minor = slot;
	ap->command = 1;
	ap->tag = 0;
	ap->buffercnt = 0;
	ap->firmwarevers = 0;
	ap->configsectorcnt = 0;
	ap->serververs = 0;
	ap->ccmd = 0;
	ap->configstringlen = 0;
	m->m_pkthdr.len = m->m_len = AOE_CFGHDRLEN;
	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, rv);
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	rv = tsleep(ah, PRIBIO|PCATCH, "aoesetup", 30 * hz);
	splx(s);
	if (rv) {
		s = splnet();
		TAILQ_REMOVE(&aoe_handlers, ah, next);
		splx(s);
		free(ah, M_DEVBUF);
		return rv;
	}
#endif
	return 0;
}

int
sr_aoe_free_resources(struct sr_discipline *sd)
{
	int			s, rv = EINVAL;
	struct aoe_handler	*ah;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_aoe_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_wu_free(sd);
	sr_ccb_free(sd);

	ah = sd->mds.mdd_aoe.sra_ah;
	if (ah) {
		s = splnet();
		TAILQ_REMOVE(&aoe_handlers, ah, next);
		splx(s);
		free(ah, M_DEVBUF);
	}

	if (sd->sd_meta)
		free(sd->sd_meta, M_DEVBUF);

	rv = 0;
	return (rv);
}

int sr_send_aoe_chunk(struct sr_workunit *wu, daddr64_t blk, int i);
int
sr_send_aoe_chunk(struct sr_workunit *wu, daddr64_t blk, int i)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	int			s;
	daddr64_t		fragblk;
	struct mbuf		*m;
	struct ether_header	*eh;
	struct aoe_packet	*ap;
	struct ifnet		*ifp;
	struct aoe_handler	*ah;
	struct aoe_req		*ar;
	int			tag, rv;
	int			fragsize;
	const int		aoe_frags = 2;

	fragblk = blk + aoe_frags * i;
	fragsize = aoe_frags * 512;
	if (fragblk + aoe_frags - 1 > wu->swu_blk_end) {
		fragsize = (wu->swu_blk_end - fragblk + 1) * 512;
	}
	tag = ++sd->mds.mdd_aoe.sra_tag;
	ah = sd->mds.mdd_aoe.sra_ah;
	ar = malloc(sizeof(*ar), M_DEVBUF, M_NOWAIT);
	if (!ar) {
		splx(s);
		return ENOMEM;
	}
	ar->v = wu;
	ar->tag = tag;
	ar->len = fragsize;
	timeout_set(&ar->to, sr_aoe_timeout, ar);
	TAILQ_INSERT_TAIL(&ah->reqs, ar, next);
	splx(s);

	ifp = ah->ifp;
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (xs->flags & SCSI_DATA_OUT && m) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			m = NULL;
		}
	}
	if (!m) {
		s = splbio();
		TAILQ_REMOVE(&ah->reqs, ar, next);
		splx(s);
		free(ar, M_DEVBUF);
		return ENOMEM;
	}

	eh = mtod(m, struct ether_header *);
	memcpy(eh->ether_dhost, sd->mds.mdd_aoe.sra_eaddr, 6);
	memcpy(eh->ether_shost, ((struct arpcom *)ifp)->ac_enaddr, 6);
	eh->ether_type = htons(ETHERTYPE_AOE);
	ap = (struct aoe_packet *)&eh[1];
	ap->vers = 1;
	ap->flags = 0;
	ap->error = 0;
	ap->major = ah->major;
	ap->minor = ah->minor;
	ap->command = 0;
	ap->tag = tag;
	ap->aflags = 0; /* AOE_EXTENDED; */
	if (xs->flags & SCSI_DATA_OUT) {
		ap->aflags |= AOE_WRITE;
		ap->cmd = AOE_WRITE;
		memcpy(ap->data, xs->data + (aoe_frags * i * 512), fragsize);
	} else {
		ap->cmd = AOE_READ;
	}
	ap->feature = 0;
	ap->sectorcnt = fragsize / 512;
	AOE_BLK2HDR(fragblk, ap);

	m->m_pkthdr.len = m->m_len = AOE_CMDHDRLEN + fragsize;
	s = splnet();
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, rv);
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	if (rv == 0)
		timeout_add_sec(&ar->to, 10);
	splx(s);

	if (rv) {
		s = splbio();
		TAILQ_REMOVE(&ah->reqs, ar, next);
		splx(s);
		free(ar, M_DEVBUF);
	}

	return rv;
}

int
sr_aoe_rw(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_chunk		*scp;
	daddr64_t		blk;
	int			s, ios, rt;
	int			rv, i;
	const int		aoe_frags = 2;


	printf("%s: sr_aoe_rw 0x%02x\n", DEVNAME(sd->sd_sc),
	    xs->cmd->opcode);
	return (1);

	DNPRINTF(SR_D_DIS, "%s: sr_aoe_rw 0x%02x\n", DEVNAME(sd->sd_sc),
	    xs->cmd->opcode);

	/* blk and scsi error will be handled by sr_validate_io */
	if (sr_validate_io(wu, &blk, "sr_aoe_rw"))
		goto bad;

	/* add 1 to get the inclusive amount, then some more for rounding */
	ios = (wu->swu_blk_end - wu->swu_blk_start + 1 + (aoe_frags - 1)) /
	    aoe_frags;
	wu->swu_io_count = ios;

	if (xs->flags & SCSI_POLL)
		panic("can't AOE poll");

	s = splbio();
	for (i = 0; i < ios; i++) {
		if (xs->flags & SCSI_DATA_IN) {
			rt = 0;
ragain:
			scp = sd->sd_vol.sv_chunks[0];
			switch (scp->src_meta.scm_status) {
			case BIOC_SDONLINE:
			case BIOC_SDSCRUB:
				break;

			case BIOC_SDOFFLINE:
			case BIOC_SDREBUILD:
			case BIOC_SDHOTSPARE:
				if (rt++ < sd->sd_meta->ssdi.ssd_chunk_no)
					goto ragain;

				/* FALLTHROUGH */
			default:
				/* volume offline */
				printf("%s: is offline, can't read\n",
				DEVNAME(sd->sd_sc));
				goto bad;
			}
		} else {
			scp = sd->sd_vol.sv_chunks[0];
			switch (scp->src_meta.scm_status) {
			case BIOC_SDONLINE:
			case BIOC_SDSCRUB:
			case BIOC_SDREBUILD:
				break;

			case BIOC_SDHOTSPARE: /* should never happen */
			case BIOC_SDOFFLINE:
				wu->swu_io_count--;
				goto bad;

			default:
				goto bad;
			}
		}

		rv = sr_send_aoe_chunk(wu, blk, i);

		if (rv) {
			return rv;
		}
	}

	return (0);
bad:
	/* wu is unwound by sr_wu_put */
	return (1);
}

void
sr_aoe_input(struct aoe_handler *ah, struct mbuf *m)
{
	struct sr_discipline	*sd;
	struct scsi_xfer	*xs;
	struct aoe_req		*ar;
	struct aoe_packet	*ap;
	struct sr_workunit	*wu;
	daddr64_t		blk, offset;
	int			len, s;
	int			tag;

	ap = mtod(m, struct aoe_packet *);
	tag = ap->tag;

	s = splnet();
	TAILQ_FOREACH(ar, &ah->reqs, next) {
		if (ar->tag == tag) {
			TAILQ_REMOVE(&ah->reqs, ar, next);
			break;
		}
	}
	splx(s);
	if (!ar) {
		goto out;
	}
	timeout_del(&ar->to);
	wu = ar->v;
	sd = wu->swu_dis;
	xs = wu->swu_xs;


	if (ap->flags & AOE_F_ERROR) {
		wu->swu_ios_failed++;
		goto out;
	} else {
		wu->swu_ios_succeeded++;
		len = ar->len; /* XXX check against sector count */
		if (xs->flags & SCSI_DATA_IN) {
			AOE_HDR2BLK(ap, blk);
			/* XXX bounds checking */
			offset = (wu->swu_blk_start - blk) * 512;
			memcpy(xs->data + offset, ap->data, len);
		}
	}

	wu->swu_ios_complete++;

	s = splbio();

	if (wu->swu_ios_complete == wu->swu_io_count) {
		if (wu->swu_ios_failed == wu->swu_ios_complete)
			xs->error = XS_DRIVER_STUFFUP;
		else
			xs->error = XS_NOERROR;

		xs->resid = 0;

		sr_scsi_done(sd, xs);
	}

out:
	m_freem(m);
}

void
sr_aoe_timeout(void *v)
{
	struct aoe_req		*ar = v;
	struct sr_discipline	*sd;
	struct scsi_xfer	*xs;
	struct aoe_handler	*ah;
	struct aoe_req		*ar2;
	struct sr_workunit	*wu;
	int			s;

	wu = ar->v;
	sd = wu->swu_dis;
	xs = wu->swu_xs;
	ah = sd->mds.mdd_aoe.sra_ah;

	s = splnet();
	TAILQ_FOREACH(ar2, &ah->reqs, next) {
		if (ar2->tag == ar->tag) {
			TAILQ_REMOVE(&ah->reqs, ar, next);
			break;
		}
	}
	splx(s);
	if (!ar2)
		return;
	free(ar, M_DEVBUF);
	/* give it another go */
	/* XXX this is going to repeat the whole workunit */
	sr_aoe_rw(wu);
}

/* AOE target */
void		sr_aoe_server(struct aoe_handler *, struct mbuf *);
void		sr_aoe_server_create_thread(void *);
void		sr_aoe_server_thread(void *);

int
sr_aoe_server_create(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk, int64_t coerced_size)
{

	if (no_chunk != 1)
		return EINVAL;

	sd->sd_meta->ssdi.ssd_size = coerced_size;

	strlcpy(sd->sd_name, "AOE TARG", sizeof(sd->sd_name));

	sd->sd_max_ccb_per_wu = no_chunk;

	return 0;
}

int
sr_aoe_server_assemble(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk)
{

	sd->sd_max_ccb_per_wu = sd->sd_meta->ssdi.ssd_chunk_no;

	return 0;
}

int
sr_aoe_server_alloc_resources(struct sr_discipline *sd)
{
	int			s, rv = EINVAL;
	unsigned char		slot;
	unsigned short		shelf;
	const char		*nic;
	struct aoe_handler	*ah;
	struct ifnet		*ifp;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_aoe_server_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	/* setup runtime values */
	/* XXX where do these come from */
	slot = 3;
	shelf = 4;
	nic = "re0";

	ifp = ifunit(nic);
	if (!ifp) {
		printf("%s: sr_aoe_server_alloc_resources: illegal interface "
		    "%s\n", DEVNAME(sd->sd_sc), nic);
		return (EINVAL);
	}
	shelf = htons(shelf);

	ah = malloc(sizeof(*ah), M_DEVBUF, M_WAITOK | M_ZERO);
	ah->ifp = ifp;
	ah->major = shelf;
	ah->minor = slot;
	ah->fn = (workq_fn)sr_aoe_server;
	TAILQ_INIT(&ah->reqs);

	s = splnet();
	TAILQ_INSERT_TAIL(&aoe_handlers, ah, next);
	splx(s);

	sd->mds.mdd_aoe.sra_ah = ah;
	sd->mds.mdd_aoe.sra_eaddr[0] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[1] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[2] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[3] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[4] = 0xff;
	sd->mds.mdd_aoe.sra_eaddr[5] = 0xff;
	sd->mds.mdd_aoe.sra_ifp = ifp;

	if (sr_wu_alloc(sd))
		goto bad;
	if (sr_ccb_alloc(sd))
		goto bad;

	rv = 0;
bad:
	return (rv);
}

int
sr_aoe_server_free_resources(struct sr_discipline *sd)
{
	int			s;

	if (!sd)
		return (EINVAL);

	DNPRINTF(SR_D_DIS, "%s: sr_aoe_server_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_wu_free(sd);
	sr_ccb_free(sd);

	s = splnet();
	if (sd->mds.mdd_aoe.sra_ah) {
		TAILQ_REMOVE(&aoe_handlers, sd->mds.mdd_aoe.sra_ah, next);
		free(sd->mds.mdd_aoe.sra_ah, M_DEVBUF);
	}
	splx(s);

	return (0);
}

int
sr_aoe_server_start(struct sr_discipline *sd)
{
	kthread_create_deferred(sr_aoe_server_create_thread, sd);

	return (0);
}

void
sr_aoe_server_create_thread(void *arg)
{
	struct sr_discipline	*sd = arg;

	if (kthread_create(sr_aoe_server_thread, arg, NULL, DEVNAME(sd->sd_sc))
	    != 0) {
		printf("%s: unable to create AOE thread\n",
		    DEVNAME(sd->sd_sc));
		/* XXX unwind */
		return;
	}
}

void
sr_aoe_server_thread(void *arg)
{
	struct sr_discipline	*sd = arg;
	struct ifnet		*ifp;
	struct aoe_handler	*ah;
	struct aoe_req		*ar;
	struct aoe_packet	*rp, *ap;
	struct mbuf		*m, *m2;
	struct ether_header	*eh;
	struct buf		buf;
	daddr64_t		blk;
	int			len;
	int			rv, s;

	/* sanity */
	if (!sd)
		return;
	ah = sd->mds.mdd_aoe.sra_ah;
	if (ah == NULL)
		return;
	ifp = sd->mds.mdd_aoe.sra_ifp;
	if (ifp == NULL)
		return;

	printf("%s: AOE target: %s exported via: %s\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname, ifp->if_xname);

	while (1) {
		s = splnet();
resleep:
		rv = tsleep(ah, PCATCH | PRIBIO, "aoe targ", 0);
		if (rv) {
			splx(s);
			break;
		}
		ar = TAILQ_FIRST(&ah->reqs);
		if (!ar) {
			goto resleep;
		}
		TAILQ_REMOVE(&ah->reqs, ar, next);
		splx(s);
		m2 = ar->v;
		rp = mtod(m2, struct aoe_packet *);
		if (rp->command) {
			continue;
		}
		if (rp->aflags & AOE_AF_WRITE) {
			MGETHDR(m, M_DONTWAIT, MT_HEADER);
			if (!m)
				continue;
			len = rp->sectorcnt * 512;

			eh = mtod(m, struct ether_header *);
			memcpy(eh->ether_dhost, sd->mds.mdd_aoe.sra_eaddr, 6);
			memcpy(eh->ether_shost,
			    ((struct arpcom *)ifp)->ac_enaddr, 6);
			eh->ether_type = htons(ETHERTYPE_AOE);
			ap = (struct aoe_packet *)&eh[1];
			AOE_HDR2BLK(ap, blk);
			bzero(&buf, sizeof(buf));
			buf.b_blkno = blk;
			buf.b_flags = B_WRITE | B_PHYS;
			buf.b_bcount = len;
			buf.b_bufsize = len;
			buf.b_resid = len;
			buf.b_data = rp->data;
			buf.b_error = 0;
			buf.b_proc = curproc;
			buf.b_dev = sd->sd_vol.sv_chunks[0]->src_dev_mm;
			buf.b_vp = sd->sd_vol.sv_chunks[0]->src_vn;
			if ((buf.b_flags & B_READ) == 0)
				buf.b_vp->v_numoutput++;
			LIST_INIT(&buf.b_dep);

			s = splbio();
			VOP_STRATEGY(&buf);
			biowait(&buf);
			splx(s);

			ap->vers = 1;
			ap->flags = AOE_F_RESP;
			ap->error = 0;
			ap->major = rp->major;
			ap->minor = rp->minor;
			ap->command = 1;
			ap->tag = rp->tag;
			ap->aflags = rp->aflags;
			ap->feature = 0;
			ap->sectorcnt = len / 512;
			ap->cmd = AOE_WRITE;
			ap->lba0 = 0;
			ap->lba1 = 0;
			ap->lba2 = 0;
			ap->lba3 = 0;
			ap->lba4 = 0;
			ap->lba5 = 0;
			ap->reserved = 0;

			m->m_pkthdr.len = m->m_len = AOE_CMDHDRLEN;

			s = splnet();
			IFQ_ENQUEUE(&ifp->if_snd, m, NULL, rv);
			if ((ifp->if_flags & IFF_OACTIVE) == 0)
				(*ifp->if_start)(ifp);
			splx(s);
		} else {
			MGETHDR(m, M_DONTWAIT, MT_HEADER);
			if (m) {
				MCLGET(m, M_DONTWAIT);
				if (!(m->m_flags & M_EXT)) {
					m_freem(m);
					m = NULL;
				}
			}
			if (!m)
				continue;
			len = rp->sectorcnt * 512;

			eh = mtod(m, struct ether_header *);
			memcpy(eh->ether_dhost, sd->mds.mdd_aoe.sra_eaddr, 6);
			memcpy(eh->ether_shost,
			    ((struct arpcom *)ifp)->ac_enaddr, 6);
			eh->ether_type = htons(ETHERTYPE_AOE);
			ap = (struct aoe_packet *)&eh[1];
			AOE_HDR2BLK(ap, blk);
			memset(&buf, 0, sizeof buf);
			buf.b_blkno = blk;
			buf.b_flags = B_WRITE | B_PHYS;
			buf.b_bcount = len;
			buf.b_bufsize = len;
			buf.b_resid = len;
			buf.b_data = ap->data;
			buf.b_error = 0;
			buf.b_proc = curproc;
			buf.b_dev = sd->sd_vol.sv_chunks[0]->src_dev_mm;
			buf.b_vp = sd->sd_vol.sv_chunks[0]->src_vn;
			if ((buf.b_flags & B_READ) == 0)
				buf.b_vp->v_numoutput++;
			LIST_INIT(&buf.b_dep);

			s = splbio();
			VOP_STRATEGY(&buf);
			biowait(&buf);
			splx(s);

			ap->vers = 1;
			ap->flags = AOE_F_RESP;
			ap->error = 0;
			ap->major = rp->major;
			ap->minor = rp->minor;
			ap->command = 1;
			ap->tag = rp->tag;
			ap->aflags = rp->aflags;
			ap->feature = 0;
			ap->sectorcnt = len / 512;
			ap->cmd = AOE_READ;
			ap->lba0 = 0;
			ap->lba1 = 0;
			ap->lba2 = 0;
			ap->lba3 = 0;
			ap->lba4 = 0;
			ap->lba5 = 0;
			ap->reserved = 0;
			m->m_pkthdr.len = m->m_len = AOE_CMDHDRLEN;

			s = splnet();
			IFQ_ENQUEUE(&ifp->if_snd, m, NULL, rv);
			if ((ifp->if_flags & IFF_OACTIVE) == 0)
				(*ifp->if_start)(ifp);
			splx(s);
		}
	}
}

void
sr_aoe_server(struct aoe_handler *ah, struct mbuf *m)
{
	struct aoe_req		*ar;
	int			s;

	ar = malloc(sizeof *ar, M_DEVBUF, M_NOWAIT);
	if (!ar) {
		/* XXX warning? */
		m_freem(m);
		return;
	}
	ar->v = m;
	s = splnet();
	TAILQ_INSERT_TAIL(&ah->reqs, ar, next);
	wakeup(ah);
	splx(s);
}
