/*	$OpenBSD: todclockvar.h,v 1.2 2004/05/19 03:17:07 drahn Exp $	*/
/*	$NetBSD: todclockvar.h,v 1.1 2002/02/10 12:26:00 chris Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 *	This product includes software developed by Mark Brinicombe
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * todclockvar.h
 *
 * structures and variables for the todclock device
 *
 * Created      : 12/02/97
 */

/*
 * Attach args for todclock device
 */

struct todclock_attach_args {
	const char	*ta_name;			/* device name */
	void	*ta_rtc_arg;				/* arg to read/write */
	int	(*ta_rtc_write) (void *, rtc_t *);	/* function to write rtc */
	int	(*ta_rtc_read)  (void *, rtc_t *);	/* function to read rtc */
	int	ta_flags;				/* flags */
#define TODCLOCK_FLAG_FAKE	0x01			/* tod service is faked */
};

/* End of todclockvar.h */
