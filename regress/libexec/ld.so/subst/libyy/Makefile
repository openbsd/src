#	$OpenBSD: Makefile,v 1.3 2026/09/20 22:19:13 kurt Exp $

.include <bsd.obj.mk>

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

LIB=		yy
SRCS=		yy.c
CPPFLAGS+= 	-I$(AA_DIR)
LDADD+=		-Wl,-rpath,'$$ORIGIN/$${OSNAME}/$$OSREL/$${PLATFORM}'
LDADD+=		-L$(AA_OBJDIR) -laa

regress: all

.include <bsd.lib.mk>
