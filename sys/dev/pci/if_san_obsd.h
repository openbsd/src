/*	$OpenBSD: if_san_obsd.h,v 1.5 2005/04/01 21:42:36 canacar Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */



#ifndef __IF_SAN_OBSD_H
# define __IF_SAN_OBSD_H

#define IF_IFACE_V35		0x1001
#define IF_IFACE_T1		0x1002
#define IF_IFACE_E1		0x1003
#define IF_IFACE_SYNC_SERIAL	0x1004

#define IF_PROTO_HDLC		0x2001
#define IF_PROTO_PPP		0x2002
#define IF_PROTO_CISCO		0x2003
#define IF_PROTO_FR		0x2004
#define IF_PROTO_FR_ADD_PVC	0x2005
#define IF_PROTO_FR_DEL_PVC	0x2006
#define IF_PROTO_X25		0x2007
#define WAN_PROTO_X25		0x2007

#define IF_GET_PROTO		0x3001

#define te1_settings		void
#define sync_serial_settings	void

#define ifs_size		data_length
#define ifs_te1			data
#define ifs_sync		data
#define ifs_cisco		data
#define ifs_fr			data
#define ifs_fr_pvc		data
#define ifs_fr_pvc_info	data


#define SANCFG_LBO_FLAG		0x0001
#define SANCFG_CLOCK_FLAG	0x0002

typedef struct { int dummy; } cisco_proto, fr_proto, fr_proto_pvc;
struct if_settings {
	unsigned int	type;
	unsigned int	data_length;
	unsigned long	flags;
	void*		data;
};

typedef struct {
	int		proto;
	int		iface;
	char		hwprobe[100];
	sdla_te_cfg_t	te_cfg;
	union {
		cisco_proto	cisco;
		fr_proto	fr;
		fr_proto_pvc	fr_pvc;
	} protocol;
} wanlite_def_t;

/* WANPIPE Generic function interface */
# if defined(_KERNEL)
struct ifnet	*wanpipe_generic_alloc (sdla_t *);
void		 wanpipe_generic_free (struct ifnet *);
int		 wanpipe_generic_name (sdla_t *, char *, int);
int		 wanpipe_generic_register(sdla_t *, struct ifnet *, char *);
void		 wanpipe_generic_unregister(struct ifnet *);
int		 wanpipe_generic_open(struct ifnet *);
int		 wanpipe_generic_close(struct ifnet *);
int		 wanpipe_generic_input(struct ifnet *, struct mbuf *);
int		 wanpipe_generic_tx_timeout(struct ifnet *);
int		 wp_lite_set_proto(struct ifnet *, struct ifreq *);
int		 wp_lite_set_te1_cfg(struct ifnet *, struct ifreq *);
# endif
#endif /* __IF_SAN_OBSD_H */
