# $OpenBSD: Makefile,v 1.1 2003/02/01 19:56:17 drahn Exp $

.include <bsd.obj.mk>

PROG=prog2

SRCS=prog2.C

AA_DIR=${.CURDIR}/../libaa
AA_OBJDIR!=    if [ -d $(AA_DIR)/${__objdir} ]; then \
                        echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

AB_DIR=${.CURDIR}/../libab
AB_OBJDIR!=    if [ -d $(AB_DIR)/${__objdir} ]; then \
                        echo "$(AB_DIR)/${__objdir}"; \
		else \
			echo "$(AB_DIR)"; \
		fi



CFLAGS=-I${.CURDIR}/../libab
LDADD=
LDADD+=-laa
LDADD+=-lab
LDFLAGS=-L$(AA_OBJDIR) -L$(AB_OBJDIR)
LDFLAGS+= -Wl,-rpath,$(AA_OBJDIR)  -Wl,-rpath,$(AB_OBJDIR) 
NOMAN=
CC=c++

.include <bsd.regress.mk>
