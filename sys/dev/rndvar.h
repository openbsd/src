/*	$OpenBSD: rndvar.h,v 1.3 1996/08/11 07:31:32 dm Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff.
 * 
 * This software derived from one contributed by Theodore Ts'o.
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
 *	This product includes software developed by Theodore Ts'o.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */

#ifndef __RNDVAR_H__
#define __RNDVAR_H__

#define POOLWORDS 128    /* Power of 2 - note that this is 32-bit words */

#define	RND_RND		0	/* real randomness like nuclear chips */
#define	RND_SRND	1	/* strong random source */
#define	RND_URND	2	/* less strong random source */
#define	RND_PRND	3	/* pseudo random source */
#define RND_ARND	4	/* aRC4 based random number generator */
#define RND_NODEV	5	/* First invalid minor device number */

#ifdef _KERNEL

extern void add_mouse_randomness __P((u_int32_t));
extern void add_net_randomness __P((int));
extern void add_blkdev_randomness __P((dev_t));
extern void add_tty_randomness __P((dev_t, int));

extern void get_random_bytes __P((void *, size_t));
extern unsigned long arc4random __P((void));

#endif /* _KERNEL */

#endif /* __RNDVAR_H__ */
