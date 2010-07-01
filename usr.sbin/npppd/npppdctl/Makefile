#
# $Id: Makefile,v 1.2 2010/07/01 03:38:17 yasuoka Exp $
#
.include <bsd.own.mk>

PROG=		npppdctl
SRCS=		npppdctl.c debugutil.c
MAN=		npppdctl.8

CPPFLAGS+=	-I${.CURDIR}/../npppd -I${.CURDIR}/../common

.include <bsd.prog.mk>
.PATH:		${.CURDIR}/../common
