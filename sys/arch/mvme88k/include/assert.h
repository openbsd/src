/*	$OpenBSD: assert.h,v 1.5 2001/01/14 20:25:23 smurph Exp $ */
#ifndef __MACHINE_ASSERT_H__
#define __MACHINE_ASSERT_H__
#define assert(x) \
({\
	if (!(x)) {\
		printf("assertion failure \"%s\" line %d file %s\n", \
		#x, __LINE__, __FILE__); \
		panic("assertion"); \
	} \
})
#endif __MACHINE_ASSERT_H__
