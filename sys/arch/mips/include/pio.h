/*	$OpenBSD: pio.h,v 1.2 1998/09/15 10:50:12 pefo Exp $	*/

/*
 * Copyright (c) 1995 Per Fogelstrom.  All rights reserved.
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
 *      This product includes software developed by Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _MACHINE_PIO_H_
#define _MACHINE_PIO_H_

/*
 * I/O macros.
 */

#define	outb(a,v)	(*(volatile unsigned char*)(a) = (v))
#define	out8(a,v)	(*(volatile unsigned char*)(a) = (v))
#define	outw(a,v)	(*(volatile unsigned short*)(a) = (v))
#define	out16(a,v)	outw(a,v)
#define	outl(a,v)	(*(volatile unsigned int*)(a) = (v))
#define	out32(a,v)	outl(a,v)
#define	inb(a)		(*(volatile unsigned char*)(a))
#define	in8(a)		(*(volatile unsigned char*)(a))
#define	inw(a)		(*(volatile unsigned short*)(a))
#define	in16(a)		inw(a)
#define	inl(a)		(*(volatile unsigned int*)(a))
#define	in32(a)		inl(a)

#define	out8rb(a,v)	(*(volatile unsigned char*)(a) = (v))
#define out16rb(a,v)	(__out16rb((volatile u_int16_t *)(a), v))
#define out32rb(a,v)	(__out32rb((volatile u_int32_t *)(a), v))
#define	in8rb(a)	(*(volatile unsigned char*)(a))
#define in16rb(a)	(__in16rb((volatile u_int16_t *)(a)))
#define in32rb(a)	(__in32rb((volatile u_int32_t *)(a)))

#define	_swap_(x) \
	(((x) >> 24) | ((x) << 24) | \
	(((x) >> 8) & 0xff00) | (((x) & 0xff00) << 8))

static __inline void __out32rb __P((volatile u_int32_t *, u_int32_t));
static __inline void __out16rb __P((volatile u_int16_t *, u_int16_t));
static __inline u_int32_t __in32rb __P((volatile u_int32_t *));
static __inline u_int16_t __in16rb __P((volatile u_int16_t *));

static __inline void
__out32rb(a,v)
	volatile u_int32_t *a;
	u_int32_t v;
{
	u_int32_t _v_ = v;

	_v_ = _swap_(_v_);
	out32(a, _v_);
}

static __inline void
__out16rb(a,v)
        volatile u_int16_t *a;
        u_int16_t v;
{
        u_int16_t _v_;

	_v_ = ((v >> 8) & 0xff) | (v << 8);
	out16(a, _v_);
}  

static __inline u_int32_t
__in32rb(a)
        volatile u_int32_t *a;
{
        u_int32_t _v_;

	_v_ = in32(a);
	_v_ = _swap_(_v_);
        return _v_;
}                      

static __inline u_int16_t
__in16rb(a)
        volatile u_int16_t *a;
{
        u_int16_t _v_;

	_v_ = in16(a);
	_v_ = ((_v_ >> 8) & 0xff) | (_v_ << 8);
        return _v_;
}

void insb __P((u_int8_t *, u_int8_t *,int));
void insw __P((u_int16_t *, u_int16_t *,int));
void insl __P((u_int32_t *, u_int32_t *,int));
void outsb __P((u_int8_t *, const u_int8_t *,int));
void outsw __P((u_int16_t *, const u_int16_t *,int));
void outsl __P((u_int32_t *, const u_int32_t *,int));

#endif /*_MACHINE_PIO_H_*/
