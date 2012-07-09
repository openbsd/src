#	$OpenBSD: Makefile,v 1.44 2012/07/09 12:16:24 eric Exp $

.PATH:		 ${.CURDIR}/.. ${.CURDIR}/../../../lib/libc/asr

PROG=		smtpd
SRCS=		aliases.c auth.c auth_bsd.c auth_pwd.c bounce.c		\
		iobuf.c ioev.c						\
		delivery.c delivery_filename.c delivery_maildir.c	\
		delivery_mbox.c delivery_mda.c				\
		config.c control.c dns.c expand.c envelope.c forward.c	\
		lka.c lka_session.c log.c map.c map_db.c map_stdio.c	\
		mda.c mfa.c mfa_session.c mta.c mta_session.c parse.y 	\
		queue.c ruleset.c scheduler.c scheduler_backend.c	\
		smtp.c							\
		smtp_session.c smtpd.c ssl.c ssl_privsep.c util.c	\
		queue_backend.c queue_fsqueue.c				\
		user.c user_pwd.c stats.c scheduler_ramqueue.c		\
		map_static.c

SRCS+=		asr.c asr_debug.c asr_utils.c				\
		res_send_async.c  hostaddr_async.c getaddrinfo_async.c	\
		getnameinfo_async.c gethostnamadr_async.c

MAN=		smtpd.8 smtpd.conf.5
BINDIR=		/usr/sbin

LDADD+=		-levent -lutil -lssl -lcrypto -lm
DPADD+=		${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBM}
CFLAGS+=	-g3 -ggdb -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/asr
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
#CFLAGS+=	-Werror # during development phase (breaks some archs)
CFLAGS+=	-DIO_SSL
YFLAGS=

.include <bsd.prog.mk>
