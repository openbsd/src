/*	$Id: INTERN.h,v 1.1.1.1 1995/10/18 08:45:55 deraadt Exp $ */

#ifdef EXT
#undef EXT
#endif
#define EXT

#ifdef INIT
#undef INIT
#endif
#define INIT(x) = x

#define DOINIT
