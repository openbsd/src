/*
 * Copyright (c) 1995 Mats O Jansson.  All rights reserved.
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
 *	This product includes software developed by Mats O Jansson.
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

#ifndef LINT
static char rcsid[] = "$Id: mopdef.c,v 1.1.1.1 1996/09/21 13:49:16 maja Exp $";
#endif

#define MOPDEF_SURPESS_EXTERN
#include "common/mopdef.h"

char dl_mcst[6] = MOP_DL_MULTICAST;	/* Dump/Load Multicast         */
char rc_mcst[6] = MOP_RC_MULTICAST;	/* Remote Console Multicast    */
char dl_802_proto[5] = MOP_K_PROTO_802_DL; /* MOP Dump/Load 802.2      */
char rc_802_proto[5] = MOP_K_PROTO_802_RC; /* MOP Remote Console 802.2 */
char lp_802_proto[5] = MOP_K_PROTO_802_LP; /* Loopback 802.2           */

int
mopdef_dummy()
{
	/* Just to keep them as variables */
	return(dl_mcst[0]-rc_mcst[0]-
	       lp_802_proto[1]-rc_802_proto[1]-lp_802_proto[1]);
}
