/*	$OpenBSD: ieee1394var.h,v 1.4 2002/12/30 11:19:45 tdeval Exp $	*/
/*	$NetBSD: ieee1394var.h,v 1.15 2002/02/27 05:04:28 jmc Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_IEEE1394_IEEE1394VAR_H_
#define	_DEV_IEEE1394_IEEE1394VAR_H_

extern const char * const ieee1394_speeds[];

struct ieee1394_softc;
struct ieee1394_node;

/* These buffers have no reference counting.  It is assumed that
 * the upper level buffer (struct buf or struct mbuf) will have the
 * requisite reference counting.
 */
typedef struct ieee1394_abuf {
	struct ieee1394_softc	 *ab_req;	/* requestor */
	struct ieee1394_softc	 *ab_resp;	/* response */
	u_int32_t		 *ab_data;
	struct uio		 *ab_uio;
	u_int64_t		  ab_addr;
	u_int8_t		  ab_tcode;
	u_int8_t		  ab_tlabel;
	u_int32_t		  ab_length;
	u_int32_t		  ab_retlen;	/* length returned from read. */
	u_int32_t		  ab_retries;
	void			(*ab_cb)(struct ieee1394_abuf *, int);
	void			 *ab_cbarg;
} ieee1394_abuf;

typedef struct ieee1394_callbacks {
	void	(*cb1394_busreset)(struct ieee1394_softc *);
	void	(*cb1394_at_queue)(struct ieee1394_softc *, int type,
				   struct ieee1394_abuf *);
	void	(*cb1394_at_done) (struct ieee1394_softc *,
				   struct ieee1394_abuf *);
} ieee1394_callbacks;

typedef struct ieee1394_attach_args {
	char		  name[7];
	u_int8_t	  uid[8];
	u_int16_t	  nodeid;
	int		(*read) (struct ieee1394_abuf *);
	int		(*write)(struct ieee1394_abuf *);
	int		(*inreg)(struct ieee1394_abuf *, int);
	int		(*unreg)(struct ieee1394_abuf *, int);
} ieee1394_attach_args;

typedef struct ieee1394_softc {
	struct device		  sc1394_dev;
	struct device		 *sc1394_if;	/* Move to fwohci level. */

	struct ieee1394_callbacks sc1394_callback; /* Nuke probably. */
	u_int32_t		 *sc1394_configrom;
	u_int32_t		  sc1394_configrom_len;	/* quadlets. */
	u_int32_t		  sc1394_max_receive;
	u_int8_t		  sc1394_guid[8];
	u_int8_t		  sc1394_link_speed;	/* IEEE1394_SPD_* */
	u_int16_t		  sc1394_node_id;	/* my node id
							 * in network order */
	int	(*sc1394_ifoutput)(struct device *, struct mbuf *,
				   void (*)(struct device *, struct mbuf *));
				/* Nuke. */
	int	(*sc1394_ifinreg)(struct device *, u_int32_t, u_int32_t,
				   void (*)(struct device *, struct mbuf *));
				/* Nuke */
	int	(*sc1394_ifsetiso)(struct device *, u_int32_t, u_int32_t,
			u_int32_t, void (*)(struct device *, struct mbuf *));
				/* Nuke */

	LIST_ENTRY(ieee1394_softc) sc1394_node;
} ieee1394_softc;

typedef struct ieee1394_node {
	struct device		 node_dev;

	struct ieee1394_softc	*node_sc;	/* owning bus */
	u_int32_t		*node_configrom;
	size_t			 node_configrom_len;
} ieee1394_node;

int ieee1394_init(struct ieee1394_softc *);

#define	IEEE1394_ARGTYPE_PTR			0
#define	IEEE1394_ARGTYPE_MBUF			1

#endif	/* _DEV_IEEE1394_IEEE1394VAR_H_ */
