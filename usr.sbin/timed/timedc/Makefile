#	$OpenBSD: Makefile,v 1.5 2002/05/16 08:25:50 deraadt Exp $
#
#	@(#)Makefile	5.4 (Berkeley) 5/11/93

PROG=	timedc
SRCS=	cmds.c cmdtab.c timedc.c byteorder.c measure.c cksum.c
MAN=	timedc.8
BINOWN=	root
LDSTATIC=-static
BINMODE=4555
.PATH:	${.CURDIR}/../timed

.include "../../Makefile.inc"
.include <bsd.prog.mk>
