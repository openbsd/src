/*	$OpenBSD: INTERN.h,v 1.2 1996/06/10 11:21:24 niklas Exp $ */

#ifdef EXT
#undef EXT
#endif
#define EXT

#ifdef INIT
#undef INIT
#endif
#define INIT(x) = x

#define DOINIT
