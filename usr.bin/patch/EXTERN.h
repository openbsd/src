/*	$Id: EXTERN.h,v 1.1.1.1 1995/10/18 08:45:54 deraadt Exp $ */

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
