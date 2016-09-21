#	$OpenBSD: Makefile.inc,v 1.2 2016/09/21 07:13:02 mpi Exp $

TOPDIR=		${.CURDIR}/..

.ifdef RADIX
SRCS+=		radix.c
.else
SRCS+=		art.c
CFLAGS+=	-DART
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall -DSMALL_KERNEL

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
