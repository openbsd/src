# $OpenBSD: Makefile,v 1.3 2017/07/22 13:50:54 anton Exp $

REGRESS_TARGETS=	send

PROG=		edit
CFLAGS+=	-Wall
LDADD+=		-lutil
CLEANFILES+=	edit.c

edit.c: ${.CURDIR}/../../bin/ksh/edit/edit.c
	@cp ${.CURDIR}/../../bin/ksh/edit/edit.c .

send: edit
	sh ${.CURDIR}/send.sh

.include <bsd.regress.mk>
