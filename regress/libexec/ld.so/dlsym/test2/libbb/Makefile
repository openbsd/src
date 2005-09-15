# $OpenBSD: Makefile,v 1.1.1.1 2005/09/15 22:16:27 kurt Exp $

.include <bsd.obj.mk>

CC_DIR=${.CURDIR}/../libcc

CC_OBJDIR!=	if [ -d $(CC_DIR)/${__objdir} ]; then \
			echo "$(CC_DIR)/${__objdir}"; \
		else \
			echo "$(CC_DIR)"; \
		fi

LIB=bb
SRCS= bb.c
LDADD=		-Wl,-E -Wl,-rpath,$(CC_OBJDIR) -L$(CC_OBJDIR) -lcc
.include <bsd.lib.mk>
