/*	$OpenBSD: EXTERN.h,v 1.4 2003/07/21 14:32:21 deraadt Exp $	*/

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
