#	$OpenBSD: Makefile,v 1.1 2026/09/16 15:48:56 kurt Exp $

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
LDADD+=		-Wl,-z,origin,-rpath,'$$ORIGIN/libaa'
LDADD+=		-L$(AA_OBJDIR) -laa

regress: all

.include <bsd.lib.mk>
