/*	$OpenBSD: rndioctl.h,v 1.2 1996/08/11 07:31:32 dm Exp $	*/

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


#ifndef __RNDIOCTL_H__
#define __RNDIOCTL_H__

struct rnd_pool_info {
	size_t	entropy_count;
	size_t	buf_size;
	u_int32_t *buf;
};

/* ioctl()'s for the random number generator */

#define RNDGETENTCNT	_IOR('R', 0, sizeof(u_int))
#define RNDADDTOENTCNT	_IOW('R', 1, sizeof(u_int))
#define RNDGETPOOL	_IOWR('R', 2, sizeof(struct rnd_pool_info))
#define RNDADDENTROPY	_IOW('R', 3, sizeof(u_int))
#define RNDZAPENTCNT	_IO( 'R', 4)
#define RNDSTIRARC4	_IO( 'R', 5)


#endif /* __RNDIOCTL_H__ */
