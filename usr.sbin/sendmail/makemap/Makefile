#	$OpenBSD: Makefile,v 1.3 1997/07/15 07:23:53 downsj Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/7/93

.PATH:	${.CURDIR}/../src

PROG=	makemap
MAN=	makemap.8
CFLAGS+=-I${.CURDIR}/../src -DNEWDB -DNOT_SENDMAIL

SRCS=	makemap.c safefile.c

.include "../../Makefile.inc"
.include <bsd.prog.mk>
