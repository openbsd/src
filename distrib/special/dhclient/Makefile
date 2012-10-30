#	$OpenBSD: Makefile,v 1.11 2012/10/30 18:39:44 krw Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../sbin/dhclient

CLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'
CPPFLAGS+= -I${.CURDIR}/../../../sbin/dhclient
SRCS=	dhclient.c clparse.c dispatch.c bpf.c options.c \
	conflex.c errwarn.c inet.c kroute.c packet.c convert.c \
	tables.c parse.c privsep.c
PROG=	dhclient

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
