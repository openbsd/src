#	$OpenBSD: Makefile.inc,v 1.4 2019/03/31 14:03:40 mpi Exp $

TOPDIR=		${.CURDIR}/..

.ifdef RADIX
SRCS+=		radix.c
.else
SRCS+=		art.c
CFLAGS+=	-DART
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall -Wno-macro-redefined

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
