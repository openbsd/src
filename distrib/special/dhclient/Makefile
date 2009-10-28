#	$OpenBSD: Makefile,v 1.10 2009/10/28 07:36:49 deraadt Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../sbin/dhclient

CLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'
CPPFLAGS+= -I${.CURDIR}/../../../sbin/dhclient
SRCS=	dhclient.c clparse.c dispatch.c bpf.c options.c \
	conflex.c errwarn.c inet.c packet.c convert.c tables.c parse.c \
	privsep.c
PROG=	dhclient

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
