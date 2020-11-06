#	$OpenBSD: Makefile.inc,v 1.6 2020/11/06 15:26:38 bluhm Exp $

TOPDIR=		${.CURDIR}/..

.ifdef RADIX
SRCS+=		radix.c
.else
SRCS+=		art.c
CFLAGS+=	-DART
.endif

CPPFLAGS+=	-I${TOPDIR} -Wall \
		-D'satosin(sa)=((struct sockaddr_in *)(sa))' \
		-D'satosin6(sa)=((struct sockaddr_in6 *)(sa))'

.PATH:		${TOPDIR} ${TOPDIR}/../../../../sys/net
