#	$OpenBSD: Makefile,v 1.2 2026/09/17 16:53:28 kurt Exp $

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
LDADD+=		-Wl,-z,origin,-rpath,'$$ORIGIN/$${OSNAME}/$$OSREL/$${PLATFORM}'
LDADD+=		-L$(AA_OBJDIR) -laa

regress: all

.include <bsd.lib.mk>
