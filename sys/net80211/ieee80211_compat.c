/*	$OpenBSD: ieee80211_compat.c,v 1.3 2004/10/04 16:01:46 mickey Exp $	*/
/*	$NetBSD: ieee80211_compat.c,v 1.3 2003/09/23 15:57:25 dyoung Exp $	*/

/*-
 * Copyright (c) 2003, 2004 David Young
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
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <net/if.h>
#include <net80211/ieee80211_compat.h>

#ifdef __NetBSD__
__KERNEL_RCSID(0, "$NetBSD: ieee80211_compat.c,v 1.3 2003/09/23 15:57:25 dyoung Exp $");
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
void
if_printf(struct ifnet *ifp, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	printf("%s: ", ifp->if_xname);
	vprintf(fmt, ap);

	va_end(ap);
	return;
}
#endif
