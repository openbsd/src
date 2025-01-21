# $OpenBSD: Makefile,v 1.3 2025/01/21 19:14:27 anton Exp $

.include <bsd.obj.mk>

PROG=prog1

SRCS=prog1.C

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



CPPFLAGS=-I${.CURDIR}/../libab
LDADD=
LDADD+=-lab
LDADD+=-laa
LDFLAGS=-L$(AA_OBJDIR) -L$(AB_OBJDIR)
LDFLAGS+= -Wl,-rpath,$(AA_OBJDIR)  -Wl,-rpath,$(AB_OBJDIR) 
NOMAN=
CC=${CXX}

.include <bsd.regress.mk>
