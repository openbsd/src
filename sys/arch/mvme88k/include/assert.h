/*	$OpenBSD: assert.h,v 1.6 2001/03/09 05:44:40 smurph Exp $ */
#ifndef __MACHINE_ASSERT_H__
#define __MACHINE_ASSERT_H__
#ifndef assert
#define assert(x) \
({\
	if (!(x)) {\
		printf("assertion failure \"%s\" line %d file %s\n", \
		#x, __LINE__, __FILE__); \
		panic("assertion"); \
	} \
})
#endif /* assert */
#endif __MACHINE_ASSERT_H__

