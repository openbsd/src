#	$OpenBSD: Makefile,v 1.7 2004/11/10 22:07:45 drahn Exp $
#
#	@(#)Makefile	5.4 (Berkeley) 5/11/93

PROG=	timedc
SRCS=	cmds.c cmdtab.c timedc.c byteorder.c measure.c cksum.c
MAN=	timedc.8
LDSTATIC=${STATIC}
.PATH:	${.CURDIR}/../timed

.include "../../Makefile.inc"
.include <bsd.prog.mk>
