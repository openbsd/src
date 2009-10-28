#	$OpenBSD: Makefile,v 1.3 2009/10/28 07:36:49 deraadt Exp $

PROG=	grep
SRCS=	binary.c file.c grep.c mmfile.c queue.c util.c
COPTS+=	-DNOZ
.PATH:  ${.CURDIR}/../../../usr.bin/grep

.include <bsd.prog.mk>
