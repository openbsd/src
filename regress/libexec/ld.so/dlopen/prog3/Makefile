# $OpenBSD: Makefile,v 1.4 2017/03/18 16:45:32 kettenis Exp $

.include <bsd.obj.mk>

PROG=prog3

SRCS=main.c

AA_DIR=${.CURDIR}/../libaa
AA_OBJDIR!=    if [ -d $(AA_DIR)/${__objdir} ]; then \
                        echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

AC_DIR=${.CURDIR}/../libac
AC_OBJDIR!=    if [ -d $(AC_DIR)/${__objdir} ]; then \
                        echo "$(AC_DIR)/${__objdir}"; \
		else \
			echo "$(AC_DIR)"; \
		fi



LDADD=
#LDADD+=-laa
#LDADD+=-lac
LDFLAGS=-L$(AA_OBJDIR) -L$(AC_OBJDIR)
LDFLAGS+= -Wl,-disable-new-dtags
LDFLAGS+= -Wl,-rpath,$(AA_OBJDIR)  -Wl,-rpath,$(AC_OBJDIR) 
NOMAN=
CC=c++

.include <bsd.regress.mk>
