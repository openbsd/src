# $OpenBSD: Makefile,v 1.3 2006/05/31 02:43:15 ckuethe Exp $

.include <bsd.own.mk>

SRCS=	bootp.c confpars.c db.c dhcp.c dhcpd.c bpf.c packet.c errwarn.c \
	dispatch.c print.c memory.c options.c inet.c conflex.c parse.c \
	alloc.c tables.c tree.c hash.c convert.c icmp.c pfutils.c
PROG=	dhcpd
MAN=	dhcpd.8 dhcpd.conf.5 dhcpd.leases.5 dhcp-options.5

CFLAGS+=-Wall

.include <bsd.prog.mk>
