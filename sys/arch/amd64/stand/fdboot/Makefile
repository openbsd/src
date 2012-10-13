#	$OpenBSD: Makefile,v 1.2 2012/10/13 14:10:35 jsing Exp $

MAN=
MLINKS=

.if ${MACHINE} == "amd64"
PROG=	fdboot
COPTS=	-DFDBOOT
.endif

.PATH:	${.CURDIR}/../boot

.include "${.CURDIR}/../boot/Makefile"
