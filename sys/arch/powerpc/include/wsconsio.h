/*	$OpenBSD: wsconsio.h,v 1.1 1998/09/27 03:55:57 rahnds Exp $	*/
/*	$NetBSD: wsconsio.h,v 1.1 1996/04/12 01:43:06 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Get keyboard type.  Keyboard type definitions are below.
 */
#define	WSCONSIO_KBD_GTYPE		_IOR('W',100,int)
#define		KBD_TYPE_LK201		0		/* lk-201 */
#define		KBD_TYPE_LK401		1		/* lk-401 */
#define		KBD_TYPE_PC		2		/* pc-like */

/*
 * If arg is one, don't process scancodes into characters
 */
#define	WSCONSIO_KBD_SCANCODES		_IO('W',101,int)

/*
 * Bell ioctls.
 */

struct wsconsio_bell_data {
	int	wbd_flags;
	int	wbd_pitch;		/* pitch, in Hz. */
	int	wbd_period;		/* period, in milliseconds. */
	int	wbd_volume;		/* percentage of maximum volume. */
};

#define	WSCONSIO_BELLDATA_PITCH		0x01	/* pitch data present */
#define	WSCONSIO_BELLDATA_PERIOD	0x02	/* period data present */
#define	WSCONSIO_BELLDATA_VOLUME	0x04	/* volume data present */

#define	WSCONSIO_BELL			_IO('W',102)
#define	WSCONSIO_COMPLEXBELL		_IOW('W',103,struct wsconsio_bell_data)
#define	WSCONSIO_SETBELL		_IOW('W',104,struct wsconsio_bell_data)
#define	WSCONSIO_GETBELL		_IOR('W',105,struct wsconsio_bell_data)
