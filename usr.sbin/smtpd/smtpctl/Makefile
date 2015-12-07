#	$OpenBSD: Makefile,v 1.41 2015/12/07 12:29:19 sunil Exp $

.PATH:		${.CURDIR}/..

PROG=	smtpctl
BINOWN=	root
BINGRP=	_smtpq

BINMODE?=2555

BINDIR=	/usr/sbin
MAN=	smtpctl.8
MLINKS=	smtpctl.8 mailq.8

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-DNO_IO

SRCS=	enqueue.c
SRCS+=	parser.c
SRCS+=	log.c
SRCS+=	envelope.c
SRCS+=	crypto.c
SRCS+=	queue_backend.c
SRCS+=	queue_fs.c
SRCS+=	smtpctl.c
SRCS+=	util.c
SRCS+=	compress_backend.c
SRCS+=	compress_gzip.c
SRCS+=	to.c
SRCS+=	expand.c
SRCS+=	tree.c
SRCS+=	dict.c
SRCS+=	aliases.c
SRCS+=	limit.c
SRCS+=	makemap.c
SRCS+=	parse.y
SRCS+=	mailaddr.c
SRCS+=	table.c
SRCS+=	table_static.c
SRCS+=	table_db.c
SRCS+=	table_getpwnam.c
SRCS+=	table_proc.c

LDADD+=	-lutil -lz -lcrypto
DPADD+=	${LIBUTIL} ${LIBZ} ${LIBCRYPTO}
.include <bsd.prog.mk>
