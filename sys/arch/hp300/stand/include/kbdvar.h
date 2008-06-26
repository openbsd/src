/*	$OpenBSD: kbdvar.h,v 1.5 2008/06/26 05:42:10 ray Exp $	*/
/*	$NetBSD: kbdvar.h,v 1.1 1997/04/14 19:00:13 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Structure definitions and prototypes for the indirect keyboard driver
 * for standalone ITE.
 */

struct kbdsw {
	int	(*k_getc)(void);	/* get character */
	void	(*k_nmi)(void);		/* handle non-maskable interrupt */
	int	(*k_init)(void);	/* probe/initialize keyboard */
};

#ifdef ITECONSOLE

extern struct kbdsw kbdsw[];

int	kbdgetc(void);
void	kbdinit(void);
void	kbdnmi(void);

#ifdef HIL_KEYBOARD
int	hilkbd_getc(void);
int	hilkbd_init(void);
void	hilkbd_nmi(void);
#endif

#ifdef DOMAIN_KEYBOARD
int	dnkbd_getc(void);
int	dnkbd_init(void);
void	dnkbd_nmi(void);
#endif
#endif /* ITECONSOLE */
