.PATH:		${.CURDIR}/..

PROG=	tcfsmng
BINOWN=	root

BINDIR=	/usr/bin
NOMAN=
#MAN=	tcfsmng.1

SRCS=	tcfsmng.c tcfsadduser.c tcfsrmuser.c tcfsaddgroup.c tcfsrmgroup.c

.include <bsd.prog.mk>

LDADD+=	-lutil -ldes
DPADD+= ${LIBUTIL} ${LIBDES}
