/*      $OpenBSD: ip_gre.h,v 1.1 2000/01/07 21:38:01 angelos Exp $ */
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

/*
 * Names for GRE sysctl objects
 */
#define GRECTL_ALLOW    1		/* accept incoming GRE packets */
#define GRECTL_MAXID    2
 
#define GRECTL_NAMES { \
        { 0, 0 }, \
        { "allow", CTLTYPE_INT }, \
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
void gre_input __P((struct mbuf *, ...));
void gre_mobile_input __P((struct mbuf *, ...));

int     ipmobile_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
int     gre_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));

#ifndef MROUTING
void gre_ipip_input __P((struct mbuf *, ...));
#endif
#endif /* _KERNEL */
