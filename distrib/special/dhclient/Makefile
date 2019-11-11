#	$OpenBSD: Makefile,v 1.17 2019/11/11 05:39:04 deraadt Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../sbin/dhclient

CPPFLAGS+= -I${.CURDIR}/../../../sbin/dhclient
SRCS=	dhclient.c clparse.c dispatch.c bpf.c options.c \
	conflex.c log.c kroute.c packet.c \
	parse.c privsep.c
PROG=	dhclient
LDADD+=	-lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
