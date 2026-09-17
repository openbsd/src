PROG=getexecpath

WARNINGS=yes

CLEANFILES+=wrap.sh

run-regress-${PROG}:	${PROG}
	./${PROG} `cd ${.OBJDIR} && pwd -P`/${PROG}
	cp ${.CURDIR}/wrap.sh ${.OBJDIR}
	#./wrap.sh `cd ${.OBJDIR} && pwd -P`/${PROG}

.include <bsd.regress.mk>
