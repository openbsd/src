#	$OpenBSD: Makefile,v 1.27 2011/05/17 18:54:32 gilles Exp $

PROG=		smtpd
SRCS=		aliases.c auth_backend.c bounce.c client.c		\
		config.c control.c dns.c expand.c forward.c		\
		lka.c lka_session.c log.c map.c map_backend.c		\
		map_parser.c mda.c mfa.c mta.c parse.y queue.c		\
		queue_shared.c ruleset.c runner.c smtp.c smtp_session.c	\
		smtpd.c ssl.c ssl_privsep.c util.c asr.c print.c pack.c	\
		dname.c	res_random.c sockaddr.c ramqueue.c		\
		queue_backend.c	queue_fsqueue.c user_backend.c

MAN=		smtpd.8 smtpd.conf.5
BINDIR=		/usr/sbin

LDADD+=		-levent -lutil -lssl -lcrypto -lm
DPADD+=		${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBM}
CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
#CFLAGS+=	-Werror # during development phase (breaks some archs)
YFLAGS=

.PATH: ${.CURDIR}/..

.include <bsd.prog.mk>
