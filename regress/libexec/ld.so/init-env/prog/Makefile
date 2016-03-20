# $OpenBSD: Makefile,v 1.1 2016/03/20 05:13:22 guenther Exp $

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
CC=c++

.include <bsd.regress.mk>
