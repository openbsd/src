/*	$OpenBSD: ssvar.h,v 1.14 2010/01/09 21:12:06 dlg Exp $	*/
/*	$NetBSD: ssvar.h,v 1.2 1996/03/30 21:47:11 christos Exp $	*/

/*
 * Copyright (c) 1995 Kenneth Stailey.  All rights reserved.
 *   modified for configurable scanner support by Joachim Koenig
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
 *	This product includes software developed by Kenneth Stailey.
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
 */

#ifdef _KERNEL

/*
 * SCSI scanner interface description
 */

/*
 * Special handlers for impractically different scanner types.
 * Register NULL for a function if you want to try the real SCSI code
 * (with quirks table)
 */
struct ss_softc;
struct scan_io;

struct ss_special {
	int	(*set_params)(struct ss_softc *, struct scan_io *);
	int	(*trigger_scanner)(struct ss_softc *);
	int	(*get_params)(struct ss_softc *);
	/* some scanners only send line-multiples */
	void	(*minphys)(struct ss_softc *, struct buf *);
	int	(*read)(struct ss_softc *, struct scsi_xfer *, struct buf *);
	int	(*rewind_scanner)(struct ss_softc *);
	int	(*load_adf)(struct ss_softc *);
	int	(*unload_adf)(struct ss_softc *);
};

/*
 * ss_softc has to be declared here, because the device dependant
 * modules include it
 */
struct ss_softc {
	struct device sc_dev;

	int flags;
#define SSF_TRIGGERED	0x01	/* read operation has been primed */
#define	SSF_LOADED	0x02	/* parameters loaded */
#define SSF_WAITING	0x04
	struct scsi_link *sc_link;	/* contains our targ, lun, etc.	*/
	struct scan_io sio;
	struct buf buf_queue;		/* the queue of pending IO operations */
	const struct quirkdata *quirkdata; /* if we have a rogue entry */
	struct ss_special special;	/* special handlers for spec. devices */
	struct timeout timeout;
	struct mutex queue_mtx;
	struct mutex start_mtx;
};

struct buf *ss_buf_dequeue(struct ss_softc *);
void	ss_buf_enqueue(struct ss_softc *, struct buf *);
void	ss_buf_requeue(struct ss_softc *, struct buf *);

/*
 * define the special attach routines if configured
 */
void mustek_attach(struct ss_softc *, struct scsi_attach_args *);
void scanjet_attach(struct ss_softc *, struct scsi_attach_args *);

#endif /* _KERNEL */
