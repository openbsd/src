#	$OpenBSD: Makefile,v 1.100 2018/12/11 13:29:52 gilles Exp $

.PATH:		${.CURDIR}/..

PROG=		smtpd

SRCS=	aliases.c
SRCS+=	bounce.c
SRCS+=	ca.c
SRCS+=	cert.c
SRCS+=	compress_backend.c
SRCS+=	config.c
SRCS+=	control.c
SRCS+=	crypto.c
SRCS+=	dict.c
SRCS+=	dns.c
SRCS+=	unpack_dns.c
SRCS+=	envelope.c
SRCS+=	esc.c
SRCS+=	expand.c
SRCS+=	forward.c
SRCS+=	iobuf.c
SRCS+=	ioev.c
SRCS+=	limit.c
SRCS+=	lka.c
SRCS+=	lka_filter.c
SRCS+=	lka_proc.c
SRCS+=	lka_report.c
SRCS+=	lka_session.c
SRCS+=	log.c
SRCS+=	mailaddr.c
SRCS+=	mda.c
SRCS+=	mda_unpriv.c
SRCS+=	mda_variables.c
SRCS+=	mproc.c
SRCS+=	mta.c
SRCS+=	mta_session.c
SRCS+=	parse.y
SRCS+=	pony.c
SRCS+=	queue.c
SRCS+=	queue_backend.c
SRCS+=	report_smtp.c
SRCS+=	resolver.c
SRCS+=	rfc5322.c
SRCS+=	ruleset.c
SRCS+=	runq.c
SRCS+=	scheduler.c
SRCS+=	scheduler_backend.c
SRCS+=	smtp.c
SRCS+=	smtp_session.c
SRCS+=	smtpd.c
SRCS+=	ssl.c
SRCS+=	ssl_smtpd.c
SRCS+=	stat_backend.c
SRCS+=	table.c
SRCS+=	to.c
SRCS+=	tree.c
SRCS+=	util.c
SRCS+=	waitq.c

# backends
SRCS+=		compress_gzip.c

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

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration
#CFLAGS+=	-Werror # during development phase (breaks some archs)
CFLAGS+=	-DIO_SSL
CFLAGS+=	-DQUEUE_PROFILING
YFLAGS=

.include <bsd.prog.mk>
