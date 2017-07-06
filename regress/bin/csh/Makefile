REGRESS_TARGETS=	filec

PROG=		edit
SRCS=		edit.c
CFLAGS+=	-Wall
LDADD+=		-lutil
CLEANFILES+=	edit.c

edit.c: ${.CURDIR}/../ksh/edit/edit.c
	@cp ${.CURDIR}/../ksh/edit/edit.c .

filec: edit
	sh ${.CURDIR}/filec.sh

.include <bsd.regress.mk>
