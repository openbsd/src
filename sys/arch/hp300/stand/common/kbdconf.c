/*	$OpenBSD: kbdconf.c,v 1.2 2008/06/26 05:42:10 ray Exp $	*/
/*	$NetBSD: kbdconf.c,v 1.1 1997/04/14 19:00:12 thorpej Exp $	*/

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
 * Keyboard support configuration.
 */

#ifdef ITECONSOLE

#include <sys/param.h>

#include "samachdep.h"
#include "kbdvar.h"

#ifndef SMALL

/*
 * Note, these are arranged in order of preference.  The first `init'
 * routine to report success gets to play.
 */
struct kbdsw kbdsw[] = {
#ifdef HIL_KEYBOARD
	{ hilkbd_getc, hilkbd_nmi, hilkbd_init },
#endif
#ifdef DOMAIN_KEYBOARD
	{ dnkbd_getc, dnkbd_nmi, dnkbd_init },
#endif
	{ NULL, NULL, NULL },
};

#endif /* SMALL */

#endif /* ITECONSOLE */
