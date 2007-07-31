# $OpenBSD: Makefile,v 1.1.1.1 2007/07/31 20:31:42 kurt Exp $

.include <bsd.obj.mk>

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

PROG=		test2

SRCS=		test2.c

LDADD+=		-laa
LDADD+=		-lab
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,-rpath,$(AA_OBJDIR)
LDFLAGS+=	-L$(AA_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(AB_OBJDIR)
LDFLAGS+=	-L$(AB_OBJDIR)

NOMAN=

install:

.include <bsd.regress.mk>
