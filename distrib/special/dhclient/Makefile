#	$OpenBSD: Makefile,v 1.5 2001/07/30 11:25:14 hugh Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../usr.sbin/dhcp/dhclient
.PATH:  ${.CURDIR}/../../../usr.sbin/dhcp/common

COPTS+=-Os
CLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'
CPPFLAGS+= -I${.CURDIR}/../../../usr.sbin/dhcp \
	-I${.CURDIR}/../../../usr.sbin/dhcp/includes \
	-DCLIENT_PATH=${CLIENT_PATH}
SRCS=	dhclient.c clparse.c \
	alloc.c dispatch.c hash.c memory.c print.c bpf.c icmp.c options.c \
	tree.c conflex.c errwarn.c inet.c packet.c socket.c convert.c \
	ethernet.c tables.c parse.c
PROG=	dhclient
MAN=

LDSTATIC?=      -static

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
