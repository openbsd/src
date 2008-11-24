/*	$OpenBSD: btdev.h,v 1.5 2008/11/24 23:54:03 uwe Exp $	*/
/*	$NetBSD: btdev.h,v 1.1 2006/06/19 15:44:45 gdamore Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_BLUETOOTH_BTDEV_H_
#define _DEV_BLUETOOTH_BTDEV_H_

#include <netbt/bluetooth.h>

/*
 * Bluetooth Device attach arguments (from userland)
 */
struct btdev_attach_args {
	bdaddr_t	bd_laddr;	/* local address */
	bdaddr_t	bd_raddr;	/* remote address */
	uint16_t	bd_type;	/* device type */
	int		bd_mode;	/* link mode */

	union {
		struct {	/* HID arguments */
			uint16_t  hid_flags;/* HID flags */
			uint16_t  hid_ctl;  /* control PSM */
			uint16_t  hid_int;  /* interrupt PSM */
			void	 *hid_desc; /* HID descriptor */
			uint16_t  hid_dlen; /* descriptor length */
		} bdd_hid;

		struct {	/* HSET arguments */
			uint8_t   hset_channel; /* RFCOMM channel */
			uint8_t   hset_mtu;	/* SCO mtu */
			int	  hset_listen;	/* connect or listen */
		} bdd_hset;

		struct {	/* HF arguments */
			uint8_t   hf_channel; /* RFCOMM channel */
			uint8_t   hf_mtu;	/* SCO mtu */
			int	  hf_listen;	/* connect or listen */
		} bdd_hf;
	} bdd;
};

#define	bd_hid		bdd.bdd_hid
#define bd_hset		bdd.bdd_hset
#define bd_hf		bdd.bdd_hf

/* btdev type */
#define BTDEV_NONE		0x0000
#define BTDEV_HID		0x0001
#define BTDEV_HSET		0x0002
#define BTDEV_HF		0x0003

/* btdev link mode */
#define BTDEV_MODE_NONE		0
#define BTDEV_MODE_AUTH		1
#define BTDEV_MODE_ENCRYPT	2
#define BTDEV_MODE_SECURE	3

/* bthid flags */
#define BTHID_INITIATE		(1 << 0)	/* normally initiate */

/* btdev attach/detach ioctl's */
#define BTDEV_ATTACH		_IOW('b', 14, struct btdev_attach_args)
#define BTDEV_DETACH		_IOW('b', 15, struct btdev_attach_args)

#ifdef _KERNEL

/*
 * Bluetooth device header
 */
struct btdev {
	struct device		sc_dev;		/* system device */
	bdaddr_t		sc_addr;	/* device bdaddr */
	uint16_t		sc_type;	/* device type */

	LIST_ENTRY(btdev)	sc_next;
};

#define btdev_name(d)		(((struct btdev *)(d))->sc_dev.dv_xname)

#endif /* _KERNEL */

#endif /* _DEV_BLUETOOTH_BTDEV_H_ */
