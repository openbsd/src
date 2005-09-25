/*	$OpenBSD: amivar.h,v 1.28 2005/09/25 04:48:51 dlg Exp $	*/

/*
 * Copyright (c) 2001 Michael Shalayeff
 * Copyright (c) 2005 Marco Peereboom
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

struct ami_softc;

struct ami_ccb {
	struct ami_softc	*ccb_sc;
	struct ami_iocmd	*ccb_cmd;
	paddr_t			ccb_cmdpa;
	struct ami_passthrough	*ccb_pt;
	paddr_t			ccb_ptpa;
	struct ami_sgent	*ccb_sglist;
	paddr_t			ccb_sglistpa;
	struct scsi_xfer	*ccb_xs;
	volatile int		ccb_wakeup;
	TAILQ_ENTRY(ami_ccb)	ccb_link;
	enum {
		AMI_CCB_FREE, AMI_CCB_READY, AMI_CCB_QUEUED, AMI_CCB_PREQUEUED
	} ccb_state;
	int			ccb_len;
	enum {
		AMI_CCB_IN, AMI_CCB_OUT
	}			ccb_dir;
	void			*ccb_data;
	bus_dmamap_t		ccb_dmamap;
};

typedef TAILQ_HEAD(ami_queue_head, ami_ccb)	ami_queue_head;

struct ami_rawsoftc {
	struct scsi_link sc_link;
	struct ami_softc *sc_softc;
	u_int8_t	sc_channel;

	int		sc_proctarget;	/* ses/safte target id */
	char		sc_procdev[16];	/* ses/safte device */
};

struct ami_softc {
	struct device	sc_dev;
	void		*sc_ih;
	struct scsi_link sc_link;

/* don't use 0x0001 */
#define AMI_BROKEN 	0x0002
#define	AMI_CMDWAIT	0x0004
#define AMI_QUARTZ	0x0008
	u_int	sc_flags;

	/* low-level interface */
	int (*sc_init)(struct ami_softc *sc);
	int (*sc_exec)(struct ami_softc *sc, struct ami_iocmd *);
	int (*sc_done)(struct ami_softc *sc, struct ami_iocmd *);
	int (*sc_poll)(struct ami_softc *sc, struct ami_iocmd *);
	int (*sc_ioctl)(struct device *, u_long, caddr_t);

	bus_space_tag_t	iot;
	bus_space_handle_t ioh;
	bus_dma_tag_t	dmat;

	volatile struct ami_iocmd *sc_mbox;
	paddr_t		sc_mbox_pa;
	struct ami_ccb	sc_ccbs[AMI_MAXCMDS];
	ami_queue_head	sc_free_ccb, sc_ccbq, sc_ccbdone;

	void		*sc_cmds;
	bus_dmamap_t	sc_cmdmap;
	bus_dma_segment_t sc_cmdseg[1];

	void		*sc_pts;
	bus_dmamap_t	sc_ptmap;
	bus_dma_segment_t sc_ptseg[1];

	void		*sc_sgents;
	bus_dmamap_t	sc_sgmap;
	bus_dma_segment_t sc_sgseg[1];

	int		sc_timeout;
	struct timeout	sc_requeue_tmo;
	struct timeout	sc_poll_tmo;
	int		sc_dis_poll;

	char	sc_fwver[16];
	char	sc_biosver[16];
	int	sc_maxcmds;
	int	sc_memory;
	int	sc_targets;
	int	sc_channels;
	int	sc_maxunits;
	int	sc_nunits;
	struct {
		u_int8_t	hd_present;
		u_int8_t	hd_is_logdrv;
		u_int8_t	hd_prop;
		u_int8_t	hd_stat;
		u_int32_t	hd_size;
		char		dev[16];
	} sc_hdr[AMI_BIG_MAX_LDRIVES];
	struct ami_rawsoftc *sc_rawsoftcs;
};

/* XXX These have to become spinlocks in case of SMP */
#define AMI_LOCK_AMI(sc) splbio()
#define AMI_UNLOCK_AMI(sc, lock) splx(lock)
typedef int ami_lock_t;

int  ami_attach(struct ami_softc *sc);
int  ami_intr(void *);

int ami_quartz_init(struct ami_softc *sc);
int ami_quartz_exec(struct ami_softc *sc, struct ami_iocmd *);
int ami_quartz_done(struct ami_softc *sc, struct ami_iocmd *);
int ami_quartz_poll(struct ami_softc *sc, struct ami_iocmd *);

int ami_schwartz_init(struct ami_softc *sc);
int ami_schwartz_exec(struct ami_softc *sc, struct ami_iocmd *);
int ami_schwartz_done(struct ami_softc *sc, struct ami_iocmd *);
int ami_schwartz_poll(struct ami_softc *sc, struct ami_iocmd *);

#ifdef AMI_DEBUG
void ami_print_mbox(struct ami_iocmd *);
#endif /* AMI_DEBUG */
