#	$OpenBSD: Makefile,v 1.9 2005/07/18 02:33:50 deraadt Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../sbin/dhclient

COPTS+=-Os
CLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'
CPPFLAGS+= -I${.CURDIR}/../../../sbin/dhclient
SRCS=	dhclient.c clparse.c dispatch.c bpf.c options.c \
	conflex.c errwarn.c inet.c packet.c convert.c tables.c parse.c \
	privsep.c
PROG=	dhclient
MAN=

LDSTATIC?=      -static

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
