#	$OpenBSD: Makefile,v 1.58 2013/01/26 09:37:24 gilles Exp $

.PATH:		${.CURDIR}/.. ${.CURDIR}/../../../lib/libc/asr

PROG=		smtpd

SRCS=		aliases.c bounce.c ca.c compress_backend.c config.c	\
		control.c delivery.c dict.c dns.c envelope.c		\
		expand.c forward.c iobuf.c ioev.c lka.c	lka_session.c	\
		log.c mda.c mfa.c mfa_session.c mproc.c			\
		mta.c mta_session.c parse.y queue.c queue_backend.c	\
		ruleset.c scheduler.c scheduler_backend.c		\
		smtp.c smtp_session.c smtpd.c ssl.c ssl_privsep.c	\
		ssl_smtpd.c stat_backend.c table.c to.c tree.c util.c	\
		waitq.c

# backends
SRCS+=		compress_gzip.c
SRCS+=		delivery_filename.c
SRCS+=		delivery_maildir.c
SRCS+=		delivery_mbox.c
SRCS+=		delivery_mda.c
SRCS+=		table_db.c
SRCS+=		table_getpwnam.c
SRCS+=		table_static.c
SRCS+=		table_ldap.c
SRCS+=		table_sqlite.c
SRCS+=		queue_fsqueue.c
SRCS+=		queue_null.c
SRCS+=		queue_ram.c
SRCS+=		scheduler_ramqueue.c
SRCS+=		scheduler_null.c
SRCS+=		stat_ramstat.c

# resolver
SRCS+=		asr.c asr_debug.c asr_utils.c gethostnamadr_async.c	\
		res_send_async.c getaddrinfo_async.c getnameinfo_async.c

# ldap backend
SRCS+=		ber.c aldap.c

MAN=		smtpd.8 smtpd.conf.5
BINDIR=		/usr/sbin

LDADD+=		-levent -lutil -lssl -lcrypto -lm -lz -lsqlite3
DPADD+=		${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBM} ${LIBZ} ${LIBSQLITE3}
CFLAGS+=	-g3 -ggdb -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/asr

CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
#CFLAGS+=	-Werror # during development phase (breaks some archs)
CFLAGS+=	-DIO_SSL
CFLAGS+=	-DQUEUE_PROFILING
CFLAGS+=	-DASR_OPT_THREADSAFE=0
YFLAGS=

.include <bsd.prog.mk>

