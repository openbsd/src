#	$OpenBSD: Makefile,v 1.77 2014/12/14 15:26:56 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=		smtpd

SRCS=		aliases.c bounce.c ca.c compress_backend.c config.c		\
		control.c crypto.c delivery.c dict.c dns.c envelope.c esc.c	\
		expand.c forward.c iobuf.c ioev.c limit.c lka.c	lka_session.c	\
		log.c mda.c mproc.c				\
		mta.c mta_session.c parse.y pony.c queue.c queue_backend.c	\
		ruleset.c runq.c scheduler.c scheduler_backend.c		\
		smtp.c smtp_session.c smtpd.c ssl.c ssl_privsep.c		\
		ssl_smtpd.c stat_backend.c table.c to.c tree.c util.c		\
		waitq.c

# RFC parsers
SRCS+=		rfc2822.c

# backends
SRCS+=		compress_gzip.c
SRCS+=		delivery_filename.c
SRCS+=		delivery_maildir.c
SRCS+=		delivery_mbox.c
SRCS+=		delivery_mda.c
SRCS+=		delivery_lmtp.c
SRCS+=		table_db.c
SRCS+=		table_getpwnam.c
SRCS+=		table_proc.c
SRCS+=		table_static.c
SRCS+=		queue_fs.c
SRCS+=		queue_null.c
SRCS+=		queue_proc.c
SRCS+=		queue_ram.c
SRCS+=		scheduler_ramqueue.c
SRCS+=		scheduler_null.c
SRCS+=		scheduler_proc.c
SRCS+=		stat_ramstat.c

MAN=		sendmail.8 smtpd.8 smtpd.conf.5 table.5
BINDIR=		/usr/sbin

LDADD+=		-levent -lutil -lssl -lcrypto -lm -lz
DPADD+=		${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBM} ${LIBZ}
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
#CFLAGS+=	-Werror # during development phase (breaks some archs)
CFLAGS+=	-DIO_SSL
CFLAGS+=	-DQUEUE_PROFILING
YFLAGS=

.include <bsd.prog.mk>
