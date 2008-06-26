/*	$OpenBSD: auconv.h,v 1.8 2008/06/26 05:42:14 ray Exp $ */
/*	$NetBSD: auconv.h,v 1.5 1999/11/01 18:12:19 augustss Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/* Convert between signed and unsigned. */
extern void change_sign8(void *, u_char *, int);
extern void change_sign16_le(void *, u_char *, int);
extern void change_sign16_be(void *, u_char *, int);
/* Convert between little and big endian. */
extern void swap_bytes(void *, u_char *, int);
extern void swap_bytes_change_sign16_le(void *, u_char *, int);
extern void swap_bytes_change_sign16_be(void *, u_char *, int);
extern void change_sign16_swap_bytes_le(void *, u_char *, int);
extern void change_sign16_swap_bytes_be(void *, u_char *, int);
/* Byte expansion/contraction */
extern void linear8_to_linear16_le(void *, u_char *, int);
extern void linear8_to_linear16_be(void *, u_char *, int);
extern void linear16_to_linear8_le(void *, u_char *, int);
extern void linear16_to_linear8_be(void *, u_char *, int);
/* Byte expansion/contraction with sign change */
extern void ulinear8_to_linear16_le(void *, u_char *, int);
extern void ulinear8_to_linear16_be(void *, u_char *, int);
extern void linear16_to_ulinear8_le(void *, u_char *, int);
extern void linear16_to_ulinear8_be(void *, u_char *, int);

/* same as above, plus converting mono to stereo */
extern void noswap_bytes_mts(void *, u_char *, int);
extern void swap_bytes_mts(void *, u_char *, int);
extern void linear8_to_linear16_le_mts(void *, u_char *, int);
extern void linear8_to_linear16_be_mts(void *, u_char *, int);
extern void ulinear8_to_linear16_le_mts(void *, u_char *, int);
extern void ulinear8_to_linear16_be_mts(void *, u_char *, int);
extern void change_sign16_le_mts(void *, u_char *, int);
extern void change_sign16_be_mts(void *, u_char *, int);
extern void change_sign16_swap_bytes_le_mts(void *, u_char *, int);
extern void change_sign16_swap_bytes_be_mts(void *, u_char *, int);
void swap_bytes_change_sign16_le_mts(void *, u_char *, int);
void swap_bytes_change_sign16_be_mts(void *, u_char *, int);

/* 16-bit signed linear stereo to mono.  drops every other sample */
void linear16_decimator(void *, u_char *, int);
void linear16_to_linear8_le_stm(void *, u_char *, int);
void linear16_to_linear8_be_stm(void *, u_char *, int);
void linear16_to_ulinear8_le_stm(void *, u_char *, int);
void linear16_to_ulinear8_be_stm(void *, u_char *, int);
void change_sign16_le_stm(void *, u_char *, int);
void change_sign16_be_stm(void *, u_char *, int);
void swap_bytes_stm(void *, u_char *, int);
void swap_bytes_change_sign16_be_stm(void *, u_char *, int);
void change_sign16_swap_bytes_le_stm(void *, u_char *, int);

/* backwards compat for now */
#if BYTE_ORDER == LITTLE_ENDIAN
#define change_sign16 change_sign16_le
#define change_sign16_swap_bytes swap_bytes_change_sign16_le
#define swap_bytes_change_sign16 swap_bytes_change_sign16_le
#define linear8_to_linear16 linear8_to_linear16_le
#define ulinear8_to_linear16 ulinear8_to_linear16_le
#define linear8_to_linear16_mts linear8_to_linear16_le_mts
#define ulinear8_to_linear16_mts ulinear8_to_linear16_le_mts
#define change_sign16_mts change_sign16_le_mts
#define change_sign16_swap_bytes_mts change_sign16_swap_bytes_le_mts
#else
#define change_sign16 change_sign16_be
#define change_sign16_swap_bytes swap_bytes_change_sign16_be
#define swap_bytes_change_sign16 swap_bytes_change_sign16_be
#define linear8_to_linear16 linear8_to_linear16_be
#define ulinear8_to_linear16 ulinear8_to_linear16_be
#define linear8_to_linear16_mts linear8_to_linear16_be_mts
#define ulinear8_to_linear16_mts ulinear8_to_linear16_be_mts
#define change_sign16_mts change_sign16_be_mts
#define change_sign16_swap_bytes_mts change_sign16_swap_bytes_be_mts
#endif
