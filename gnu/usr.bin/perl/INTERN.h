/*    INTERN.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * EXT  designates a global var which is defined in perl.h
 * dEXT designates a global var which is defined in another
 *      file, so we can't count on finding it in perl.h
 *      (this practice should be avoided).
 */
#undef EXT
#undef dEXT
#if defined(VMS) && !defined(__GNUC__)
#  define EXT globaldef {"$GLOBAL_RW_VARS"} noshare
#  define dEXT globaldef {"$GLOBAL_RW_VARS"} noshare
#else
#  define EXT
#  define dEXT
#endif

#undef INIT
#define INIT(x) = x

#define DOINIT
