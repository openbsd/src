#	from: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:45:39 deraadt Exp $

DIR=	usd/07.mail
SRCS=	mail0.nr mail1.nr mail2.nr mail3.nr mail4.nr mail5.nr mail6.nr \
	mail7.nr mail8.nr mail9.nr maila.nr
MACROS=	-me

paper.ps: ${SRCS}
	${TBL} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
