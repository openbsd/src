/*	$OpenBSD: enc-proto.h,v 1.1 1998/03/12 04:48:47 art Exp $	*/
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)enc-proto.h	8.1 (Berkeley) 6/4/93
 *
 *	@(#)enc-proto.h	5.2 (Berkeley) 3/22/91
 */

/*
 * This source code is no longer held under any constraint of USA
 * `cryptographic laws' since it was exported legally.  The cryptographic
 * functions were removed from the code and a "Bones" distribution was
 * made.  A Commodity Jurisdiction Request #012-94 was filed with the
 * USA State Department, who handed it to the Commerce department.  The
 * code was determined to fall under General License GTDA under ECCN 5D96G,
 * and hence exportable.  The cryptographic interfaces were re-added by Eric
 * Young, and then KTH proceeded to maintain the code in the free world.
 *
 */

/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America is assumed
 * to require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* $KTH: enc-proto.h,v 1.8 1997/11/02 03:57:10 assar Exp $ */

#if	defined(ENCRYPTION)
Encryptions *findencryption __P((int));
Encryptions *finddecryption __P((int));
int EncryptAutoDec __P((int));
int EncryptAutoEnc __P((int));
int EncryptDebug __P((int));
int EncryptDisable __P((char*, char*));
int EncryptEnable __P((char*, char*));
int EncryptStart __P((char*));
int EncryptStartInput __P((void));
int EncryptStartOutput __P((void));
int EncryptStatus __P((void));
int EncryptStop __P((char*));
int EncryptStopInput __P((void));
int EncryptStopOutput __P((void));
int EncryptType __P((char*, char*));
int EncryptVerbose __P((int));
int net_write __P((unsigned char *, int));
void decrypt_auto __P((int));
void encrypt_auto __P((int));
void encrypt_debug __P((int));
void encrypt_dec_keyid __P((unsigned char*, int));
void encrypt_display __P((void));
void encrypt_enc_keyid __P((unsigned char*, int));
void encrypt_end __P((void));
void encrypt_gen_printsub __P((unsigned char*, int, unsigned char*, int));
void encrypt_init __P((char*, int));
void encrypt_is __P((unsigned char*, int));
void encrypt_list_types __P((void));
void encrypt_not __P((void));
void encrypt_printsub __P((unsigned char*, int, unsigned char*, int));
void encrypt_reply __P((unsigned char*, int));
void encrypt_request_end __P((void));
void encrypt_request_start __P((unsigned char*, int));
void encrypt_send_end __P((void));
void encrypt_send_keyid __P((int, unsigned char*, int, int));
void encrypt_send_request_end __P((void));
void encrypt_send_request_start __P((void));
void encrypt_send_support __P((void));
void encrypt_session_key __P((Session_Key*, int));
void encrypt_start __P((unsigned char*, int));
void encrypt_start_output __P((int));
void encrypt_support __P((unsigned char*, int));
void encrypt_verbose_quiet __P((int));
void encrypt_wait __P((void));
int encrypt_delay __P((void));

#ifdef	TELENTD
void encrypt_wait __P((void));
#else
void encrypt_display __P((void));
#endif

void cfb64_encrypt __P((unsigned char *, int));
int cfb64_decrypt __P((int));
void cfb64_init __P((int));
int cfb64_start __P((int, int));
int cfb64_is __P((unsigned char *, int));
int cfb64_reply __P((unsigned char *, int));
void cfb64_session __P((Session_Key *, int));
int cfb64_keyid __P((int, unsigned char *, int *));
void cfb64_printsub __P((unsigned char *, int, unsigned char *, int));

void ofb64_encrypt __P((unsigned char *, int));
int ofb64_decrypt __P((int));
void ofb64_init __P((int));
int ofb64_start __P((int, int));
int ofb64_is __P((unsigned char *, int));
int ofb64_reply __P((unsigned char *, int));
void ofb64_session __P((Session_Key *, int));
int ofb64_keyid __P((int, unsigned char *, int *));
void ofb64_printsub __P((unsigned char *, int, unsigned char *, int));

#endif
