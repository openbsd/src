#	$NetBSD: Makefile,v 1.2 1995/06/27 18:22:51 phil Exp $
#	Makefile for man4/man4.pc532

MAN=	lpt.4 plip.4
MANSUBDIR=/pc532

plip.cat4:
	tbl ${.IMPSRC} | nroff -mandoc > ${.TARGET}

.include <bsd.prog.mk>
