# $OpenBSD: Makefile,v 1.4 2017/11/16 19:05:44 anton Exp $

REGRESS_TARGETS=	filec

CSH=	/bin/csh

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil
CLEANFILES+=	edit.c

edit.c: ${.CURDIR}/../ksh/edit/edit.c
	@cp ${.CURDIR}/../ksh/edit/edit.c .

filec: edit
	sh ${.CURDIR}/filec.sh ${CSH}

.include <bsd.regress.mk>
