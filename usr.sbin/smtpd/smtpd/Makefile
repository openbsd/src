#	$OpenBSD: Makefile,v 1.40 2012/01/29 11:37:32 eric Exp $

PROG=		smtpd
SRCS=		aliases.c auth.c auth_bsd.c auth_pwd.c bounce.c		\
		iobuf.c ioev.c						\
		delivery.c delivery_filename.c delivery_maildir.c	\
		delivery_mbox.c delivery_mda.c				\
		config.c control.c dns.c expand.c envelope.c forward.c	\
		lka.c lka_session.c log.c map.c map_db.c map_stdio.c	\
		mda.c mfa.c mfa_session.c mta.c parse.y 		\
		queue.c ruleset.c runner.c smtp.c 			\
		smtp_session.c smtpd.c ssl.c ssl_privsep.c util.c asr.c \
		print.c pack.c dname.c res_random.c sockaddr.c 		\
		queue_backend.c queue_fsqueue.c				\
		user.c user_pwd.c stats.c scheduler.c scheduler_ramqueue.c

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
CFLAGS+=	-DIO_SSL
YFLAGS=

.PATH: ${.CURDIR}/..

.include <bsd.prog.mk>
