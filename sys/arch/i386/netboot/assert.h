/*	$NetBSD: assert.h,v 1.3 1994/10/27 04:21:07 cgd Exp $	*/

/*
 * source in this file came from
 * the Mach ethernet boot written by Leendert van Doorn.
 *
 * assert.h
 */

#ifndef	NDEBUG
#define	assert(exp) \
	if (!(exp)) { \
	    printf("Assertion \"%s\" failed: file %s, line %d\n", \
		#exp, __FILE__, __LINE__); \
	    exit(1); \
	}
#else
#define	assert(exp)	/* */
#endif /* _ASSERT_H */
