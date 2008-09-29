/*    INTERN.h
 *
 *    Copyright (C) 1993, 1994, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#undef EXT
#define EXT

#ifdef __cplusplus
#  define EXTERN_C extern "C"
#else
#  ifndef EXTERN_C
#    define EXTERN_C
#  endif
#endif

#undef INIT
#define INIT(x) = x

#define DOINIT
