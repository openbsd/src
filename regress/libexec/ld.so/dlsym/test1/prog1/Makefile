# $OpenBSD: Makefile,v 1.1.1.1 2005/09/14 15:59:37 kurt Exp $

.include <bsd.obj.mk>

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

PROG=		prog1
SRCS=		main.c
CPPFLAGS+= 	-I$(AA_DIR)
LDADD=		-Wl,-E -Wl,-rpath,$(AA_OBJDIR) -L$(AA_OBJDIR) -laa

.include <bsd.regress.mk>
