REGRESS_TARGETS=	send

PROG=		edit
SRCS=		edit.c
CFLAGS+=	-Wall
LDADD+=		-lutil
CLEANFILES+=	edit.c

edit.c: ${.CURDIR}/../../bin/ksh/edit/edit.c
	@cp ${.CURDIR}/../../bin/ksh/edit/edit.c .

send: edit
	sh ${.CURDIR}/send.sh

.include <bsd.regress.mk>
