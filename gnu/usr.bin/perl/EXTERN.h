/*    EXTERN.h
 *
 *    Copyright (c) 1991-1997, Larry Wall
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
#undef EXTCONST
#undef dEXTCONST
#if defined(VMS) && !defined(__GNUC__)
#  define EXT globalref
#  define dEXT globaldef {"$GLOBAL_RW_VARS"} noshare
#  define EXTCONST globalref
#  define dEXTCONST globaldef {"$GLOBAL_RO_VARS"} readonly
#else
#  if (defined(_MSC_VER) && defined(_WIN32)) || (defined(__BORLANDC__) && defined(__WIN32__))
#    ifdef PERLDLL
#      define EXT extern __declspec(dllexport)
#      define dEXT 
#      define EXTCONST extern __declspec(dllexport) const
#      define dEXTCONST const
#    else
#      define EXT extern __declspec(dllimport)
#      define dEXT 
#      define EXTCONST extern __declspec(dllimport) const
#      define dEXTCONST const
#    endif
#  else
#    define EXT extern
#    define dEXT
#    define EXTCONST extern const
#    define dEXTCONST const
#  endif
#endif

#undef INIT
#define INIT(x)

#undef DOINIT
