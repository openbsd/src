#	$OpenBSD: Makefile,v 1.7 2004/05/04 13:10:11 henning Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../sbin/dhclient

COPTS+=-Os
CLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'
CPPFLAGS+= -I${.CURDIR}/../../../sbin/dhclient
SRCS=	dhclient.c clparse.c alloc.c dispatch.c hash.c bpf.c options.c \
	tree.c conflex.c errwarn.c inet.c packet.c convert.c tables.c parse.c \
	privsep.c
PROG=	dhclient
MAN=

LDSTATIC?=      -static

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
