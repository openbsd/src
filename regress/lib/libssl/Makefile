
CLEANFILES+= testdsa.key testdsa.pem rsakey.pem rsacert.pem dsa512.pem

install:

regress:
	sh ${.CURDIR}/testenc.sh ${.OBJDIR} ${.CURDIR}
	sh ${.CURDIR}/testdsa.sh ${.OBJDIR} ${.CURDIR}
#	sh ${.CURDIR}/testrsa.sh ${.OBJDIR} ${.CURDIR}

.include <bsd.prog.mk>
