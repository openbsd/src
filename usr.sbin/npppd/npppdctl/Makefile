#
# $Id: Makefile,v 1.1 2010/01/13 07:49:44 yasuoka Exp $
#
NOMAN=		#defined

.include <bsd.own.mk>

PROG=		npppdctl
SRCS=		npppdctl.c debugutil.c

CPPFLAGS+=	-I${.CURDIR}/../npppd -I${.CURDIR}/../common

.include <bsd.prog.mk>
.PATH:		${.CURDIR}/../common
