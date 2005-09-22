# $OpenBSD: Makefile,v 1.1.1.1 2005/09/22 22:31:27 drahn Exp $

.include <bsd.obj.mk>

PROG=prog1

SRCS=main.c

AAG_DIR=${.CURDIR}/../libaa_g
AAG_OBJDIR!=    if [ -d $(AAG_DIR)/${__objdir} ]; then \
                        echo "$(AAG_DIR)/${__objdir}"; \
		else \
			echo "$(AAG_DIR)"; \
		fi

AAB_DIR=${.CURDIR}/../libaa_b
AAB_OBJDIR!=    if [ -d $(AAB_DIR)/${__objdir} ]; then \
                        echo "$(AAB_DIR)/${__objdir}"; \
		else \
			echo "$(AAB_DIR)"; \
		fi



CFLAGS=-I${.CURDIR}/../libac
LDADD=
LDADD+=-laa
LDFLAGS=-L$(AAG_OBJDIR)
LDFLAGS+= -Wl,-rpath,$(AAB_OBJDIR)
NOMAN=


run-regress-${PROG}: ${PROG}
	./${PROG} 2>&1 | grep "undefined symbol 'libglobal'"
	-./${PROG} >/dev/null 2>&1 || echo "ld.so killed program, good";
.include <bsd.regress.mk>
