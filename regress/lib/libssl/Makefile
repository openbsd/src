#	$OpenBSD: Makefile,v 1.5 2002/01/02 15:17:34 art Exp $

CLEANFILES+= testdsa.key testdsa.pem rsakey.pem rsacert.pem dsa512.pem

REGRESSTARGETS=ssl-enc ssl-dsa ssl-rsa

ssl-enc:
	sh ${.CURDIR}/testenc.sh ${.OBJDIR} ${.CURDIR}
ssl-dsa:
	sh ${.CURDIR}/testdsa.sh ${.OBJDIR} ${.CURDIR}
ssl-rsa:
	sh ${.CURDIR}/testrsa.sh ${.OBJDIR} ${.CURDIR}

.include <bsd.regress.mk>
