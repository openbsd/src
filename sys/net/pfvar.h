/*	$OpenBSD: pfvar.h,v 1.5 2001/06/24 23:44:00 art Exp $ */

/*
 * Copyright (c) 2001, Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer. 
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _NET_PFVAR_H_
#define _NET_PFVAR_H_

#include <sys/types.h>

enum	{ PF_IN=0, PF_OUT=1 };
enum	{ PF_PASS=0, PF_DROP=1, PF_DROP_RST=2 };

struct rule_addr {
	u_int32_t	addr;
	u_int32_t	mask;
	u_int16_t	port[2];
	u_int8_t	not;
	u_int8_t	port_op;
};

struct rule {
	char		 ifname[IFNAMSIZ];
	struct ifnet	*ifp;
	struct rule_addr src;
	struct rule_addr dst;
	struct rule	*next;

	u_int8_t	 action;
	u_int8_t	 direction;
	u_int8_t	 log;
	u_int8_t	 quick;

	u_int8_t	 keep_state;
	u_int8_t	 proto;
	u_int8_t	 type;
	u_int8_t	 code;

	u_int8_t	 flags;
	u_int8_t	 flagset;
};

struct state_host {
	u_int32_t	addr;
	u_int16_t	port;
};

struct state_peer {
	u_int32_t	seqlo;
	u_int32_t	seqhi;
	u_int8_t	state;
};

struct state {
	struct state	*next;
	struct state_host lan;
	struct state_host gwy;
	struct state_host ext;
	struct state_peer src;
	struct state_peer dst;
	u_int32_t	 creation;
	u_int32_t	 expire;
	u_int32_t	 packets;
	u_int32_t	 bytes;
	u_int8_t	 proto;
	u_int8_t	 direction;
};

struct nat {
	char		 ifname[IFNAMSIZ];
	struct ifnet	*ifp;
	struct nat	*next;
	u_int32_t	 saddr;
	u_int32_t	 smask;
	u_int32_t	 daddr;
	u_int8_t	 proto;
	u_int8_t	 not;
};

struct rdr {
	char		 ifname[IFNAMSIZ];
	struct ifnet	*ifp;
	struct rdr	*next;
	u_int32_t	 daddr;
	u_int32_t	 dmask;
	u_int32_t	 raddr;
	u_int16_t	 dport;
	u_int16_t	 rport;
	u_int8_t	 proto;
	u_int8_t	 not;
};

struct status {
	u_int32_t	running;
	u_int32_t	bytes[2];
	u_int32_t	packets[2][2];
	u_int32_t	states;
	u_int32_t	state_inserts;
	u_int32_t	state_removals;
	u_int32_t	state_searches;
	u_int32_t	since;
};

/*
 * ioctl parameter structure
 */

struct pfioc {
	u_int32_t	 size;
	u_int16_t	 entries;
	void		*buffer;
};

/*
 * ioctl operations
 */

#define DIOCSTART	_IO  ('D',  1)
#define DIOCSTOP	_IO  ('D',  2)
#define DIOCSETRULES	_IOWR('D',  3, struct pfioc)
#define DIOCGETRULES	_IOWR('D',  4, struct pfioc)
#define DIOCSETNAT	_IOWR('D',  5, struct pfioc)
#define DIOCGETNAT	_IOWR('D',  6, struct pfioc)
#define DIOCSETRDR	_IOWR('D',  7, struct pfioc)
#define DIOCGETRDR	_IOWR('D',  8, struct pfioc)
#define DIOCCLRSTATES	_IO  ('D',  9)
#define DIOCGETSTATES	_IOWR('D', 10, struct pfioc)
#define DIOCSETSTATUSIF _IOWR('D', 11, struct pfioc)
#define DIOCGETSTATUS	_IOWR('D', 12, struct pfioc)

/*
 * ioctl errors
 */

enum error_msg {
	NO_ERROR=0,
	ERROR_INVALID_OP=100,
	ERROR_ALREADY_RUNNING,
	ERROR_NOT_RUNNING,
	ERROR_INVALID_PARAMETERS,
	ERROR_MALLOC,
	MAX_ERROR_NUM
};


#ifdef _KERNEL

int	pf_test(int, struct ifnet *, struct mbuf **);

#endif /* _KERNEL */

#endif /* _NET_PFVAR_H_ */
