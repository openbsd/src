# $OpenBSD: Makefile,v 1.2 2025/01/21 19:14:28 anton Exp $

.include <bsd.obj.mk>

PROG=prog

SRCS=prog.c

AA_DIR=${.CURDIR}/../libaa
AA_OBJDIR!=    if [ -d $(AA_DIR)/${__objdir} ]; then \
                        echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

LDADD=
LDADD+=-laa
LDFLAGS=-L$(AA_OBJDIR)
LDFLAGS+= -Wl,-rpath,$(AA_OBJDIR)
NOMAN=
CC=${CXX}

.include <bsd.regress.mk>
