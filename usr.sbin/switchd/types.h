/*	$OpenBSD: types.h,v 1.1 2016/07/19 16:54:26 reyk Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _SWITCHD_TYPES_H
#define _SWITCHD_TYPES_H

#ifndef SWITCHD_USER
#define SWITCHD_USER	"_hostapd"
#endif

#ifndef SWITCHD_NAME
#define SWITCHD_NAME	"switch"
#endif

#ifndef SWITCHD_CONFIG
#define SWITCHD_CONFIG	"/etc/" SWITCHD_NAME "d.conf"
#endif
#define SWITCHD_SOCKET	"/var/run/" SWITCHD_NAME "d.sock"

#define SWITCHD_CYCLE_BUFFERS	8	/* # of static buffers for mapping */
#define SWITCHD_READ_BUFFER	0xffff
#define SWITCHD_MSGBUF_MAX	0xffff

#define SWITCHD_CTLR_PORT	6633	/* Previously used by OpenFlow */
#define SWITCHD_CTLR_IANA_PORT	6653	/* Assigned by IANA for OpenFlow */

#define SWITCHD_CACHE_MAX	4096	/* Default MAC address cache limit */
#define SWITCHD_CACHE_TIMEOUT	240	/* t/o in seconds for learned MACs */

#define SWITCHD_OFCCONN_TIMEOUT	20	/* connect timeout for OpenFlow ch. */

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN		6
#endif

struct constmap {
	unsigned int	 cm_type;
	const char	*cm_name;
	const char	*cm_descr;
};

enum imsg_type {
	IMSG_NONE	= 0,
	IMSG_CTL_VERBOSE,
	IMSG_CTL_NOTIFY,
	IMSG_CTL_OK,
	IMSG_CTL_FAIL,
	IMSG_CTL_END,
	IMSG_CTL_RELOAD,
	IMSG_CTL_RESET,
	IMSG_CTL_SWITCH,
	IMSG_CTL_MAC,
	IMSG_CTL_SHOW_SUM,
	IMSG_CTL_DEVICE_CONNECT,
	IMSG_CTL_DEVICE_DISCONNECT
};

enum privsep_procid {
	PROC_PARENT	= 0,
	PROC_OFP,
	PROC_CONTROL,
	PROC_OFCCONN,
	PROC_MAX
} privsep_process;

enum blockmodes {
	BM_NORMAL,
	BM_NONBLOCK
};

enum flushmode {
	RESET_RELOAD	= 0,
	RESET_ALL
};

enum switch_conn_type {
	SWITCH_CONN_LOCAL,
	SWITCH_CONN_TCP,
	SWITCH_CONN_TLS
};

#ifndef nitems
#define nitems(_a)   (sizeof((_a)) / sizeof((_a)[0]))
#endif

#endif /* _SWITCHD_TYPES_H */
