/* $OpenBSD: INTERN.h,v 1.3 2003/07/21 14:00:41 deraadt Exp $ */

#ifdef EXT
#undef EXT
#endif
#define EXT

#ifdef INIT
#undef INIT
#endif
#define INIT(x) = x

#define DOINIT
