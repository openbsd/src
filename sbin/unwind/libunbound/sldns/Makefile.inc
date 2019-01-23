#	$OpenBSD: Makefile.inc,v 1.1 2019/01/23 13:05:27 florian Exp $

.PATH: ${.CURDIR}/libunbound/sldns

SRCS+=	keyraw.c parse.c parseutil.c rrdef.c sbuffer.c str2wire.c wire2str.c
