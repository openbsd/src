#	$NetBSD: Makefile,v 1.3 1996/01/25 07:19:54 phil Exp $
#	Makefile for man4/man4.pc532

MAN=	lpt.4 mem.4 plip.4
MANSUBDIR=/pc532

plip.cat4:
	tbl ${.IMPSRC} | nroff -mandoc > ${.TARGET}

.include <bsd.prog.mk>
