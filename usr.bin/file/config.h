/*
 * Hand-made config.h file for OpenBSD, so we don't have to run
 * the dratted configure script every time we build this puppy,
 * but can still carefully import stuff from Christos' version.
 *
 * This file is in the public domain. Original Author Ian F. Darwin.
 * $OpenBSD: config.h,v 1.1 2003/03/03 23:06:46 ian Exp $
 */

/* header file issues. */
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_LOCALE_H 1
#define HAVE_SYS_STAT_H 1
#define	HAVE_INTTYPES_H 1
/* #define	HAVE_LIBZ 1  DO NOT ENABLE YET -- ian */

/* Compiler issues */
#define HAVE_LONG_LONG 1
#define SIZEOF_UINT8_T 1
#define SIZEOF_UINT16_T 2
#define SIZEOF_UINT32_T 4
#define SIZEOF_UINT64_T 8

/* Library issues */
#define HAVE_GETOPT_LONG 1	/* in-tree as of 3.2 */
#define HAVE_MKSTEMP 1

/* ELF support */
#define BUILTIN_ELF 1
#define ELF_CORE 1
