/* $OpenBSD: kbind.h,v 1.1 2016/03/20 02:32:39 guenther Exp $ */

/* kbind disabled in the kernel for hppa64 until we do dynamic linking */
#define	MD_DISABLE_KBIND	do { } while (0)
