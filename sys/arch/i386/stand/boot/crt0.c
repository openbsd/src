/*	$OpenBSD: crt0.c,v 1.3 1998/05/25 19:20:49 mickey Exp $	*/

/*
 * Copyright (c) 1997-1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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


#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <termios.h>
#include "libsa.h"
#include <lib/libsa/unixdev.h>

void start __P((void)) asm("start");
void _rtt __P((void));
extern int  boot __P((dev_t));
static void domap __P((void));
static void seterm __P((void));

void
start()
{
	domap();
	seterm();
	uexit(boot(0));
}

void
_rtt()
{
	uexit(1);
}

#define ummap(a,l,p,f,fd,o) (caddr_t)syscall((quad_t)SYS_mmap,a,l,p,f,fd,0,o)

static void
domap()
{
	extern char end[];
	register caddr_t p = (caddr_t)(((u_long)end + PGOFSET) & ~PGOFSET);

	/* map heap */
	if ( (p = ummap(p, 32*NBPG, PROT_READ|PROT_WRITE,
		   MAP_FIXED|MAP_ANON, -1, 0)) == (caddr_t)-1) {
		printf("mmap failed: %d\n", errno);
		uexit(1);
	}
#ifdef DEBUG
	else
		printf("mmap==%p\n", p);
#endif

	/* map kernel */
	if ( (p = ummap(0x100000, 0xf00000, PROT_READ|PROT_WRITE,
		   MAP_FIXED|MAP_ANON, -1, 0)) == (caddr_t)-1) {
		printf("mmap failed: %d\n", errno);
		uexit(1);
	}
#ifdef DEBUG
	else
		printf("mmap==%p\n", p);
#endif

}

void
seterm()
{
	struct termios tc;

	if (uioctl(0, TIOCGETA, (char *)&tc) < 0) {
		printf("cannot get tty\n");
		uexit(1);
	}

        tc.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
        tc.c_oflag &= ~OPOST;
        tc.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
        tc.c_cflag &= ~(CSIZE|PARENB);
        tc.c_cflag |= CS8;

	if (uioctl(0, TIOCSETA, (char *)&tc) < 0) {
		printf("cannot set tty\n");
		uexit(1);
	}
}
