/*    EXTERN.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#undef EXT
#define EXT extern

#ifdef __cplusplus
#  define EXTERN_C extern "C"
#else
#  define EXTERN_C extern
#endif

#undef INIT
#define INIT(x)

#undef DOINIT
