# $OpenBSD: Makefile,v 1.2 2017/03/18 16:58:22 kettenis Exp $

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

AC_DIR=${.CURDIR}/../libac
AC_OBJDIR!=    if [ -d $(AC_DIR)/${__objdir} ]; then \
                        echo "$(AC_DIR)/${__objdir}"; \
		else \
			echo "$(AC_DIR)"; \
		fi

AD_DIR=${.CURDIR}/../libad
AD_OBJDIR!=    if [ -d $(AD_DIR)/${__objdir} ]; then \
                        echo "$(AD_DIR)/${__objdir}"; \
		else \
			echo "$(AD_DIR)"; \
		fi

AE_DIR=${.CURDIR}/../libae
AE_OBJDIR!=    if [ -d $(AE_DIR)/${__objdir} ]; then \
                        echo "$(AE_DIR)/${__objdir}"; \
		else \
			echo "$(AE_DIR)"; \
		fi

LDADD=-laa
LDFLAGS=  -L$(AA_OBJDIR)
LDFLAGS+= -Wl,--disable-new-dtags
LDFLAGS+= -Wl,-rpath,$(AA_OBJDIR) 
LDFLAGS+= -Wl,-rpath,$(AB_OBJDIR) 
LDFLAGS+= -Wl,-rpath,$(AC_OBJDIR) 
LDFLAGS+= -Wl,-rpath,$(AD_OBJDIR) 
LDFLAGS+= -Wl,-rpath,$(AE_OBJDIR) 
NOMAN=
CC=c++

run-regress-${PROG}: ${PROG}
	[ "`./${PROG}`" = "DBECAPpacebd" ]

.include <bsd.regress.mk>
