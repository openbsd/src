/*	$OpenBSD: kbd.h,v 1.1 1996/10/30 22:41:41 niklas Exp $ */

/*
 * Copyright (c) 1996 Niklas Hallqvist
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
 *	This product includes software developed by Niklas Hallqvist.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

extern void kbdattach __P((struct device *, struct wscons_idev_spec *));
extern void kbd_repeat __P((void *));
extern void kbd_input __P((int));
extern int kbdopen __P((dev_t, int, int, struct proc *));
extern int kbdclose __P((dev_t, int, int, struct proc *p));
extern int kbdread __P((dev_t, struct uio *, int));
extern int kbdwrite __P((dev_t, struct uio *, int));
extern int kbdioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
#ifdef notyet
extern int kbdpoll __P((dev_t, int, struct proc *));
#else
extern int kbdselect __P((dev_t, int, struct proc *));
#endif
extern int kbd_cngetc __P((dev_t));
extern void kbd_cnpollc __P((dev_t, int));
extern void wscons_kbd_bell __P((void));
