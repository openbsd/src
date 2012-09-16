#	$OpenBSD: Makefile,v 1.54 2012/09/16 10:48:28 eric Exp $

.PATH:		${.CURDIR}/.. ${.CURDIR}/../../../lib/libc/asr

PROG=		smtpd

SRCS=		aliases.c auth.c bounce.c compress_backend.c config.c	\
		control.c delivery.c dns.c envelope.c			\
		expand.c forward.c iobuf.c ioev.c lka.c lka_session.c	\
		log.c map.c mda.c mfa.c mfa_session.c mta.c		\
		mta_session.c parse.y queue.c queue_backend.c ruleset.c \
		scheduler.c scheduler_backend.c smtp.c smtp_session.c	\
		smtpd.c	ssl.c ssl_privsep.c stat_backend.c tree.c	\
		user.c util.c

# backends
SRCS+=		auth_bsd.c
SRCS+=		auth_pwd.c
SRCS+=		compress_gzip.c
SRCS+=		delivery_filename.c
SRCS+=		delivery_maildir.c
SRCS+=		delivery_mbox.c
SRCS+=		delivery_mda.c
SRCS+=		map_db.c
SRCS+=		map_static.c
SRCS+=		map_stdio.c
SRCS+=		queue_fsqueue.c
SRCS+=		scheduler_ramqueue.c
SRCS+=		stat_ramstat.c
#SRCS+=		stat_sqlite.c
SRCS+=		user_pwd.c

# resolver
SRCS+=		asr.c asr_debug.c asr_utils.c gethostnamadr_async.c	\
		res_send_async.c getaddrinfo_async.c getnameinfo_async.c

MAN=		smtpd.8 smtpd.conf.5
BINDIR=		/usr/sbin

LDADD+=		-levent -lutil -lssl -lcrypto -lm -lz
DPADD+=		${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBM} ${LIBZ}
CFLAGS+=	-g3 -ggdb -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/asr
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
#CFLAGS+=	-Werror # during development phase (breaks some archs)
CFLAGS+=	-DIO_SSL
CFLAGS+=	-DASR_OPT_THREADSAFE=0
YFLAGS=

.include <bsd.prog.mk>
