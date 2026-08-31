PROG=getexecpath

WARNINGS=yes

run-regress-${PROG}:	${PROG}
	./${PROG} `cd ${.OBJDIR} && pwd -P`/${PROG}

.include <bsd.regress.mk>
