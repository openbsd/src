/*	$OpenBSD: itevar.h,v 1.1 1996/05/26 19:02:07 briggs Exp $	*/
/*	$NetBSD: itevar.h,v 1.1 1996/05/05 06:16:49 briggs Exp $	*/

/*
 * Copyright (c) 1995 Allen Briggs.  All rights reserved.
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
 *	This product includes software developed by Allen Briggs.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/adbsys.h>

int	ite_intr __P((adb_event_t *event));
int	iteon __P((dev_t dev, int flags));
int	iteoff __P((dev_t dev, int flags));

#ifndef CN_DEAD
#include <dev/cons.h>
#endif

void	itestop __P((struct tty * tp, int flag));
void	itestart __P((register struct tty * tp));
int	iteopen __P((dev_t dev, int mode, int devtype, struct proc * p));
int	iteclose __P((dev_t dev, int flag, int mode, struct proc * p));
int	iteread __P((dev_t dev, struct uio * uio, int flag));
int	itewrite __P((dev_t dev, struct uio * uio, int flag));
int	iteioctl __P((dev_t, int, caddr_t, int, struct proc *));
struct tty	*itetty __P((dev_t dev));

int	itecnprobe __P((struct consdev * cp));
int	itecninit __P((struct consdev * cp));
int	itecngetc __P((dev_t dev));
int	itecnputc __P((dev_t dev, int c));
