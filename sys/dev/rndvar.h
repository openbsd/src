/*	$OpenBSD: rndvar.h,v 1.38 2016/05/23 15:48:59 deraadt Exp $	*/

/*
 * Copyright (c) 1996,2000 Michael Shalayeff.
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

#define	RND_SRC_TRUE	0
#define	RND_SRC_TIMER	1
#define	RND_SRC_MOUSE	2
#define	RND_SRC_TTY	3
#define	RND_SRC_DISK	4
#define	RND_SRC_NET	5
#define	RND_SRC_AUDIO	6
#define	RND_SRC_VIDEO	7
#define	RND_SRC_NUM	8

#ifdef _KERNEL
#define	add_true_randomness(d)	enqueue_randomness(RND_SRC_TRUE,  (int)(d))
#define	add_timer_randomness(d)	enqueue_randomness(RND_SRC_TIMER, (int)(d))
#define	add_mouse_randomness(d)	enqueue_randomness(RND_SRC_MOUSE, (int)(d))
#define	add_tty_randomness(d)	enqueue_randomness(RND_SRC_TTY,   (int)(d))
#define	add_disk_randomness(d)	enqueue_randomness(RND_SRC_DISK,  (int)(d))
#define	add_net_randomness(d)	enqueue_randomness(RND_SRC_NET,   (int)(d))
#define	add_audio_randomness(d)	enqueue_randomness(RND_SRC_AUDIO, (int)(d))
#define	add_video_randomness(d)	enqueue_randomness(RND_SRC_VIDEO, (int)(d))

void random_start(void);

void enqueue_randomness(unsigned int, unsigned int);
void suspend_randomness(void);
void resume_randomness(char *, size_t);

#endif /* _KERNEL */

#endif /* __RNDVAR_H__ */
