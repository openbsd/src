/*	$OpenBSD: assert.h,v 1.4 1999/02/09 06:36:25 smurph Exp $ */
#define assert(x) \
({\
	if (!(x)) {\
		printf("assertion failure \"%s\" line %d file %s\n", \
		#x, __LINE__, __FILE__); \
		panic("assertion"); \
	} \
})
