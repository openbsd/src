/*	$OpenBSD: mpls_proto.c,v 1.9 2014/12/05 15:50:04 mpi Exp $	*/

/*
 * Copyright (C) 1999, 2000 and 2001 AYAME Project, WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/radix.h>
#include <net/radix_mpath.h>

#include <netmpls/mpls.h>

/*
 * MPLS protocol family:
 */
struct protosw mplssw[] = {
{ 0,			&mplsdomain,		0,	0,
  0,			0,			0,	0,
  0,
  mpls_init,		0,			0,	0,	mpls_sysctl
},
{ SOCK_DGRAM,		&mplsdomain,		0,	PR_ATOMIC|PR_ADDR,
  0,			0,			0,	0,
  mpls_raw_usrreq,
  0,			0,			0,	0,	mpls_sysctl,
},
/* raw wildcard */
{ SOCK_RAW,		&mplsdomain,		0,	PR_ATOMIC|PR_ADDR,
  0,			0,			0,	0,
  mpls_raw_usrreq,
  0,			0,			0,	0,	mpls_sysctl,
},
};

struct domain mplsdomain = {
	AF_MPLS, "mpls", mpls_init, 0, 0,
	mplssw,
	&mplssw[nitems(mplssw)], 0,
	rn_mpath_inithead,
	offsetof(struct sockaddr_mpls, smpls_label) << 3,
	sizeof(struct sockaddr_mpls)
};
