#	$OpenBSD: Makefile,v 1.6 2004/01/23 21:07:38 deraadt Exp $
#
#	@(#)Makefile	5.4 (Berkeley) 5/11/93

PROG=	timedc
SRCS=	cmds.c cmdtab.c timedc.c byteorder.c measure.c cksum.c
MAN=	timedc.8
LDSTATIC=-static
.PATH:	${.CURDIR}/../timed

.include "../../Makefile.inc"
.include <bsd.prog.mk>
