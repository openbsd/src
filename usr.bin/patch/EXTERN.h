/*	$OpenBSD: EXTERN.h,v 1.2 1996/06/10 11:21:23 niklas Exp $ */

#ifdef EXT
#undef EXT
#endif
#define EXT extern

#ifdef INIT
#undef INIT
#endif
#define INIT(x)

#ifdef DOINIT
#undef DOINIT
#endif
