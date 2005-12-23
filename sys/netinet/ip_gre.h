/*      $OpenBSD: ip_gre.h,v 1.7 2005/12/23 13:01:23 claudio Exp $ */
/*	$NetBSD: ip_gre.h,v 1.3 1998/10/07 23:33:02 thorpej Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#ifndef _NETINET_IP_GRE_H_
#define _NETINET_IP_GRE_H_

/* Protocol number for Cisco's WCCP
 * The Internet Draft is:
 *   draft-forster-wrec-wccp-v1-00.txt
 */
#define GREPROTO_WCCP	0x883e

/*
 * Names for GRE sysctl objects
 */
#define GRECTL_ALLOW    1		/* accept incoming GRE packets */
#define GRECTL_WCCP     2		/* accept WCCPv1-style GRE packets */
#define GRECTL_MAXID    3

#define GRECTL_NAMES { \
        { 0, 0 }, \
        { "allow", CTLTYPE_INT }, \
        { "wccp", CTLTYPE_INT }, \
}

/*
 * Names for MobileIP sysctl objects
 */
#define MOBILEIPCTL_ALLOW    1		/* accept incoming MobileIP packets */
#define MOBILEIPCTL_MAXID    2

#define MOBILEIPCTL_NAMES { \
        { 0, 0 }, \
        { "allow", CTLTYPE_INT }, \
}

#ifdef _KERNEL
void gre_input(struct mbuf *, ...);
void gre_mobile_input(struct mbuf *, ...);

int     ipmobile_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int     gre_sysctl(int *, u_int, void *, size_t *, void *, size_t);

#endif /* _KERNEL */
#endif /* _NETINET_IP_GRE_H_ */
