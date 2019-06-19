/* Public domain. */

#ifndef _LINUX_RANDOM_H
#define _LINUX_RANDOM_H

#include <sys/types.h>
#include <sys/systm.h>

#define get_random_u32()	arc4random()
#define get_random_int()	arc4random()

static inline uint64_t
get_random_u64(void)
{
	uint64_t r;
	arc4random_buf(&r, sizeof(r));
	return r;
}

static inline unsigned long
get_random_long(void)
{
#ifdef __LP64__
	return get_random_u64();
#else
	return get_random_u32();
#endif
}

#endif
