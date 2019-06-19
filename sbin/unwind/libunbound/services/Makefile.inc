#	$OpenBSD: Makefile.inc,v 1.1 2019/01/23 13:05:27 florian Exp $

.PATH: ${.CURDIR}/libunbound/services

SRCS+=	authzone.c listen_dnsport.c localzone.c mesh.c modstack.c \
	outbound_list.c outside_network.c view.c
