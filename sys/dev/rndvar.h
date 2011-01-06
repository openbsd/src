/*	$OpenBSD: rndvar.h,v 1.26 2011/01/06 15:41:51 deraadt Exp $	*/

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

#define POOLWORDS 2048	/* Power of 2 - note that this is 32-bit words */

#define	RND_SRC_TRUE	0
#define	RND_SRC_TIMER	1
#define	RND_SRC_MOUSE	2
#define	RND_SRC_TTY	3
#define	RND_SRC_DISK	4
#define	RND_SRC_NET	5
#define	RND_SRC_AUDIO	6
#define	RND_SRC_VIDEO	7
#define	RND_SRC_NUM	8

struct rndstats {
	quad_t rnd_total;	/* total bits of entropy generated */
	quad_t rnd_used;	/* strong data bits read so far */
	quad_t rnd_reads;	/* strong read calls -- unused */
	quad_t arc4_reads;	/* aRC4 data bytes read so far */
	quad_t arc4_nstirs;	/* arc4 pool stirs */
	quad_t arc4_stirs;	/* arc4 pool stirs (bits used) */

	quad_t rnd_pad[5];

	quad_t rnd_waits;	/* sleeps for data -- unused */
	quad_t rnd_enqs;	/* enqueue calls */
	quad_t rnd_deqs;	/* dequeue calls */
	quad_t rnd_drops;	/* queue-full drops */
	quad_t rnd_drople;	/* queue low watermark low entropy drops */

	quad_t rnd_ed[32];	/* entropy feed distribution */
	quad_t rnd_sc[RND_SRC_NUM]; /* add* calls */
	quad_t rnd_sb[RND_SRC_NUM]; /* add* bits */
};

#ifdef _KERNEL
extern struct rndstats rndstats;

#define	add_true_randomness(d)	enqueue_randomness(RND_SRC_TRUE,  (int)(d))
#define	add_timer_randomness(d)	enqueue_randomness(RND_SRC_TIMER, (int)(d))
#define	add_mouse_randomness(d)	enqueue_randomness(RND_SRC_MOUSE, (int)(d))
#define	add_tty_randomness(d)	enqueue_randomness(RND_SRC_TTY,   (int)(d))
#define	add_disk_randomness(d)	enqueue_randomness(RND_SRC_DISK,  (int)(d))
#define	add_net_randomness(d)	enqueue_randomness(RND_SRC_NET,   (int)(d))
#define	add_audio_randomness(d)	enqueue_randomness(RND_SRC_AUDIO, (int)(d))
#define	add_video_randomness(d)	enqueue_randomness(RND_SRC_VIDEO, (int)(d))

void enqueue_randomness(int, int);
void arc4random_buf(void *, size_t);
u_int32_t arc4random(void);
u_int32_t arc4random_uniform(u_int32_t);

#endif /* _KERNEL */

#endif /* __RNDVAR_H__ */
