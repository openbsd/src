# $OpenBSD: Makefile,v 1.1 2002/02/05 21:47:23 matthieu Exp $

BAR_DIR=${.CURDIR}/../libbar

.if defined(OBJMACHINE)
OBJDIR= obj.${MACHINE}
.else
OBJDIR= obj
.endif

BAR_OBJDIR!=	if [ -d $(BAR_DIR)/$(OBJDIR) ]; then \
			echo $(BAR_DIR)/$(OBJDIR); \
		else \
			echo $(BAR_DIR); \
		fi

PROG=		elfbug
SRCS=		main.c
CPPFLAGS+= 	-I${.CURDIR}/..
LDADD=		-Wl,-rpath,$(BAR_OBJDIR) -L$(BAR_OBJDIR) -lbar

.include <bsd.regress.mk>
