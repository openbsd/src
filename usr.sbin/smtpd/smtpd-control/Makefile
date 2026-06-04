#	$OpenBSD: Makefile,v 1.1 2026/06/04 05:43:00 deraadt Exp $

.PATH:		${.CURDIR}/..

PROG=		smtpd-control
RELINK=		"./${PROG} -V > /dev/null"
BINDIR=		/usr/libexec
NOMAN=

SRCS=	aliases.c bounce.c ca.c compress_backend.c config.c control.c \
	crypto.c dict.c dns.c unpack_dns.c envelope.c esc.c expand.c \
	forward.c iobuf.c ioev.c limit.c lka.c lka_filter.c \
	lka_session.c log.c mailaddr.c mda.c mda_mbox.c mda_unpriv.c \
	mda_variables.c mproc.c mta.c mta_session.c parse.y \
	dispatcher.c proxy.c queue.c queue_backend.c report_smtp.c \
	resolver.c rfc5322.c ruleset.c runq.c scheduler.c \
	scheduler_backend.c smtp.c smtp_session.c smtpd.c srs.c ssl.c \
	stat_backend.c table.c to.c tree.c util.c waitq.c \
	compress_gzip.c table_db.c table_getpwnam.c table_proc.c \
	table_static.c queue_fs.c queue_null.c queue_proc.c \
	queue_ram.c scheduler_ramqueue.c scheduler_null.c \
	scheduler_proc.c stat_ramstat.c

.include "../smtpd/Makefile.inc"
.include <bsd.prog.mk>
