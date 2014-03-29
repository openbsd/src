/*	$OpenBSD: atomic.h,v 1.8 2014/03/29 18:09:30 guenther Exp $	*/

/* Public Domain */

#ifndef _MIPS64_ATOMIC_H_
#define _MIPS64_ATOMIC_H_

#if defined(_KERNEL)

/* wait until the bits to set are clear, and set them */
static __inline void
atomic_wait_and_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp0, tmp1;

	__asm__ volatile (
	"1:	ll	%0,	0(%2)\n"
	"	and	%1,	%0,	%3\n"
	"	bnez	%1,	1b\n"
	"	or	%0,	%3,	%0\n"
	"	sc	%0,	0(%2)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp0), "=&r"(tmp1) :
		"r"(uip), "r"(v) : "memory");
}

static __inline void
atomic_setbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm__ volatile (
	"1:	ll	%0,	0(%1)\n"
	"	or	%0,	%2,	%0\n"
	"	sc	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(v) : "memory");
}

static __inline void
atomic_clearbits_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm__ volatile (
	"1:	ll	%0,	0(%1)\n"
	"	and	%0,	%2,	%0\n"
	"	sc	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(~v) : "memory");
}

static __inline void
atomic_add_int(volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm__ volatile (
	"1:	ll	%0,	0(%1)\n"
	"	addu	%0,	%2,	%0\n"
	"	sc	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(v) : "memory");
}
static __inline void
atomic_add_uint64(volatile uint64_t *uip, uint64_t v)
{
	uint64_t tmp;

	__asm__ volatile (
	"1:	lld	%0,	0(%1)\n"
	"	daddu	%0,	%2,	%0\n"
	"	scd	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(v) : "memory");
}
#endif /* defined(_KERNEL) */
#endif /* _MIPS64_ATOMIC_H_ */
