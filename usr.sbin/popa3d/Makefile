# $OpenBSD: Makefile,v 1.3 2002/09/06 19:17:24 deraadt Exp $

PROG=	popa3d
MAN=	popa3d.8
SRCS=	auth_passwd.c database.c mailbox.c misc.c pop_auth.c pop_root.c \
	pop_trans.c protocol.c standalone.c startup.c

CFLAGS+=-Wall -Werror -DHAVE_PROGNAME

.include <bsd.prog.mk>

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+= -lwrap
DPADD+= ${LIBWRAP}
.endif
