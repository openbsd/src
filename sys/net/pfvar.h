/*	$OpenBSD: pfvar.h,v 1.2 2001/06/24 21:24:13 kjell Exp $ */

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
 * REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

struct rule {
	u_int8_t	 action;
	u_int8_t	 direction;
	u_int8_t	 log;
	u_int8_t	 quick;
	u_int8_t	 keep_state;
	char		 ifname[16];
	struct ifnet	*ifp;
	u_int8_t	 proto;
	struct {
		u_int8_t	not;
		u_int32_t	addr,
				mask;
		u_int8_t	port_op;
		u_int16_t	port[2];
	}		 src,
			 dst;
	u_int8_t	 type,
			 code;
	u_int8_t	 flags,
			 flagset;
	struct rule	*next;
};

struct state {
	u_int8_t	 proto;
	u_int8_t	 direction;
	struct host {
		u_int32_t	addr;
		u_int16_t	port;
	}		 lan,
			 gwy,
			 ext;
	struct peer {
		u_int32_t	seqlo,
				seqhi;
		u_int8_t	state;
	}		 src,
			 dst;
	u_int32_t	 creation,
			 expire;
	u_int32_t	 packets,
			 bytes;
	struct state	*next;
};

struct nat {
	char		 ifname[16];
	struct ifnet	*ifp;
	u_int8_t	 proto;
	u_int8_t	 not;
	u_int32_t	 saddr,
			 smask,
			 daddr;
	struct nat	*next;
};

struct rdr {
	char		 ifname[16];
	struct ifnet	*ifp;
	u_int8_t	 proto;
	u_int8_t	 not;
	u_int32_t	 daddr,
			 dmask,
			 raddr;
	u_int16_t	 dport,
			 rport;
	struct rdr	*next;
};

struct status {
	u_int8_t	running;
	u_int32_t	bytes[2];
	u_int32_t	packets[2][2];
	u_int32_t	states,
			state_inserts,
			state_removals,
			state_searches;
	u_int32_t	since;
};

/*
 * ioctl parameter structure
 */

struct ioctlbuffer {
	u_int32_t	 size;
	u_int16_t	 entries;
	void		*buffer;
};

/*
 * ioctl operations
 */

#define DIOCSTART	_IO  ('D',  1)
#define DIOCSTOP	_IO  ('D',  2)
#define DIOCSETRULES	_IOWR('D',  3, struct ioctlbuffer)
#define DIOCGETRULES	_IOWR('D',  4, struct ioctlbuffer)
#define DIOCSETNAT	_IOWR('D',  5, struct ioctlbuffer)
#define DIOCGETNAT	_IOWR('D',  6, struct ioctlbuffer)
#define DIOCSETRDR	_IOWR('D',  7, struct ioctlbuffer)
#define DIOCGETRDR	_IOWR('D',  8, struct ioctlbuffer)
#define DIOCCLRSTATES	_IO  ('D',  9)
#define DIOCGETSTATES	_IOWR('D', 10, struct ioctlbuffer)
#define DIOCSETSTATUSIF _IOWR('D', 11, struct ioctlbuffer)
#define DIOCGETSTATUS	_IOWR('D', 12, struct ioctlbuffer)

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

int pf_test (int, struct ifnet *, struct mbuf **);

#endif /* _KERNEL */

#endif /* _NET_PFVAR_H_ */
