/*	$OpenBSD: opti.c,v 1.1 1996/04/24 16:51:14 mickey Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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

/*
 * Code to setup 82C929 chipset
 */

#define	OPTI_DEBUG		9

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/pio.h>

#include <i386/isa/icu.h>
#include <dev/isa/isavar.h>

#include <dev/isa/opti.h>

#ifdef OPTI_DEBUG
   int opti_debuglevel = OPTI_DEBUG;
#  define XDEBUG(level, data)	((opti_debuglevel >= level)? printf data:0)
#else
#  define XDEBUG(level, data)	/* ((opti_debuglevel >= level)? printf data:0) */
#endif

#define	OPTI_cd_valid_ift(i)	((i)==OPTI_SONY||(i)==OPTI_PANASONIC||\
					(i)==OPTI_MITSUMI||(i)==OPTI_IDE)
static __inline int
OPTI_cd_addr(a)
	int	a;
{
	switch(a) {
		case 0x320:	return 0xc0;
		case 0x330:	return 0x40;
		case 0x340:	return 0x00;
		case 0x360:	return 0x80;
		default:	return -1;
	}
}
static __inline int
OPTI_cd_irq(i)
	int	i;
{
	switch(i) {
		case 5:	return 0x04;
		case 7:	return 0x08;
		case 3:	return 0x0c;
		case 9:	return 0x10;
		case 10:return 0x14;
		case 11:return 0x18;
		case -1:return 0x00;
		default:return -1;
	}
}
static __inline int
OPTI_cd_drq(d)
	int	d;
{
	switch(d) {
		case 3:
		case 5:	return 0;
		case 6:	return 1;
		case 7:	return 2;
		default:return 3;
	}
}

#define	OPTI_snd_valid_ift(i)	((i)==OPTI_WSS||(i)==OPTI_SB)

static __inline int
OPTI_snd_addr(a)
	int	a;
{
	switch(a) {
		case 0x220:	return 0x0;
		case 0x240:	return 0x3;
		case 0x530:	return 0x8;
		case 0xE80:	return 0x9;
		case 0xF40:	return 0xa;
		case 0x604:	return 0xb;
		default:	return -1;
	}
}
static __inline int
OPTI_snd_irq(i)
	int	i;
{
	switch(i) {
		case 5:	return 0x04;
		case 7:	return 0x08;
		case 3:	return 0x0c;
		case 9:	return 0x10;
		case 10:return 0x14;
		case 11:return 0x18;
		case -1:return 0x00;
		default:return -1;
	}
}
static __inline int
OPTI_snd_drq(d)
	int	d;
{
	switch(d) {
		case 3:
		case 5:	return 0;
		case 6:	return 1;
		case 7:	return 2;
		default:return 3;
	}
}

static int
opti_present( void )
{
	register u_char	a, b;
	int s = splhigh();

	outb( OPTI_CTRL, 0xe3 );
	a = inb( OPTI_CTRL );

	outb( OPTI_CTRL, 0xe3 );
	outb( OPTI_CTRL, 0x00 );

	outb( OPTI_CTRL, 0xe3 );
	b = inb( OPTI_CTRL );

	outb( OPTI_CTRL, 0xe3 );
	outb( OPTI_CTRL,  a );

	splx(s);

	return b == 2;
}

int
opti_cd_setup( ift, addr, irq, drq )
	int	ift, addr, irq, drq;
{
	int	ret = 0;

	XDEBUG( 2, ("opti: do CD setup t=%u, a=0x%x, i=%d, d=%d\n",
		    ift, addr, irq, drq));

	if( !opti_present() )
		XDEBUG( 2, ("opti: not present.\n"));
	else if( !OPTI_cd_valid_ift(ift) )
		XDEBUG( 2, ("opti: invalid CD-ROM interface type.\n"));
	else if( OPTI_cd_addr(addr) == -1)
		XDEBUG( 2, ("opti: illegal CD-ROM interface address.\n"));
	else if( OPTI_cd_irq(irq) == -1)
		XDEBUG( 2, ("opti: wrong CD-ROM irq number.\n"));
	else if( OPTI_cd_drq(drq) == -1)
		XDEBUG( 2, ("opti: bad CD_ROM drq number.\n"));
	else
	{
			/* so the setup */
		int s = splhigh();
		register u_char	a, b;

			/* set interface type */
		outb( OPTI_CTRL, 0xe3 );
		a = inb( OPTI_IFTP );

		outb( OPTI_CTRL, 0xe3 );
		b = (inb( OPTI_DATA ) & 0x20) | 3 ;

		outb( OPTI_CTRL, 0xe3 );
		outb( OPTI_DATA,  b );

		outb( OPTI_CTRL, 0xe3 );
		outb( OPTI_IFTP, (a & 0xf1) | 2*ift );

		outb( OPTI_CTRL, 0xe3 );
		outb( OPTI_ENBL, 0x80 );

			/* we don't need any additional setup for IDE CD-ROM */
		if( ift != OPTI_IDE )
		{
				/* set address */
			outb( OPTI_CTRL, 0xe3 );
			a = inb( OPTI_DATA );

			outb( OPTI_CTRL, 0xe3 );
			outb( OPTI_DATA, (a & 0x3f) |
			     (0x40 * OPTI_cd_addr(addr)) );

				/* set irq */
			if( irq != IRQUNK )
			{
				outb( OPTI_CTRL, 0xe3 );
				a = inb( OPTI_DATA );

				outb( OPTI_CTRL, 0xe3 );
				outb( OPTI_DATA, (inb( OPTI_DATA ) & 0xe3) |
				     OPTI_cd_irq(irq) );
			}

				/* set drq */
			if( drq != DRQUNK )
			{
				outb( OPTI_CTRL, 0xe3 );
				a = inb( OPTI_DATA );

				outb( OPTI_CTRL, 0xe3 );
				outb( OPTI_DATA, (inb( OPTI_DATA ) & 0xfc) |
				     OPTI_cd_drq(drq) );
			}
		}
		splx(s);
		DELAY(1000);
		ret = 1;
	}

	return ret;
}

int
opti_snd_setup( ift, addr, irq, drq )
	int	ift, addr, irq, drq;
{
	XDEBUG( 2, ("opti: do SND setup t=%u,a=%x,i=%d,d=%d\n",
		    ift, addr, irq, drq));

	if( !opti_present() )
		XDEBUG( 2, ("opti: not present.\n"));
	else if( !OPTI_snd_valid_ift(ift) )
		XDEBUG( 2, ("opti: invalid SND interface type.\n"));
	else if( OPTI_snd_addr(addr) == -1)
		XDEBUG( 2, ("opti: illegal SND interface address.\n"));
	else if( OPTI_snd_irq(irq) == -1)
		XDEBUG( 2, ("opti: wrong SND irq number.\n"));
	else if( OPTI_snd_drq(drq) == -1)
		XDEBUG( 2, ("opti: bad SND drq number.\n"));
	else
	{
			/* so the setup */
		int s = splhigh();
		register u_char	a, b;

		if (ift == OPTI_WSS) {
			outb( OPTI_CTRL, 0xe3 );
			a = inb( OPTI_CTRL );

			outb( OPTI_CTRL, 0xe3 );
			outb( OPTI_ENBL, 0x1a );

			outb( OPTI_CTRL, 0xe3 );
			outb( OPTI_IFTP, OPTI_snd_addr(addr)*16 + 1 );

			outb( OPTI_CTRL, 0xe3 );
			outb( OPTI_ENBL, 0x1a );

			outb( OPTI_CTRL, 0xe3 );
			outb( OPTI_CTRL, a );
		}

		splx(s);
		DELAY(1000);
		return 1;
	}

	return 0;
}
