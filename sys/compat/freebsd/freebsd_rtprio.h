/*	$OpenBSD: freebsd_rtprio.h,v 1.3 1996/08/02 20:34:47 niklas Exp $	*/

/*
 * Copyright (c) 1994, Henrik Vestergaard Draboel
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
 *	This product includes software developed by (name).
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 *	from: rtprio.h,v 1.2 1994/10/02 04:45:59 davidg Exp
 */

#ifndef _FREEBSD_RTPRIO_H
#define _FREEBSD_RTPRIO_H

/*
 * Process realtime-priority specifications to rtprio.
 */

/* priority types */
#define FREEBSD_RTP_PRIO_REALTIME	0
#define FREEBSD_RTP_PRIO_NORMAL		1
#define FREEBSD_RTP_PRIO_IDLE		2

/* priority range */
#define FREEBSD_RTP_PRIO_MIN		0	/* Highest priority */
#define FREEBSD_RTP_PRIO_MAX		31	/* Lowest priority */

/*
 * rtprio() syscall functions
 */
#define FREEBSD_RTP_LOOKUP		0
#define FREEBSD_RTP_SET			1

#ifndef _LOCORE
struct freebsd_rtprio {
	u_short type;
	u_short prio;
};
#endif
#endif	/* !_FREEBSD_RTPRIO_H */
