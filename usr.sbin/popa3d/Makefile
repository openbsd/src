# $OpenBSD: Makefile,v 1.2 2001/09/21 20:22:06 camield Exp $

PROG=	popa3d
MAN=	popa3d.8
SRCS=	auth_passwd.c database.c mailbox.c misc.c pop_auth.c pop_root.c \
	pop_trans.c protocol.c standalone.c startup.c virtual.c

CFLAGS+=-Wall -Werror -DHAVE_PROGNAME

.include <bsd.prog.mk>

.if (${TCP_WRAPPERS:L} == "yes")
CFLAGS+= -DLIBWRAP
LDADD+= -lwrap
DPADD+= ${LIBWRAP}
.endif
