/*	$OpenBSD: pio.h,v 1.4 2001/01/15 19:50:38 deraadt Exp $	*/
/*	$NetBSD: pio.h,v 1.3 1994/10/26 08:46:38 cgd Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

#ifndef _MAC68K_PIO_H_
#define _MAC68K_PIO_H_

#define inl(y) \
({ unsigned long _tmp__; \
	__asm__ __volatile__("inl %1, %0" : "=a" (_tmp__) : "d" ((unsigned short)(y))); \
	_tmp__; })

#define inw(y) \
({ unsigned short _tmp__; \
	__asm__ __volatile__(".byte 0x66; inl %1, %0" : "=a" (_tmp__) : "d" ((unsigned short)(y))); \
	_tmp__; })

#define inb(y) \
({ unsigned char _tmp__; \
	__asm__ __volatile__("inb %1, %0" : "=a" (_tmp__) : "d" ((unsigned short)(y))); \
	_tmp__; })


#define outl(x, y) \
{ __asm__ __volatile__("outl %0, %1" : : "a" (y) , "d" ((unsigned short)(x))); }


#define outw(x, y) \
{__asm__ __volatile__(".byte 0x66; outl %0, %1" : : "a" ((unsigned short)(y)) , "d" ((unsigned short)(x))); }


#define outb(x, y) \
{ __asm__ __volatile__("outb %0, %1" : : "a" ((unsigned char)(y)) , "d" ((unsigned short)(x))); }

#endif /* _MAC68K_PIO_H_ */
