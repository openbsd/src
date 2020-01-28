#	$OpenBSD: Makefile.inc,v 1.1 2020/01/28 17:17:04 florian Exp $

.PATH: ${.CURDIR}/bin/dig

SRCS+=	dig.c dighost.c host.c nslookup.c
MAN+=	dig.1 host.1 nslookup.1
