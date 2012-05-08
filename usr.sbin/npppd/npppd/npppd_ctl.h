/*	$OpenBSD: npppd_ctl.h,v 1.5 2012/05/08 13:15:12 yasuoka Exp $ */

/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef NPPPD_CTL_H
#define NPPPD_CTL_H 1

/** Message size of npppd control protocol messages */
#define	NPPPD_CTL_MSGSZ			2048

/** Path of npppd control protocol's socket */
#define	NPPPD_CTL_SOCK_PATH		"/var/run/npppd_ctl"

/** Size of username */
#define	NPPPD_CTL_USERNAME_SIZE		256

/** Npppd control protocol command */
enum npppd_ctl_cmd {
	/** Connected user statistics */
	NPPPD_CTL_CMD_WHO,

	/** Disconnect specified user's sessions */
	NPPPD_CTL_CMD_DISCONNECT_USER,

	/** Set client authentication information */
	NPPPD_CTL_CMD_TERMID_SET_AUTH,

	/** Reset npppd's routing information to the system routing table */
	NPPPD_CTL_CMD_RESET_ROUTING_TABLE,

	/** Disconnect specified ppp-id's sessions */
	NPPPD_CTL_CMD_DISCONNECT
};

struct npppd_ctl_who_request {
	enum npppd_ctl_cmd	cmd;
};

struct npppd_who {
	/** Ppp Id */
	u_int		ppp_id;

	/** Username */
	char		username[NPPPD_CTL_USERNAME_SIZE];

	/** Start time */
	time_t		time;

	/** Elapsed time */
	uint32_t	duration_sec;

	/** Concentrated interface */
	char		ifname[IF_NAMESIZE];

	/** Authenticated realm name */
	char		rlmname[32];

	/** Tunnel protocol name */
	char		tunnel_proto[16];

	/** Tunnel peer address */
	union {
		struct sockaddr_in  peer_in4;
		struct sockaddr_in6 peer_in6;
		struct sockaddr_dl  peer_dl;
	}		tunnel_peer;

	/** Framed IP Address */
	struct in_addr	framed_ip_address;

	/** Numbers of input packets */
	uint32_t	ipackets;

	/** Numbers of output packets */
	uint32_t	opackets;

	/** Numbers of input error packets */
	uint32_t	ierrors;

	/** Numbers of output error packets */
	uint32_t	oerrors;

	/** Bytes of input packets */
	uint64_t	ibytes;

	/** Bytes of output packets */
	uint64_t	obytes;
};

struct npppd_ctl_who_response {
	int			count;
	struct npppd_who	entry[0];
};

struct npppd_ctl_disconnect_user_request {
	enum npppd_ctl_cmd	cmd;
	char			username[NPPPD_CTL_USERNAME_SIZE];
};

struct npppd_ctl_termid_set_auth_request {
	enum npppd_ctl_cmd	cmd;
	u_int			use_ppp_id:1,
				use_framed_ip_address;
	u_int			ppp_id;
	struct in_addr		framed_ip_address;
	char authid[33];
};

struct npppd_ctl_disconnect_request {
	enum npppd_ctl_cmd	cmd;
	int			count;
	u_int			ppp_id[0];
};

struct npppd_ctl_disconnect_response {
	int			count;
};

#endif
