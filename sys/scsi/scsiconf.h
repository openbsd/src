/*	$OpenBSD: scsiconf.h,v 1.16 1998/08/04 22:35:05 millert Exp $	*/
/*	$NetBSD: scsiconf.h,v 1.35 1997/04/02 02:29:38 mycroft Exp $	*/

/*
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#ifndef	SCSI_SCSICONF_H
#define SCSI_SCSICONF_H 1

typedef	int			boolean;

#include <sys/queue.h>
#include <machine/cpu.h>
#include <scsi/scsi_debug.h>

/*
 * The following documentation tries to describe the relationship between the
 * various structures defined in this file:
 *
 * each adapter type has a scsi_adapter struct. This describes the adapter and
 *    identifies routines that can be called to use the adapter.
 * each device type has a scsi_device struct. This describes the device and
 *    identifies routines that can be called to use the device.
 * each existing device position (scsibus + target + lun)
 *    can be described by a scsi_link struct.
 *    Only scsi positions that actually have devices, have a scsi_link
 *    structure assigned. so in effect each device has scsi_link struct.
 *    The scsi_link structure contains information identifying both the
 *    device driver and the adapter driver for that position on that scsi bus,
 *    and can be said to 'link' the two.
 * each individual scsi bus has an array that points to all the scsi_link
 *    structs associated with that scsi bus. Slots with no device have
 *    a NULL pointer.
 * each individual device also knows the address of it's own scsi_link
 *    structure.
 *
 *				-------------
 *
 * The key to all this is the scsi_link structure which associates all the
 * other structures with each other in the correct configuration.  The
 * scsi_link is the connecting information that allows each part of the
 * scsi system to find the associated other parts.
 */

struct buf;
struct scsi_xfer;

/*
 * These entrypoints are called by the high-end drivers to get services from
 * whatever low-end drivers they are attached to each adapter type has one of
 * these statically allocated.
 */
struct scsi_adapter {
	int		(*scsi_cmd) __P((struct scsi_xfer *));
	void		(*scsi_minphys) __P((struct buf *));
	int		(*open_target_lu) __P((void));
	int		(*close_target_lu) __P((void));
};

/*
 * return values for scsi_cmd()
 */
#define SUCCESSFULLY_QUEUED	0
#define TRY_AGAIN_LATER		1
#define	COMPLETE		2
#define	ESCAPE_NOT_SUPPORTED	3

/*
 * These entry points are called by the low-end drivers to get services from
 * whatever high-end drivers they are attached to.  Each device type has one
 * of these statically allocated.
 */
struct scsi_device {
	int	(*err_handler) __P((struct scsi_xfer *));
			/* returns -1 to say err processing done */
	void	(*start) __P((void *));

	int	(*async) __P((void));
	/*
	 * When called with `0' as the second argument, we expect status
	 * back from the upper-level driver.  When called with a `1',
	 * we're simply notifying the upper-level driver that the command
	 * is complete and expect no status back.
	 */
	void	(*done)  __P((struct scsi_xfer *));
};

/*
 * This structure describes the connection between an adapter driver and
 * a device driver, and is used by each to call services provided by
 * the other, and to allow generic scsi glue code to call these services
 * as well.
 */
struct scsi_link {
	u_int8_t scsi_version;		/* SCSI-I, SCSI-II, etc. */
	u_int8_t scsibus;		/* the Nth scsibus */
	u_int8_t target;		/* targ of this dev */
	u_int8_t lun;			/* lun of this dev */
	u_int8_t adapter_target;	/* what are we on the scsi bus */
	u_int8_t adapter_buswidth;	/* 8 (regular) or 16 (wide). (0 becomes 8) */
	u_int8_t openings;		/* available operations */
	u_int8_t active;		/* operations in progress */
	u_int16_t flags;		/* flags that all devices have */
#define	SDEV_REMOVABLE	 	0x0001	/* media is removable */
#define	SDEV_MEDIA_LOADED 	0x0002	/* device figures are still valid */
#define	SDEV_WAITING	 	0x0004	/* a process is waiting for this */
#define	SDEV_OPEN	 	0x0008	/* at least 1 open session */
#define	SDEV_DBX		0x00f0	/* debuging flags (scsi_debug.h) */
#define SDEV_EJECTING		0x0100	/* eject on device close */
	u_int8_t quirks;		/* per-device oddities */
#define	SDEV_AUTOSAVE		0x01	/* do implicit SAVEDATAPOINTER on disconnect */
#define	SDEV_NOSYNCWIDE		0x02	/* does not grok SDTR or WDTR */
#define	SDEV_NOLUNS		0x04	/* does not grok LUNs */
#define	SDEV_FORCELUNS		0x08	/* prehistoric drive/ctlr groks LUNs */
#define	SDEV_NOMODESENSE	0x10	/* removable media/optical drives */
#define	SDEV_NOSTARTUNIT	0x20	/* do not issue start unit requests in sd.c */
#define	SDEV_NOTAGS		0x40	/* lies about having tagged queueing */
	u_int8_t inquiry_flags;		/* copy of flags from probe INQUIRY */
	struct	scsi_device *device;	/* device entry points etc. */
	void	*device_softc;		/* needed for call to foo_start */
	struct	scsi_adapter *adapter;	/* adapter entry points etc. */
	void	*adapter_softc;		/* needed for call to foo_scsi_cmd */
};

int	scsiprint __P((void *, const char *));

/*
 * This describes matching information for scsi_inqmatch().  The more things
 * match, the higher the configuration priority.
 */
struct scsi_inquiry_pattern {
	u_int8_t type;
	boolean removable;
	char *vendor;
	char *product;
	char *revision;
};

/*
 * One of these is allocated and filled in for each scsi bus.
 * it holds pointers to allow the scsi bus to get to the driver
 * That is running each LUN on the bus
 * it also has a template entry which is the prototype struct
 * supplied by the adapter driver, this is used to initialise
 * the others, before they have the rest of the fields filled in
 */
struct scsibus_softc {
	struct device sc_dev;
	struct scsi_link *adapter_link;		/* prototype supplied by adapter */
	struct scsi_link ***sc_link;
	u_int16_t moreluns;
	u_int8_t sc_buswidth;
};

/*
 * This is used to pass information from the high-level configuration code
 * to the device-specific drivers.
 */
struct scsibus_attach_args {
	struct scsi_link *sa_sc_link;
	struct scsi_inquiry_data *sa_inqbuf;
};

/*
 * Each scsi transaction is fully described by one of these structures
 * It includes information about the source of the command and also the
 * device and adapter for which the command is destined.
 * (via the scsi_link structure)
 */
struct scsi_xfer {
	LIST_ENTRY(scsi_xfer) free_list;
	int	flags;
	struct	scsi_link *sc_link;	/* all about our device and adapter */
	int	retries;		/* the number of times to retry */
	int	timeout;		/* in milliseconds */
	struct	scsi_generic *cmd;	/* The scsi command to execute */
	int	cmdlen;			/* how long it is */
	u_char	*data;			/* dma address OR a uio address */
	int	datalen;		/* data len (blank if uio)    */
	size_t	resid;			/* how much buffer was not touched */
	int	error;			/* an error value	*/
	struct	buf *bp;		/* If we need to associate with a buf */
	struct	scsi_sense_data	sense; /* 32 bytes*/
	/*
	 * Believe it or not, Some targets fall on the ground with
	 * anything but a certain sense length.
	 */
	int	req_sense_length;	/* Explicit request sense length */
	u_int8_t status;		/* SCSI status */
	struct	scsi_generic cmdstore;	/* stash the command in here */
};

/*
 * Per-request Flag values
 */
#define	SCSI_NOSLEEP	0x0001	/* don't sleep */
#define	SCSI_POLL	0x0002	/* poll for completion */
#define	SCSI_AUTOCONF	0x0003	/* shorthand for SCSI_POLL | SCSI_NOSLEEP */
#define	SCSI_USER	0x0004	/* Is a user cmd, call scsi_user_done	*/
#define	ITSDONE		0x0008	/* the transfer is as done as it gets	*/
#define	INUSE		0x0010	/* The scsi_xfer block is in use	*/
#define	SCSI_SILENT	0x0020	/* don't announce NOT READY or MEDIA CHANGE */
#define	SCSI_IGNORE_NOT_READY		0x0040	/* ignore NOT READY */
#define	SCSI_IGNORE_MEDIA_CHANGE	0x0080	/* ignore MEDIA CHANGE */
#define	SCSI_IGNORE_ILLEGAL_REQUEST	0x0100	/* ignore ILLEGAL REQUEST */
#define	SCSI_RESET	0x0200	/* Reset the device in question		*/
#define	SCSI_DATA_UIO	0x0400	/* The data address refers to a UIO	*/
#define	SCSI_DATA_IN	0x0800	/* expect data to come INTO memory	*/
#define	SCSI_DATA_OUT	0x1000	/* expect data to flow OUT of memory	*/
#define	SCSI_TARGET	0x2000	/* This defines a TARGET mode op.	*/
#define	SCSI_ESCAPE	0x4000	/* Escape operation			*/

/*
 * Escape op codes.  This provides an extensible setup for operations
 * that are not scsi commands.  They are intended for modal operations.
 */

#define SCSI_OP_TARGET	0x0001
#define	SCSI_OP_RESET	0x0002
#define	SCSI_OP_BDINFO	0x0003

/*
 * Error values an adapter driver may return
 */
#define XS_NOERROR	0	/* there is no error, (sense is invalid)  */
#define XS_SENSE	1	/* Check the returned sense for the error */
#define	XS_DRIVER_STUFFUP 2	/* Driver failed to perform operation	  */
#define XS_SELTIMEOUT	3	/* The device timed out.. turned off?	  */
#define XS_TIMEOUT	4	/* The Timeout reported was caught by SW  */
#define XS_BUSY		5	/* The device busy, try again later?	  */

caddr_t scsi_inqmatch __P((struct scsi_inquiry_data *, caddr_t, int,
	int, int *));

struct scsi_xfer *scsi_get_xs __P((struct scsi_link *, int));
void scsi_free_xs __P((struct scsi_xfer *, int));
int scsi_execute_xs __P((struct scsi_xfer *));
u_long scsi_size __P((struct scsi_link *, int));
int scsi_test_unit_ready __P((struct scsi_link *, int));
int scsi_change_def __P((struct scsi_link *, int));
int scsi_inquire __P((struct scsi_link *, struct scsi_inquiry_data *, int));
int scsi_prevent __P((struct scsi_link *, int, int));
int scsi_start __P((struct scsi_link *, int, int));
void scsi_done __P((struct scsi_xfer *));
void scsi_user_done __P((struct scsi_xfer *));
int scsi_scsi_cmd __P((struct scsi_link *, struct scsi_generic *,
	int cmdlen, u_char *data_addr, int datalen, int retries,
	int timeout, struct buf *bp, int flags));
int scsi_do_ioctl __P((struct scsi_link *, dev_t, u_long, caddr_t,
	int, struct proc *));
int scsi_do_safeioctl __P((struct scsi_link *, dev_t, u_long, caddr_t,
	int, struct proc *));
void sc_print_addr __P((struct scsi_link *));

void show_scsi_xs __P((struct scsi_xfer *));
void scsi_print_sense __P((struct scsi_xfer *, int));
void show_scsi_cmd __P((struct scsi_xfer *));
void show_mem __P((u_char *, int));
int scsi_probe_busses __P((int, int, int));
void scsi_strvis __P((u_char *, u_char *, int));

static __inline void _lto2b __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto3b __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto4b __P((u_int32_t val, u_int8_t *bytes));
static __inline u_int32_t _2btol __P((u_int8_t *bytes));
static __inline u_int32_t _3btol __P((u_int8_t *bytes));
static __inline u_int32_t _4btol __P((u_int8_t *bytes));

static __inline void _lto2l __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto3l __P((u_int32_t val, u_int8_t *bytes));
static __inline void _lto4l __P((u_int32_t val, u_int8_t *bytes));
static __inline u_int32_t _2ltol __P((u_int8_t *bytes));
static __inline u_int32_t _3ltol __P((u_int8_t *bytes));
static __inline u_int32_t _4ltol __P((u_int8_t *bytes));

static __inline void
_lto2b(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = (val >> 8) & 0xff;
	bytes[1] = val & 0xff;
}

static __inline void
_lto3b(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = (val >> 16) & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = val & 0xff;
}

static __inline void
_lto4b(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = (val >> 24) & 0xff;
	bytes[1] = (val >> 16) & 0xff;
	bytes[2] = (val >> 8) & 0xff;
	bytes[3] = val & 0xff;
}

static __inline u_int32_t
_2btol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = (bytes[0] << 8) |
	     bytes[1];
	return (rv);
}

static __inline u_int32_t
_3btol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = (bytes[0] << 16) |
	     (bytes[1] << 8) |
	     bytes[2];
	return (rv);
}

static __inline u_int32_t
_4btol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = (bytes[0] << 24) |
	     (bytes[1] << 16) |
	     (bytes[2] << 8) |
	     bytes[3];
	return (rv);
}

static __inline void
_lto2l(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = val & 0xff;
	bytes[1] = (val >> 8) & 0xff;
}

static __inline void
_lto3l(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = val & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = (val >> 16) & 0xff;
}

static __inline void
_lto4l(val, bytes)
	u_int32_t val;
	u_int8_t *bytes;
{

	bytes[0] = val & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = (val >> 16) & 0xff;
	bytes[3] = (val >> 24) & 0xff;
}

static __inline u_int32_t
_2ltol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = bytes[0] |
	     (bytes[1] << 8);
	return (rv);
}

static __inline u_int32_t
_3ltol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = bytes[0] |
	     (bytes[1] << 8) |
	     (bytes[2] << 16);
	return (rv);
}

static __inline u_int32_t
_4ltol(bytes)
	u_int8_t *bytes;
{
	register u_int32_t rv;

	rv = bytes[0] |
	     (bytes[1] << 8) |
	     (bytes[2] << 16) |
	     (bytes[3] << 24);
	return (rv);
}

#endif /* SCSI_SCSICONF_H */
