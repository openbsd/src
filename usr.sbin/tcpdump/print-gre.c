/*	$OpenBSD: print-gre.c,v 1.3 2002/09/18 18:49:03 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

#define	GRE_CP		0x8000		/* checksum present */
#define	GRE_RP		0x4000		/* routing present */
#define	GRE_KP		0x2000		/* key present */
#define	GRE_SP		0x1000		/* sequence# present */
#define	GRE_sP		0x0800		/* source routing */
#define	GRE_RECRS	0x0700		/* recursion count */
#define	GRE_VERS	0x0007		/* protocol version */

#define	GREPROTO_IP	0x0800		/* IP */

void
gre_print(const u_char *bp, u_int length)
{
	u_int len = length;
	u_int16_t flags, prot;

	if (len < 2)
		goto trunc;
	flags = EXTRACT_16BITS(bp);
	if ((flags & 7) != 0) {
		printf("gre: unknown version %u", flags & 7);
		return;
	}
	if (vflag) {
		printf("[%s%s%s%s%s] ",
		    (flags & GRE_CP) ? "C" : "",
		    (flags & GRE_RP) ? "R" : "",
		    (flags & GRE_KP) ? "K" : "",
		    (flags & GRE_SP) ? "S" : "",
		    (flags & GRE_sP) ? "s" : "");
	}
	len -= 2;
	bp += 2;

	if (len < 2)
		goto trunc;
	prot = EXTRACT_16BITS(bp);
	len -= 2;
	bp += 2;

	if ((flags & GRE_CP) | (flags & GRE_RP)) {
		if (len < 2)
			goto trunc;
		if (vflag)
			printf("sum 0x%x ", EXTRACT_16BITS(bp));
		bp += 2;
		len -= 2;

		if (len < 2)
			goto trunc;
		printf("off 0x%x ", EXTRACT_16BITS(bp));
		bp += 2;
		len -= 2;
	}

	if (flags & GRE_KP) {
		if (len < 4)
			goto trunc;
		printf("key=0x%x ", EXTRACT_32BITS(bp));
		bp += 4;
		len -= 4;
	}

	if (flags & GRE_SP) {
		if (len < 4)
			goto trunc;
		printf("seq=0x%x ", EXTRACT_32BITS(bp));
		bp += 4;
		len -= 4;
	}

	if (flags & GRE_RP) {
		/* Just skip over routing info */
		for (;;) {
			u_int16_t af;
			u_int8_t sreoff;
			u_int8_t srelen;

			if (len < 4)
				goto trunc;
			af = EXTRACT_16BITS(bp);
			sreoff = *(bp + 2);
			srelen = *(bp + 3);
			bp += 4;
			len -= 4;

			if (af == 0 && srelen == 0)
				break;

			if (len < srelen)
				goto trunc;
			bp += srelen;
			len -= srelen;
		}
	}

	switch (prot) {
	case GREPROTO_IP:
		ip_print(bp, len);
		break;
	default:
		printf("gre-proto-0x%x", prot);
	}
	return;

trunc:
	printf("[|gre]");
}
