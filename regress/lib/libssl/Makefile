
CLEANFILES+= testdsa.key testdsa.pem rsakey.pem rsacert.pem dsa512.pem

install:

regress:
	sh ${.CURDIR}/testenc.sh
	sh ${.CURDIR}/testdsa.sh
#	sh ${.CURDIR}/testrsa.sh

.include <bsd.prog.mk>
