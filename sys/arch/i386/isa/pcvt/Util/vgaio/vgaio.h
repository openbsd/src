/*	$OpenBSD: vgaio.h,v 1.3 1999/01/13 07:26:07 niklas Exp $	*/

/*
 * Copyright (c) 1994 Joerg Wunsch
 *
 * All rights reserved.
 *
 * This program is free software.
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
 *	This product includes software developed by Joerg Wunsch
 * 4. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $Header
 * The author may be reached unter <joerg_wunsch@uriah.sax.de>
 *
 * $Log: vgaio.h,v $
 * Revision 1.3  1999/01/13 07:26:07  niklas
 * RCSIds
 *
 * Revision 1.2  1996/03/19 21:09:52  mickey
 * Merging w/ NetBSD 021796.
 * speaker upgraded to the current.
 * some changes to the VM stuff (ie kern_thread.c added and so).
 *
 * Revision 1.1.1.1  1996/02/16 18:59:24  niklas
 * Checkin of the NetBSD src/, supped 960203
 *
 * Revision 1.3  1995/10/07 21:46:02  jtc
 * Overlay our pcvt with pcvt 3.32 sources.  All of our fixes have been
 * incorporated into the master sources, so it is unnecessary to resolve
 * all the conflicts that would occur if we let CVS "merge" the versions.
 *
 * Revision 1.2  1995/03/05  22:46:27  joerg
 * Upgrade to beta 3.20/b22
 *
 * Revision 1.1  1994/03/29  02:47:25  mycroft
 * pcvt 3.0, with some performance enhancements by Joerg Wunsch and me.
 *
 * Revision 1.2  1994/01/08  17:42:58  j
 * cleanup
 * made multiple commands per line work
 * wrote man page
 *
 *
 */

/* common structure to hold the definition for a VGA register */

#ifndef VGAIO_H
#define VGAIO_H

struct reg {
	int group, num;
};

#endif /* VGAIO_H */
