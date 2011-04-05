/*	$OpenBSD: linux_emuldata.h,v 1.5 2011/04/05 15:44:40 pirofti Exp $	*/
/*	$NetBSD: linux_emuldata.h,v 1.4 2002/02/15 16:48:02 christos Exp $	*/
/*-
 * Copyright (c) 1998,2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Eric Haszlakiewicz.
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
#ifndef _LINUX_EMULDATA_H
#define _LINUX_EMULDATA_H

/*
 * This is auxiliary data the linux compat code
 * needs to do its work.  A pointer to it is
 * stored in the emuldata field of the proc
 * structure.
 */
struct linux_emuldata {
	caddr_t	p_break;	/* Cached per-process break value */	

	void *child_set_tid;	/* Let the child set the thread ID at start */
	void *child_clear_tid;	/* Let the child clear the thread ID on exit */
	unsigned child_tls_base;/* Set the Thread Local Storage on clone */ 

	/* Same as above, passed by the parent when forking. */
	void *my_set_tid;
	void *my_clear_tid;
	unsigned my_tls_base;
};
#endif /* !_LINUX_EMULDATA_H */
