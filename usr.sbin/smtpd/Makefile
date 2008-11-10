#	$OpenBSD: Makefile,v 1.2 2008/11/10 17:24:24 deraadt Exp $

PROG=		smtpd
SRCS=		parse.y atomic.c log.c config.c buffer.c imsg.c		\
		smtpd.c lka.c mfa.c queue.c mta.c mda.c control.c	\
		smtp.c	smtp_session.c store.c debug.c			\
		ssl.c ssl_privsep.c dns.c aliases.c forward.c		\
		map.c
MAN=		smtpd.8 smtpd.conf.5

LDADD=		-levent -lutil -lssl -lcrypto
DPADD=		${LIBEVENT} ${LIBSSL} ${LIBCRYPTO}
CFLAGS=		-g3 -ggdb -I${.CURDIR}
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
#CFLAGS+=	-Werror # during development phase (breaks some archs)
YFLAGS=

.include <bsd.prog.mk>
