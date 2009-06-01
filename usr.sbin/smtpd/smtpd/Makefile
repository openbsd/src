#	$OpenBSD: Makefile,v 1.8 2009/06/01 22:51:47 gilles Exp $

PROG=		smtpd
SRCS=		parse.y log.c config.c buffer.c imsg.c			\
		smtpd.c lka.c mfa.c queue.c mta.c mda.c control.c	\
		smtp.c	smtp_session.c store.c				\
		ssl.c ssl_privsep.c dns.c aliases.c forward.c		\
		map.c runner.c util.c queue_shared.c ruleset.c
MAN=		smtpd.8 smtpd.conf.5
BINDIR=		/usr/sbin

LDADD+=		-levent -lutil -lkeynote -lssl -lcrypto -lm
DPADD+=		${LIBEVENT} ${LIBUTIL} ${LIBKEYNOTE} ${LIBSSL} ${LIBCRYPTO} ${LIBM}
CFLAGS=		-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
#CFLAGS+=	-Werror # during development phase (breaks some archs)
YFLAGS=

.PATH: ${.CURDIR}/..

.include <bsd.prog.mk>
