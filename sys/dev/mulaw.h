/*	$OpenBSD: mulaw.h,v 1.4 1998/10/28 18:01:02 downsj Exp $	*/
/*	$NetBSD: mulaw.h,v 1.10 1998/08/09 19:22:15 mycroft Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl.
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

/* Convert 8-bit mu-law to 16 bit unsigned linear. */
extern void mulaw_to_ulinear16 __P((void *, u_char *buf, int cnt));
/* Convert 8-bit mu-law to 16 bit signed linear. */
extern void mulaw_to_slinear16 __P((void *, u_char *buf, int cnt));
/* Convert 8-bit mu-law to/from 8 bit unsigned linear. */
extern void mulaw_to_ulinear8 __P((void *, u_char *buf, int cnt));
extern void ulinear8_to_mulaw __P((void *, u_char *buf, int cnt));
/* Convert 8-bit mu-law to/from 8 bit signed linear. */
extern void mulaw_to_slinear8 __P((void *, u_char *buf, int cnt));
extern void slinear8_to_mulaw __P((void *, u_char *buf, int cnt));
/* Convert 8-bit a-law to 16 bit unsigned linear. */
extern void alaw_to_ulinear16 __P((void *, u_char *buf, int cnt));
/* Convert 8-bit a-law to 16 bit signed linear. */
extern void alaw_to_slinear16 __P((void *, u_char *buf, int cnt));
/* Convert 8-bit a-law to/from 8 bit unsigned linear. */
extern void alaw_to_ulinear8 __P((void *, u_char *buf, int cnt));
extern void ulinear8_to_alaw __P((void *, u_char *buf, int cnt));
/* Convert 8-bit a-law to/from 8 bit signed linear. */
extern void alaw_to_slinear8 __P((void *, u_char *buf, int cnt));
extern void slinear8_to_alaw __P((void *, u_char *buf, int cnt));
