#	$OpenBSD: Makefile.inc,v 1.3 2017/07/27 13:34:30 mpi Exp $

TOPDIR=		${.CURDIR}/..

.ifdef RADIX
SRCS+=		radix.c
.else
SRCS+=		art.c
CFLAGS+=	-DART
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
