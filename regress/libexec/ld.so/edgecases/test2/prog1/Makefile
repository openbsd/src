#	$OpenBSD: Makefile,v 1.1.1.1 2005/09/23 18:58:07 kurt Exp $

.include <bsd.obj.mk>

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

PROG=		prog1
SRCS=		main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)

run-regress-${PROG}: ${PROG}
	./${PROG}
	! ./${PROG} 2>&1 | grep unresolvedWeakSymbol

.include <bsd.regress.mk>
