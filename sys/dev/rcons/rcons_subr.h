/*	$NetBSD: rcons_subr.h,v 1.1 1995/10/04 23:57:28 pk Exp $ */

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	extracted from: @(#)rcons_subr.c	8.1 (Berkeley) 6/11/93
 */

extern void rcons_puts __P((struct rconsole *, unsigned char *, int));
extern void rcons_font __P((struct rconsole *));
extern void rcons_text __P((struct rconsole *, unsigned char *, int));
extern void rcons_pctrl __P((struct rconsole *, int));
extern void rcons_esc __P((struct rconsole *, int));
extern void rcons_doesc __P((struct rconsole *, int));
extern void rcons_cursor __P((struct rconsole *));
extern void rcons_invert __P((struct rconsole *, int));
extern void rcons_clear2eop __P((struct rconsole *));
extern void rcons_clear2eol __P((struct rconsole *));
extern void rcons_scroll __P((struct rconsole *, int));
extern void rcons_delchar __P((struct rconsole *, int));
extern void rcons_delline __P((struct rconsole *, int));
extern void rcons_insertchar __P((struct rconsole *, int));
extern void rcons_insertline __P((struct rconsole *, int));
