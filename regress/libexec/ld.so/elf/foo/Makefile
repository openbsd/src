# $OpenBSD: Makefile,v 1.2 2002/02/10 22:55:13 fgsch Exp $

.include <bsd.obj.mk>

BAR_DIR=${.CURDIR}/../libbar

BAR_OBJDIR!=	if [ -d $(BAR_DIR)/${__objdir} ]; then \
			echo "$(BAR_DIR)/${__objdir}"; \
		else \
			echo "$(BAR_DIR)"; \
		fi

PROG=		elfbug
SRCS=		main.c
CPPFLAGS+= 	-I${.CURDIR}/..
LDADD=		-Wl,-rpath,$(BAR_OBJDIR) -L$(BAR_OBJDIR) -lbar

.include <bsd.regress.mk>
