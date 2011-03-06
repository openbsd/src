/* $FreeBSD: src/gnu/usr.bin/cc/cc_tools/freebsd-native.h,v 1.28.8.1 2009/04/15 03:14:26 kensmith Exp $ */
/* $OpenBSD: openbsd-native.h,v 1.3 2011/03/06 20:18:22 guenther Exp $ */

/* OPENBSD_NATIVE is defined when gcc is integrated into the OpenBSD
   source tree so it can be configured appropriately without using
   the GNU configure/build mechanism. */

#define OPENBSD_NATIVE 1

#undef SYSTEM_INCLUDE_DIR		/* We don't need one for now. */
#undef TOOL_INCLUDE_DIR			/* We don't need one for now. */
#undef LOCAL_INCLUDE_DIR		/* We don't wish to support one. */

/* Look for the include files in the system-defined places.  */
#define GPLUSPLUS_INCLUDE_DIR		PREFIX"/include/g++"
#define	GPLUSPLUS_BACKWARD_INCLUDE_DIR	PREFIX"/include/g++/backward"
#define GCC_INCLUDE_DIR			PREFIX"/include"
#ifdef CROSS_COMPILE
#define CROSS_INCLUDE_DIR		PREFIX"/include"
#else
#define STANDARD_INCLUDE_DIR		PREFIX"/include"
#endif

/* Under OpenBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.

   ``cc --print-search-dirs'' gives:
   install: STANDARD_EXEC_PREFIX/
   programs: STANDARD_EXEC_PREFIX:MD_EXEC_PREFIX
   libraries: STANDARD_STARTFILE_PREFIX
*/
#undef	STANDARD_BINDIR_PREFIX		/* We don't need one for now. */
#define	STANDARD_EXEC_PREFIX		PREFIX"/lib/gcc-lib/"
#define	STANDARD_LIBEXEC_PREFIX		PREFIX"/lib/gcc-lib/"
#define TOOLDIR_BASE_PREFIX		PREFIX"/"
#undef	MD_EXEC_PREFIX			/* We don't want one. */
#define	FBSD_DATA_PREFIX		PREFIX"/libdata/gcc/"

/* Under OpenBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#undef  MD_STARTFILE_PREFIX		/* We don't need one for now. */
#define STANDARD_STARTFILE_PREFIX	PREFIX"/lib/"
#define STARTFILE_PREFIX_SPEC		PREFIX"/lib/"
#ifdef CROSS_COMPILE
#define STANDARD_BINDIR_PREFIX		PREFIX"/"DEFAULT_TARGET_MACHINE "/bin/"
#else
#define STANDARD_BINDIR_PREFIX		PREFIX"/bin/"
#endif

/* OpenBSD is 4.4BSD derived */
#define bsd4_4
